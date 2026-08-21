// *************************************************************************************************
// Bosch BMA250 acceleration sensor driver for eZ430-Chronos white PCB.
// GCC port: replaces the legacy VTI CMA3000 protocol used by early Chronos revisions.
// fixed46: explicit CC430 port mapping + coherent 10-bit burst reads.
// *************************************************************************************************

#include "project.h"
#include "vti_as.h"
#include "timer.h"

#define BMA250_REG_CHIP_ID      0x00u
#define BMA250_REG_X_LSB        0x02u
#define BMA250_REG_RANGE        0x0Fu
#define BMA250_REG_BW           0x10u
#define BMA250_REG_PMU          0x11u
#define BMA250_REG_SOFTRESET    0x14u

#define BMA250_CHIP_ID          0x03u
#define BMA250_SOFTRESET_CMD    0xB6u
#define BMA250_RANGE_2G         0x03u
#define BMA250_BW_62_5HZ        0x0Bu

#define AS_BR_DIVIDER           30u

u8 as_ok;

static void as_delay_ticks(u16 ticks)
{
    u16 start;
    volatile u32 guard = 250000UL;

    if ((TA0CTL & (BIT4 | BIT5)) == 0)
    {
        while (guard--) __no_operation();
        return;
    }

    start = TA0R;
    while (((u16)(TA0R - start) < ticks) && guard--)
        __no_operation();
}

static void as_map_spi_pins(void)
{
    /* P1.5/P1.6/P1.7 are port-mapped pins on CC430F6137.  P1SEL alone is
       not enough: explicitly route UCA0 SOMI/SIMO/CLK to the Chronos BMA250
       wiring.  This is the white-PCB schematic connection. */
    PMAPPWD = 0x02D52u;
#ifdef PMAPRECFG
    PMAPCTL |= PMAPRECFG;
#endif
    P1MAP5 = PM_UCA0SOMI;   /* MCU input  <- BMA250 SDO */
    P1MAP6 = PM_UCA0SIMO;   /* MCU output -> BMA250 SDI */
    P1MAP7 = PM_UCA0CLK;    /* SPI clock */
    PMAPPWD = 0;
}

static u8 as_spi_transfer(u8 value)
{
    u16 timeout = SPI_TIMEOUT;

    while (!(AS_IRQ_REG & AS_TX_IFG) && (--timeout > 0));
    if (timeout == 0)
    {
        as_ok = 0;
        return 0;
    }

    AS_TX_BUFFER = value;

    timeout = SPI_TIMEOUT;
    while (!(AS_IRQ_REG & AS_RX_IFG) && (--timeout > 0));
    if (timeout == 0)
    {
        as_ok = 0;
        return 0;
    }

    return AS_RX_BUFFER;
}

static void as_read_burst(u8 address, u8 *dst, u8 count)
{
    u8 i;

    if (!dst || !count || !as_ok)
        return;

    AS_CSN_OUT &= ~AS_CSN_PIN;
    (void)as_spi_transfer((u8)(0x80u | (address & 0x7Fu)));
    for (i = 0; i < count; ++i)
        dst[i] = as_spi_transfer(0x00u);
    AS_CSN_OUT |= AS_CSN_PIN;
}

static s16 bma250_decode_axis(u8 lsb, u8 msb)
{
    /* BMA250 is a signed 10-bit value. Bits 9..2 live in the MSB register;
       bits 1..0 live in bits 7..6 of the LSB register. */
    s16 raw = (s16)(((u16)msb << 2) | ((u16)lsb >> 6));

    if (raw & 0x0200)
        raw -= 0x0400;

    return raw;
}

static u8 bma250_to_legacy8(s16 raw)
{
    /* Existing Chronos UI expects a signed 8-bit two's-complement sample.
       Divide the BMA250 10-bit value by four, preserving sign. */
    s16 v;

    if (raw >= 0)
        v = raw >> 2;
    else
        v = -(((-raw) + 3) >> 2);

    if (v > 127)  v = 127;
    if (v < -128) v = -128;
    return (u8)(s8)v;
}

void as_init(void)
{
    AS_PWR_OUT &= ~AS_PWR_PIN;
    AS_PWR_DIR |= AS_PWR_PIN;

    AS_CSN_OUT |= AS_CSN_PIN;
    AS_CSN_DIR |= AS_CSN_PIN;

    AS_INT_DIR &= ~AS_INT_PIN;
    AS_INT_IE  &= ~AS_INT_PIN;
    AS_INT_IFG &= ~AS_INT_PIN;

    AS_SPI_SEL &= ~(AS_SDO_PIN | AS_SDI_PIN | AS_SCK_PIN);
    AS_SPI_DIR |=  (AS_SDO_PIN | AS_SCK_PIN);
    AS_SPI_DIR &= ~AS_SDI_PIN;

    as_ok = 1;
}

void as_start(void)
{
    u8 id;

    as_ok = 1;

    as_map_spi_pins();

    /* BMA250 supports SPI mode 0 and mode 3.  For MSP430 USCI, UCCKPH=1
       with UCCKPL=0 corresponds to mode 0 (capture first edge). */
    AS_SPI_CTL1 |= UCSWRST;
    AS_SPI_CTL0 = UCSYNC | UCMST | UCMSB | UCCKPH;
    AS_SPI_CTL1 = UCSWRST | UCSSEL1;          /* SMCLK */
    AS_SPI_BR0  = AS_BR_DIVIDER;
    AS_SPI_BR1  = 0;

    AS_SPI_DIR &= ~AS_SDI_PIN;
    AS_SPI_DIR |= AS_SDO_PIN | AS_SCK_PIN;
    AS_SPI_SEL |= AS_SDO_PIN | AS_SDI_PIN | AS_SCK_PIN;

    AS_CSN_OUT |= AS_CSN_PIN;
    AS_PWR_OUT |= AS_PWR_PIN;

    as_delay_ticks(CONV_MS_TO_TICKS(10));

    AS_SPI_CTL1 &= ~UCSWRST;
    as_delay_ticks(CONV_MS_TO_TICKS(2));

    id = as_read_register(BMA250_REG_CHIP_ID);
    if (id != BMA250_CHIP_ID)
    {
        as_ok = 0;
        return;
    }

    as_write_register(BMA250_REG_SOFTRESET, BMA250_SOFTRESET_CMD);
    as_delay_ticks(CONV_MS_TO_TICKS(3));

    as_write_register(BMA250_REG_RANGE, BMA250_RANGE_2G);
    as_write_register(BMA250_REG_BW, BMA250_BW_62_5HZ);
    as_write_register(BMA250_REG_PMU, 0x00u);  /* normal mode */
    as_delay_ticks(CONV_MS_TO_TICKS(5));

    AS_INT_IE &= ~AS_INT_PIN;
    AS_INT_IFG &= ~AS_INT_PIN;
}

void as_stop(void)
{
    AS_INT_IE &= ~AS_INT_PIN;

    if (AS_PWR_OUT & AS_PWR_PIN)
    {
        if (as_ok)
            as_write_register(BMA250_REG_PMU, 0x80u); /* suspend */

        AS_SPI_CTL1 |= UCSWRST;
        AS_SPI_SEL &= ~(AS_SDO_PIN | AS_SDI_PIN | AS_SCK_PIN);
        AS_PWR_OUT &= ~AS_PWR_PIN;
        AS_CSN_OUT |= AS_CSN_PIN;
    }
}

u8 as_read_register(u8 address)
{
    u8 result;

    if (!as_ok) return 0;

    AS_CSN_OUT &= ~AS_CSN_PIN;
    (void)as_spi_transfer((u8)(0x80u | (address & 0x7Fu)));
    result = as_spi_transfer(0x00u);
    AS_CSN_OUT |= AS_CSN_PIN;

    return result;
}

u8 as_write_register(u8 address, u8 data)
{
    u8 result;

    if (!as_ok) return 0;

    AS_CSN_OUT &= ~AS_CSN_PIN;
    (void)as_spi_transfer((u8)(address & 0x7Fu));
    result = as_spi_transfer(data);
    AS_CSN_OUT |= AS_CSN_PIN;

    return result;
}

void as_get_data(u8 *data)
{
    u8 b[6];
    s16 x, y, z;

    if (!data || !as_ok || ((AS_PWR_OUT & AS_PWR_PIN) == 0))
        return;

    /* One burst keeps all six axis bytes coherent and follows Bosch's
       recommended LSB-before-MSB access order. */
    as_read_burst(BMA250_REG_X_LSB, b, sizeof(b));
    if (!as_ok)
        return;

    x = bma250_decode_axis(b[0], b[1]);
    y = bma250_decode_axis(b[2], b[3]);
    z = bma250_decode_axis(b[4], b[5]);

    data[0] = bma250_to_legacy8(x);
    data[1] = bma250_to_legacy8(y);
    data[2] = bma250_to_legacy8(z);
}
