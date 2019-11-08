/*
 **
 ** Source file generated on November 5, 2018 at 12:51:42.	
 **
 ** Copyright (C) 2011-2018 Analog Devices Inc., All Rights Reserved.
 **
 ** This file is generated automatically based upon the options selected in 
 ** the Pin Multiplexing configuration editor. Changes to the Pin Multiplexing
 ** configuration should be made by changing the appropriate options rather
 ** than editing this file.
 **
 ** Selected Peripherals
 ** --------------------
 ** SPI2 (CLK, MOSI, MISO, CS_0, CS_2)
 ** I2C0 (SCL, SDA)
 ** BEEPER0 (TONE_N, TONE_P)
 ** UART0 (Tx, Rx)
 ** UART1 (Tx, Rx)
 ** TIMER0 (TMR0_OUT)
 ** SWD0 (SWD0_CLK, SWD0_Data)
 ** SWV (SWV)
 ** ADC0_IN (ADC0_VIN0)
 ** SYS_CLK (OUT)
 ** SYS_BMODE (SYS_BMODE0)
 **
 ** GPIO (unavailable)
 ** ------------------
 ** P0_04, P0_05, P0_06, P0_07, P0_08, P0_09, P0_10, P0_11, P0_14, P1_01, P1_02, P1_03,
 ** P1_04, P1_05, P1_09, P1_15, P2_00, P2_03, P2_11, P2_15
 */

#include <sys/platform.h>
#include <stdint.h>

#define SPI2_CLK_PORTP1_MUX  ((uint16_t) ((uint16_t) 1<<4))
#define SPI2_MOSI_PORTP1_MUX  ((uint16_t) ((uint16_t) 1<<6))
#define SPI2_MISO_PORTP1_MUX  ((uint16_t) ((uint16_t) 1<<8))
#define SPI2_CS_0_PORTP1_MUX  ((uint16_t) ((uint16_t) 1<<10))
#define SPI2_CS_2_PORTP2_MUX  ((uint32_t) ((uint32_t) 1<<30))
#define I2C0_SCL_PORTP0_MUX  ((uint16_t) ((uint16_t) 1<<8))
#define I2C0_SDA_PORTP0_MUX  ((uint16_t) ((uint16_t) 1<<10))
#define BEEPER0_TONE_N_PORTP0_MUX  ((uint32_t) ((uint32_t) 1<<16))
#define BEEPER0_TONE_P_PORTP0_MUX  ((uint32_t) ((uint32_t) 1<<18))
#define UART0_TX_PORTP0_MUX  ((uint32_t) ((uint32_t) 1<<20))
#define UART0_RX_PORTP0_MUX  ((uint32_t) ((uint32_t) 1<<22))
#define UART1_TX_PORTP1_MUX  ((uint32_t) ((uint32_t) 2<<30))
#define UART1_RX_PORTP2_MUX  ((uint16_t) ((uint16_t) 2<<0))
#define TIMER0_TMR0_OUT_PORTP0_MUX  ((uint32_t) ((uint32_t) 1<<28))
#define SWD0_SWD0_CLK_PORTP0_MUX  ((uint16_t) ((uint16_t) 0<<12))
#define SWD0_SWD0_DATA_PORTP0_MUX  ((uint16_t) ((uint16_t) 0<<14))
#define SWV_SWV_PORTP1_MUX  ((uint32_t) ((uint32_t) 3<<18))
#define ADC0_IN_ADC0_VIN0_PORTP2_MUX  ((uint16_t) ((uint16_t) 1<<6))
#define SYS_CLK_OUT_PORTP2_MUX  ((uint32_t) ((uint32_t) 2<<22))
#define SYS_BMODE_SYS_BMODE0_PORTP1_MUX  ((uint16_t) ((uint16_t) 0<<2))

int32_t adi_initpinmux(void);

/*
 * Initialize the Port Control MUX Registers
 */
int32_t adi_initpinmux(void) {
    /* PORTx_MUX registers */
    *pREG_GPIO0_CFG = I2C0_SCL_PORTP0_MUX | I2C0_SDA_PORTP0_MUX
     | BEEPER0_TONE_N_PORTP0_MUX | BEEPER0_TONE_P_PORTP0_MUX | UART0_TX_PORTP0_MUX
     | UART0_RX_PORTP0_MUX | TIMER0_TMR0_OUT_PORTP0_MUX | SWD0_SWD0_CLK_PORTP0_MUX
     | SWD0_SWD0_DATA_PORTP0_MUX;
    *pREG_GPIO1_CFG = SPI2_CLK_PORTP1_MUX | SPI2_MOSI_PORTP1_MUX
     | SPI2_MISO_PORTP1_MUX | SPI2_CS_0_PORTP1_MUX | UART1_TX_PORTP1_MUX
     | SWV_SWV_PORTP1_MUX | SYS_BMODE_SYS_BMODE0_PORTP1_MUX;
    *pREG_GPIO2_CFG = SPI2_CS_2_PORTP2_MUX | UART1_RX_PORTP2_MUX
     | ADC0_IN_ADC0_VIN0_PORTP2_MUX | SYS_CLK_OUT_PORTP2_MUX;

    return 0;
}

