#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "S32K144.h"

/* =========================================
 * LEDs
 * NOTE:
 * Use real hardware-tested LED mapping.
 * Do NOT trust MaaZEDU document for LED colors.
 * ========================================= */
#define BOARD_LED_RED_PORT      IP_PORTD
#define BOARD_LED_RED_GPIO      IP_PTD
#define BOARD_LED_RED_PIN       15U

#define BOARD_LED_BLUE_PORT     IP_PORTD
#define BOARD_LED_BLUE_GPIO     IP_PTD
#define BOARD_LED_BLUE_PIN      16U

#define BOARD_LED_GREEN_PORT    IP_PORTD
#define BOARD_LED_GREEN_GPIO    IP_PTD
#define BOARD_LED_GREEN_PIN     0U

/* =========================================
 * Buttons
 * ========================================= */
#define BOARD_BUTTON0_PORT      IP_PORTC
#define BOARD_BUTTON0_GPIO      IP_PTC
#define BOARD_BUTTON0_PIN       12U

#define BOARD_BUTTON1_PORT      IP_PORTC
#define BOARD_BUTTON1_GPIO      IP_PTC
#define BOARD_BUTTON1_PIN       13U

/* =========================================
 * CAN
 * ========================================= */
#define BOARD_CAN_TX_PORT       IP_PORTE
#define BOARD_CAN_TX_GPIO       IP_PTE
#define BOARD_CAN_TX_PIN        5U

#define BOARD_CAN_RX_PORT       IP_PORTE
#define BOARD_CAN_RX_GPIO       IP_PTE
#define BOARD_CAN_RX_PIN        4U

/* =========================================
 LPSPI
 * ========================================= */
#define BOARD_SBC_SCK_PORT      IP_PORTB
#define BOARD_SBC_SCK_GPIO      IP_PTB
#define BOARD_SBC_SCK_PIN       2U

#define BOARD_SBC_MISO_PORT     IP_PORTB
#define BOARD_SBC_MISO_GPIO     IP_PTB
#define BOARD_SBC_MISO_PIN      3U

#define BOARD_SBC_MOSI_PORT     IP_PORTB
#define BOARD_SBC_MOSI_GPIO     IP_PTB
#define BOARD_SBC_MOSI_PIN      1U

#define BOARD_SBC_CS_PORT       IP_PORTB
#define BOARD_SBC_CS_GPIO       IP_PTB
#define BOARD_SBC_CS_PIN        0U

/* =========================================
 * GPIO Header
 * ========================================= */
#define BOARD_GPIO0_PORT        IP_PORTE
#define BOARD_GPIO0_GPIO        IP_PTE
#define BOARD_GPIO0_PIN         0U

#define BOARD_GPIO1_PORT        IP_PORTD
#define BOARD_GPIO1_GPIO        IP_PTD
#define BOARD_GPIO1_PIN         17U

#define BOARD_GPIO2_PORT        IP_PORTD
#define BOARD_GPIO2_GPIO        IP_PTD
#define BOARD_GPIO2_PIN         14U

#define BOARD_GPIO3_PORT        IP_PORTD
#define BOARD_GPIO3_GPIO        IP_PTD
#define BOARD_GPIO3_PIN         13U

#define BOARD_GPIO4_PORT        IP_PORTD
#define BOARD_GPIO4_GPIO        IP_PTD
#define BOARD_GPIO4_PIN         12U

#define BOARD_GPIO5_PORT        IP_PORTD
#define BOARD_GPIO5_GPIO        IP_PTD
#define BOARD_GPIO5_PIN         11U

#define BOARD_GPIO6_PORT        IP_PORTD
#define BOARD_GPIO6_GPIO        IP_PTD
#define BOARD_GPIO6_PIN         10U

#define BOARD_GPIO7_PORT        IP_PORTD
#define BOARD_GPIO7_GPIO        IP_PTD
#define BOARD_GPIO7_PIN         9U

#define BOARD_GPIO8_PORT        IP_PORTD
#define BOARD_GPIO8_GPIO        IP_PTD
#define BOARD_GPIO8_PIN         8U

#define BOARD_GPIO9_PORT        IP_PORTD
#define BOARD_GPIO9_GPIO        IP_PTD
#define BOARD_GPIO9_PIN         5U

/* =========================================
 * UART
 * ========================================= */
/* UART1 -> J-Link USB / debug serial */
#define BOARD_UART1_RX_PORT     IP_PORTC
#define BOARD_UART1_RX_GPIO     IP_PTC
#define BOARD_UART1_RX_PIN      6U

#define BOARD_UART1_TX_PORT     IP_PORTC
#define BOARD_UART1_TX_GPIO     IP_PTC
#define BOARD_UART1_TX_PIN      7U

/* UART2 -> external header */
#define BOARD_UART2_RX_PORT     IP_PORTD
#define BOARD_UART2_RX_GPIO     IP_PTD
#define BOARD_UART2_RX_PIN      6U

#define BOARD_UART2_TX_PORT     IP_PORTD
#define BOARD_UART2_TX_GPIO     IP_PTD
#define BOARD_UART2_TX_PIN      7U

/* =========================================
 * I2C
 * ========================================= */
#define BOARD_I2C0_SCL_PORT     IP_PORTA
#define BOARD_I2C0_SCL_GPIO     IP_PTA
#define BOARD_I2C0_SCL_PIN      3U

#define BOARD_I2C0_SDA_PORT     IP_PORTA
#define BOARD_I2C0_SDA_GPIO     IP_PTA
#define BOARD_I2C0_SDA_PIN      2U

/* =========================================
 * ADC
 * ========================================= */
#define BOARD_ADC0_SE12_PORT    IP_PORTC
#define BOARD_ADC0_SE12_GPIO    IP_PTC
#define BOARD_ADC0_SE12_PIN     14U

#define BOARD_ADC0_SE13_PORT    IP_PORTC
#define BOARD_ADC0_SE13_GPIO    IP_PTC
#define BOARD_ADC0_SE13_PIN     15U

#endif /* BOARD_PINS_H */