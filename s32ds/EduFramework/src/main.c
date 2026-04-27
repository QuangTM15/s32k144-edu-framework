#include "S32K144.h"
#include "clock.h"
#include "lpuart.h"
#include "lpi2c.h"
#include <stdio.h>

#define LCD_ADDR        0x27U
#define I2C_TIMEOUT     100000U

#define LCD_RS          0x01U
#define LCD_RW          0x02U
#define LCD_EN          0x04U
#define LCD_BL          0x08U

static void delay_loop(uint32_t count)
{
    while (count--)
    {
        __asm("nop");
    }
}

static void UART_PrintStatus(const char *text, LPI2C_Status_t status)
{
    char msg[64];

    sprintf(msg, "%s status = %d\r\n", text, status);
    LPUART_WriteString(IP_LPUART1, msg);
}

static LPI2C_Status_t LCD_I2C_WriteByte(uint8_t data)
{
    return LPI2C_MasterWriteBlocking(IP_LPI2C0,
                                     LCD_ADDR,
                                     &data,
                                     1U,
                                     I2C_TIMEOUT);
}

static void LCD_PulseEnable(uint8_t data)
{
    (void)LCD_I2C_WriteByte(data | LCD_EN | LCD_BL);
    delay_loop(3000U);

    (void)LCD_I2C_WriteByte((data & ~LCD_EN) | LCD_BL);
    delay_loop(3000U);
}

static void LCD_Write4Bits(uint8_t value)
{
    (void)LCD_I2C_WriteByte(value | LCD_BL);
    LCD_PulseEnable(value);
}

static void LCD_Send(uint8_t value, uint8_t mode)
{
    uint8_t high;
    uint8_t low;

    high = (uint8_t)(value & 0xF0U);
    low  = (uint8_t)((value << 4U) & 0xF0U);

    LCD_Write4Bits(high | mode);
    LCD_Write4Bits(low | mode);
}

static void LCD_Command(uint8_t cmd)
{
    LCD_Send(cmd, 0U);
    delay_loop(80000U);
}

static void LCD_Data(uint8_t data)
{
    LCD_Send(data, LCD_RS);
    delay_loop(80000U);
}

static void LCD_Print(const char *s)
{
    while (*s != '\0')
    {
        LCD_Data((uint8_t)*s);
        s++;
    }
}

static void LCD_Init(void)
{
    LPI2C_Status_t st;

    delay_loop(3000000U);

    st = LCD_I2C_WriteByte(LCD_BL);
    UART_PrintStatus("LCD backlight write", st);

    /*
     * HD44780 4-bit init sequence
     */
    LCD_Write4Bits(0x30U);
    delay_loop(3000000U);

    LCD_Write4Bits(0x30U);
    delay_loop(300000U);

    LCD_Write4Bits(0x30U);
    delay_loop(300000U);

    LCD_Write4Bits(0x20U);
    delay_loop(300000U);

    LCD_Command(0x28U);   /* 4-bit, 2 lines, 5x8 font */
    LCD_Command(0x0CU);   /* display ON, cursor OFF */
    LCD_Command(0x06U);   /* entry mode */
    LCD_Command(0x01U);   /* clear display */
    delay_loop(3000000U);
}

int main(void)
{
    LPI2C_MasterConfig_t i2cConfig;
    LPUART_Config_t uartConfig;
    LPI2C_Status_t status;
    uint8_t addr;
    uint8_t dummy = 0U;
    char msg[64];

    SOSC_init_8MHz();
    SPLL_init_160MHz();
    NormalRUNmode_80MHz();

    uartConfig.baudRate = 9600U;
    uartConfig.srcClockHz = 8000000U;
    LPUART_Init(IP_LPUART1, &uartConfig);

    LPI2C_MasterGetDefaultConfig(&i2cConfig);
    i2cConfig.baudRate = 100000U;
    LPI2C_MasterInit(IP_LPI2C0, &i2cConfig);

    LPUART_WriteString(IP_LPUART1, "\r\nI2C LCD TEST START\r\n");

    for (addr = 1U; addr < 0x7FU; addr++)
    {
        status = LPI2C_MasterWriteBlocking(IP_LPI2C0,
                                           addr,
                                           &dummy,
                                           0U,
                                           10000U);

        if (status == LPI2C_STATUS_OK)
        {
            sprintf(msg, "Found device at 0x%02X\r\n", addr);
            LPUART_WriteString(IP_LPUART1, msg);
        }
    }

    LPUART_WriteString(IP_LPUART1, "Scan done\r\n");

    LCD_Init();

    LCD_Command(0x80U);
    LCD_Print("HELLO I2C");

    LCD_Command(0xC0U);
    LCD_Print("EduFramework");

    LPUART_WriteString(IP_LPUART1, "LCD init/write done\r\n");

    while (1)
    {
    }
}
