@echo off
setlocal
where msp430-elf-gcc.exe >nul 2>nul || (echo ERROR: msp430-elf-gcc.exe not found in PATH. & exit /b 1)
cd /d %~dp0
make FREQUENCY=%1
