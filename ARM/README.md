# TemperatureTest — STM32F411CEU6

Keil uVision 5 project to read one Novosense NST118-CDNR temperature sensor once per second.

## Compile-time sensor selection
Edit SENSOR_SELECT in Src/main.c:
- 1: I2C1 PB6/PB7, address 0x48
- 2: I2C1 PB10/PB3, address 0x48
- 3: I2C1 PB10/PB3, address 0x49

USART1 PA9 TX / PA10 RX, 115200 baud, 8-N-1. Output: T=+23.5000 C\r\n. Error: ERR=I2C.

HSE: 8 MHz. PLL SYSCLK: 84 MHz. TIM2 generates a 1 Hz interrupt. Its ISR only sets a flag; main reads the sensor, transmits the result and waits with WFI (Sleep mode) for the next interrupt.

NST118 powers up with the temperature register selected. Temperature is signed 12-bit two's-complement in bits 15:4, 0.0625 °C/LSB. Datasheet: https://www.novosns.com/en/datasheet/NST118-CDNR/NST118.pdf

All project files are kept inside ARM.