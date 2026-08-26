// *************************************************************************************************
// Bosch BMP085 pressure sensor driver for eZ430-Chronos white-PCB revision
// MSP430-GCC port, fixed39
// *************************************************************************************************

#include "project.h"
#include "vti_ps.h"
#include "timer.h"

#define BMP085_ADDR       0x77u
#define BMP085_CHIP_ID    0x55u
#define BMP085_REG_ID     0xD0u
#define BMP085_REG_CTRL   0xF4u
#define BMP085_REG_DATA   0xF6u
#define BMP085_CMD_TEMP   0x2Eu
#define BMP085_CMD_PRESS  0x34u
#define BMP085_OSS        0u

#define SCP1000_ADDR      0x11u

/* Existing altitude conversion tables retained for compatibility. */
const s16 h0[17] = { -153, 0, 111, 540, 989, 1457, 1949, 2466, 3012, 3591, 4206, 4865, 5574, 6344, 7185, 8117, 9164 };
const u16 p0[17] = { 1031, 1013, 1000, 950, 900, 850, 800, 750, 700, 650, 600, 550, 500, 450, 400, 350, 300 };
float p[17];

u8 ps_ok;
u8 ps_sensor_type;

static s16 ac1, ac2, ac3, b1, b2, mb, mc, md;
static u16 ac4, ac5, ac6;
static s32 bmp_b5;

static void twi_delay(void)
{
    __no_operation();
    __no_operation();
}

static void bmp_sda_out(void) { PS_TWI_DIR |= PS_SDA_PIN; }
static void bmp_sda_in(void)  { PS_TWI_OUT |= PS_SDA_PIN; PS_TWI_DIR &= ~PS_SDA_PIN; }
static void bmp_sda_hi(void)  { PS_TWI_OUT |= PS_SDA_PIN; }
static void bmp_sda_lo(void)  { PS_TWI_OUT &= ~PS_SDA_PIN; }
static void bmp_scl_hi(void)  { PS_TWI_OUT |= PS_SCL_PIN; }
static void bmp_scl_lo(void)  { PS_TWI_OUT &= ~PS_SCL_PIN; }

/* fixed41: bounded delay based directly on the free-running ACLK timer.
   This does not depend on CCR4 interrupts, so BMP085 probing cannot deadlock
   the foreground loop if the auxiliary timer ISR is unavailable. */
static void bmp_delay_ticks(u16 ticks)
{
    u16 start = TA0R;
    u32 guard = 200000UL;
    while (((u16)(TA0R - start) < ticks) && guard--)
        __no_operation();
}

static void bmp_start(void)
{
    bmp_sda_out(); bmp_sda_hi(); bmp_scl_hi(); twi_delay();
    bmp_sda_lo(); twi_delay(); bmp_scl_lo(); twi_delay();
}

static void bmp_restart(void)
{
    bmp_sda_out(); bmp_sda_hi(); bmp_scl_lo(); twi_delay();
    bmp_scl_hi(); twi_delay(); bmp_sda_lo(); twi_delay(); bmp_scl_lo();
}

static void bmp_stop(void)
{
    bmp_sda_out(); bmp_sda_lo(); bmp_scl_lo(); twi_delay();
    bmp_scl_hi(); twi_delay(); bmp_sda_hi(); twi_delay();
}

static u8 bmp_write_byte(u8 data)
{
    u8 i;
    bmp_sda_out();
    for (i=0; i<8; ++i)
    {
        bmp_scl_lo();
        if (data & 0x80u) bmp_sda_hi(); else bmp_sda_lo();
        twi_delay(); bmp_scl_hi(); twi_delay();
        data <<= 1;
    }
    bmp_scl_lo();
    bmp_sda_in(); twi_delay(); bmp_scl_hi(); twi_delay();
    i = ((PS_TWI_IN & PS_SDA_PIN) == 0); /* ACK */
    bmp_scl_lo(); bmp_sda_out();
    return i;
}

static u8 bmp_read_byte(u8 ack)
{
    u8 i, data=0;
    bmp_sda_in();
    for (i=0; i<8; ++i)
    {
        data <<= 1;
        bmp_scl_lo(); twi_delay(); bmp_scl_hi(); twi_delay();
        if (PS_TWI_IN & PS_SDA_PIN) data |= 1u;
    }
    bmp_scl_lo(); bmp_sda_out();
    if (ack) bmp_sda_lo(); else bmp_sda_hi();
    twi_delay(); bmp_scl_hi(); twi_delay(); bmp_scl_lo(); bmp_sda_hi();
    return data;
}

static u8 bmp_write_reg(u8 reg, u8 value)
{
    u8 ok;
    bmp_start();
    ok = bmp_write_byte((u8)(BMP085_ADDR << 1));
    if (ok) ok = bmp_write_byte(reg);
    if (ok) ok = bmp_write_byte(value);
    bmp_stop();
    return ok;
}

static u8 bmp_read_regs(u8 reg, u8 *dst, u8 count)
{
    u8 ok, i;
    bmp_start();
    ok = bmp_write_byte((u8)(BMP085_ADDR << 1));
    if (ok) ok = bmp_write_byte(reg);
    if (!ok) { bmp_stop(); return 0; }
    bmp_restart();
    ok = bmp_write_byte((u8)((BMP085_ADDR << 1) | 1u));
    if (!ok) { bmp_stop(); return 0; }
    for (i=0; i<count; ++i) dst[i] = bmp_read_byte((u8)(i + 1u < count));
    bmp_stop();
    return 1;
}

static u8 bmp_read8(u8 reg)
{
    u8 v=0;
    bmp_read_regs(reg, &v, 1);
    return v;
}

static u16 bmp_read16(u8 reg)
{
    u8 b[2]={0,0};
    bmp_read_regs(reg,b,2);
    return (u16)(((u16)b[0] << 8) | b[1]);
}

static u8 scp_write_reg(u8 reg, u8 value)
{
    u8 ok;
    bmp_start();
    ok = bmp_write_byte((u8)(SCP1000_ADDR << 1));
    if (ok) ok = bmp_write_byte(reg);
    /* The SCP1000 does not ACK the final data byte.  The original TI/VTI
       driver deliberately ignores that last ACK, so only the device-address
       and register-address ACKs determine whether this transaction started
       successfully. */
    if (ok) (void)bmp_write_byte(value);
    bmp_stop();
    return ok;
}

static u8 scp_read_regs(u8 reg, u8 *dst, u8 count)
{
    u8 ok, i;
    bmp_start();
    ok = bmp_write_byte((u8)(SCP1000_ADDR << 1));
    if (ok) ok = bmp_write_byte(reg);
    if (!ok) { bmp_stop(); return 0; }
    bmp_restart();
    ok = bmp_write_byte((u8)((SCP1000_ADDR << 1) | 1u));
    if (!ok) { bmp_stop(); return 0; }
    for (i = 0; i < count; ++i)
        dst[i] = bmp_read_byte((u8)(i + 1u < count));
    bmp_stop();
    return 1;
}

static u8 scp_read8(u8 reg)
{
    u8 value = 0;
    scp_read_regs(reg, &value, 1);
    return value;
}

static u16 scp_read16(u8 reg)
{
    u8 value[2] = { 0, 0 };
    scp_read_regs(reg, value, 2);
    return (u16)(((u16)value[0] << 8) | value[1]);
}

/* Compatibility exports retained for older code/debug helpers. */
u8 ps_write_register(u8 address, u8 data)
{
    return (ps_sensor_type == PS_SENSOR_SCP1000) ?
           scp_write_reg(address, data) : bmp_write_reg(address, data);
}
u16 ps_read_register(u8 address, u8 mode)
{
    if (ps_sensor_type == PS_SENSOR_SCP1000)
        return (mode == PS_TWI_16BIT_ACCESS) ? scp_read16(address) : scp_read8(address);
    return (mode == PS_TWI_16BIT_ACCESS) ? bmp_read16(address) : bmp_read8(address);
}

void ps_init(void)
{
    PS_TWI_OUT |= PS_SCL_PIN | PS_SDA_PIN;
    PS_TWI_DIR |= PS_SCL_PIN | PS_SDA_PIN;
    PS_TWI_REN &= ~(PS_SCL_PIN | PS_SDA_PIN);
    PS_INT_IE &= ~PS_INT_PIN; /* BMP085 is sampled synchronously; EOC IRQ not required. */
    PS_INT_IFG &= ~PS_INT_PIN;
    ps_ok = 0;
    ps_sensor_type = PS_SENSOR_NONE;

    bmp_delay_ticks(CONV_MS_TO_TICKS(10));

    /* Newer white-PCB watch: Bosch BMP085 at address 0x77. */
    if (bmp_read8(BMP085_REG_ID) == BMP085_CHIP_ID)
    {
        ac1 = (s16)bmp_read16(0xAA); ac2 = (s16)bmp_read16(0xAC); ac3 = (s16)bmp_read16(0xAE);
        ac4 = bmp_read16(0xB0); ac5 = bmp_read16(0xB2); ac6 = bmp_read16(0xB4);
        b1  = (s16)bmp_read16(0xB6); b2  = (s16)bmp_read16(0xB8); mb  = (s16)bmp_read16(0xBA);
        mc  = (s16)bmp_read16(0xBC); md  = (s16)bmp_read16(0xBE);

        if ((ac4 != 0u) && (ac5 != 0u) && (ac6 != 0u) && (ac4 != 0xFFFFu))
        {
            ps_sensor_type = PS_SENSOR_BMP085;
            ps_ok = 1;
            return;
        }
    }

    /* Older watch: VTI SCP1000-D0x at address 0x11. Reset it, then use
       STATUS and the EEPROM checksum register as the identity check. */
    if (!scp_write_reg(0x06u, 0x01u)) return;
    bmp_delay_ticks(CONV_MS_TO_TICKS(100));
    if (((scp_read8(0x07u) & BIT0) == 0u) && (scp_read8(0x7Fu) == 0x01u))
    {
        ps_sensor_type = PS_SENSOR_SCP1000;
        ps_ok = 1;
    }
}

void ps_start(void)
{
    if (ps_sensor_type == PS_SENSOR_SCP1000)
    {
        PS_INT_DIR &= ~PS_INT_PIN;
        PS_INT_IES &= ~PS_INT_PIN;
        PS_INT_IFG &= ~PS_INT_PIN;
        scp_write_reg(0x03u, 0x0Bu); /* ultra-low-power continuous mode */
        bmp_delay_ticks(CONV_MS_TO_TICKS(120)); /* first conversion */
        PS_INT_IE |= PS_INT_PIN;
    }
}

void ps_stop(void)
{
    if (ps_sensor_type == PS_SENSOR_SCP1000)
    {
        PS_INT_IE &= ~PS_INT_PIN;
        PS_INT_IFG &= ~PS_INT_PIN;
        scp_write_reg(0x03u, 0x00u); /* standby */
    }
}

u16 ps_get_temp(void)
{
    s32 x1, x2, t;
    u16 ut;
    if (!ps_ok) return 2732u;
    if (ps_sensor_type == PS_SENSOR_SCP1000)
    {
        u16 data = scp_read16(0x81u);
        u16 magnitude;
        if (data & BIT(13))
        {
            magnitude = (u16)((~(data | 0xC000u) + 1u) / 2u);
            return (u16)(2732u - magnitude);
        }
        return (u16)(2732u + data / 2u);
    }
    if (!bmp_write_reg(BMP085_REG_CTRL, BMP085_CMD_TEMP)) return 2732u;
    bmp_delay_ticks(CONV_MS_TO_TICKS(5));
    ut = bmp_read16(BMP085_REG_DATA);

    x1 = (((s32)ut - (s32)ac6) * (s32)ac5) >> 15;
    if ((x1 + md) == 0) return 2732u;
    x2 = ((s32)mc << 11) / (x1 + md);
    bmp_b5 = x1 + x2;
    t = (bmp_b5 + 8) >> 4; /* 0.1 deg C */
    return (u16)(t + 2732); /* 0.1 K, API used by altitude.c */
}

u32 ps_get_pa(void)
{
    u8 raw[3];
    s32 b6, x1, x2, x3, b3, pcomp;
    u32 b4, b7, up;
    if (!ps_ok) return 0;
    if (ps_sensor_type == PS_SENSOR_SCP1000)
    {
        u32 data = (u32)(scp_read8(0x7Fu) & 0x07u) << 16;
        data |= scp_read16(0x80u);
        return data >> 2;
    }

    if (!bmp_write_reg(BMP085_REG_CTRL, (u8)(BMP085_CMD_PRESS + (BMP085_OSS << 6)))) return 0;
    bmp_delay_ticks(CONV_MS_TO_TICKS(5));
    if (!bmp_read_regs(BMP085_REG_DATA, raw, 3)) return 0;
    up = ((((u32)raw[0] << 16) | ((u32)raw[1] << 8) | raw[2]) >> (8 - BMP085_OSS));

    b6 = bmp_b5 - 4000;
    x1 = ((s32)b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = ((s32)ac2 * b6) >> 11;
    x3 = x1 + x2;
    b3 = (((((s32)ac1 * 4 + x3) << BMP085_OSS) + 2) >> 2);
    x1 = ((s32)ac3 * b6) >> 13;
    x2 = ((s32)b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    b4 = ((u32)ac4 * (u32)(x3 + 32768)) >> 15;
    if (b4 == 0) return 0;
    b7 = (up - (u32)b3) * (u32)(50000UL >> BMP085_OSS);
    if (b7 < 0x80000000UL) pcomp = (s32)((b7 << 1) / b4);
    else                    pcomp = (s32)((b7 / b4) << 1);
    x1 = (pcomp >> 8) * (pcomp >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * pcomp) >> 16;
    pcomp += (x1 + x2 + 3791) >> 4;
    return (pcomp > 0) ? (u32)pcomp : 0u;
}

// *************************************************************************************************
// @fn          init_pressure_table
// @brief       Init pressure table with constants
// @param       u32		p 		Pressure (Pa)
// @return      u16				Altitude (m)
// *************************************************************************************************
void init_pressure_table(void)
{
	u8 i;

	for (i=0; i<17; i++) p[i] = p0[i];
}


// *************************************************************************************************
// @fn          update_pressure_table
// @brief       Calculate pressure table for reference altitude.
//				Implemented straight from VTI reference code.
// @param       s16		href	Reference height
//				u32		p_meas	Pressure (Pa)
//				u16		t_meas	Temperature (10*�K)
// @return     	none
// *************************************************************************************************
void update_pressure_table(s16 href, u32 p_meas, u16 t_meas)
{
	const float Invt00 = 0.003470415f;
	const float coefp  = 0.00006f;
	volatile float p_fact; 
	volatile float p_noll;
	volatile float hnoll;
	volatile float h_low = 0;
	volatile float t0;
	u8 i;
	
	// Typecast arguments
	volatile float fl_href 		= href;
	volatile float fl_p_meas 	= (float)p_meas/100;	// Convert from Pa to hPa
	volatile float fl_t_meas	= (float)t_meas/10;		// Convert from 10�K to 1�K

	t0 = fl_t_meas + (0.0065f*fl_href);
	
	hnoll  = fl_href/(t0*Invt00);
	
	for (i=0; i<=15; i++)
	{
		if (h0[i] > hnoll) break;
		h_low = h0[i];	
	}
	
	p_noll = (float)(hnoll - h_low)*(1 - (hnoll - (float)h0[i])* coefp)*((float)p0[i] - (float)p0[i-1])/((float)h0[i] - h_low) + (float)p0[i-1];

	// Calculate multiplicator
	p_fact = fl_p_meas/p_noll;
	
	// Apply correction factor to pressure table
	for (i=0; i<=16; i++)
	{
		p[i] = p0[i]*p_fact;	
	}
}


// *************************************************************************************************
// @fn          conv_pa_to_meter
// @brief       Convert pressure (Pa) to altitude (m) using a conversion table
//				Implemented straight from VTI reference code.
// @param       u32		p_meas	Pressure (Pa)
//				u16		t_meas	Temperature (10*�K)
// @return      s16				Altitude (m)
// *************************************************************************************************
s16 conv_pa_to_meter(u32 p_meas, u16 t_meas)
{
	const float coef2  = 0.0007f;
	const float Invt00 = 0.003470415f;
	volatile float hnoll;
	volatile float t0;
	volatile float p_low;
	volatile float fl_h;
	volatile s16 h;
	u8 i;

	// Typecast arguments
	volatile float fl_p_meas = (float)p_meas/100;	// Convert from Pa to hPa
	volatile float fl_t_meas = (float)t_meas/10;		// Convert from 10�K to 1�K
	
	for (i=0; i<=16; i++)
	{
		if (p[i] < fl_p_meas) break;
		p_low = p[i];
	}
		
	if (i==0) 
	{
		hnoll = (float)(fl_p_meas - p[0])/(p[1] - p[0])*((float)(h0[1] - h0[0]));
	}
	else if (i<15) 
	{
		hnoll = (float)(fl_p_meas - p_low)*(1 - (fl_p_meas - p[i])* coef2)/(p[i] - p_low)*((float)(h0[i] - h0[i-1])) + h0[i-1];
	}
	else if (i==15)
	{
		hnoll = (float)(fl_p_meas - p_low)/(p[i] - p_low)*((float)(h0[i] - h0[i-1])) + h0[i-1];
	}
	else // i==16
	{
		hnoll = (float)(fl_p_meas - p[16])/(p[16] - p[15])*((float)(h0[16] - h0[15])) + h0[16];
	}
	
	// Compensate temperature error
	t0 = fl_t_meas/(1 - hnoll*Invt00*0.0065f);
	fl_h = Invt00*t0*hnoll;
	h = (s16)fl_h;
	
	return (h);
}
