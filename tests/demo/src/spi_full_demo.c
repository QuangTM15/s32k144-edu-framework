#include "Arduino.h"
#include "spi_full_demo.h"

#define IS_MASTER   0

#define CMD_TOGGLE  0xA5U

void Demo_SPI_Full(void)
{
    setup();

    /* ================= INIT ================= */

#if IS_MASTER
    pinMode(BTN0, INPUT_PULLUP);
#else
    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, HIGH);   /* OFF (active low) */
#endif

    SPI_begin(IS_MASTER ? SPI_ROLE_MASTER : SPI_ROLE_SLAVE);

    /* ================= LOOP ================= */
    while (1)
    {

#if IS_MASTER
        if (digitalRead(BTN0) == LOW)
        {
            SPI_transfer(CMD_TOGGLE);

            /* debounce */
            while (digitalRead(BTN0) == LOW);
            delay(50);
        }

#else
        SPI_write(0x00);

        if (SPI_available())
        {
            uint8_t data;

            data = SPI_read();

            if (data == CMD_TOGGLE)
            {
                digitalToggle(LED_RED);
            }
        }
#endif
    }
}
