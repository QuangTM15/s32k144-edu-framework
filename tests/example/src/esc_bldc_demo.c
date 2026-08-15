/**
 * @file esc_bldc_demo.c
 * @brief ESC driver demonstration implementation.
 */

#include "esc_bldc_demo.h"
#include "Arduino.h"
#include "hardware_serial.h"
#include "esc.h"

/* Global ESC control object instance */
static ESC_t myESC;

/*
 * Define the logical pin for ESC signal control.
 * Must select a pin with PIN_CAP_PWM capability (e.g., GPIO2 to GPIO8).
 */
#define ESC_SIGNAL_PIN  GPIO2

/* ========================================================================= */
/* Main Example Function                                                     */
/* ========================================================================= */

/**
 * @copydoc Example_ESC_Run
 */
void Example_ESC_Run(void)
{
    /* 1. Mandatory call to initialize Clock and Timer (LPIT) */
    setup();

    /* 2. Initialize UART to monitor the execution process via terminal */
    Serial1_begin(9600);
    Serial1_println("=== BLDC Motor ESC Demo ===");

    /* 3. Initialize 50Hz PWM configuration for the ESC on the selected pin */
    if (!ESC_Init(&myESC, ESC_SIGNAL_PIN))
    {
        Serial1_println("ERROR: Selected pin does not support PWM!");
        while (1)
        {
            delay(100); /* Halt the program if the pin configuration is incorrect */
        }
    }

    /* 4. Arming process - Very important */
    Serial1_println("Arming ESC... Please wait 3 seconds.");
    Serial1_println("NOTE: Ensure the ESC is powered by a LiPo battery NOW!");

    /* Send 1000us pulse continuously for 3 seconds so the ESC confirms the minimum throttle */
    ESC_Arm(&myESC);

    Serial1_println("ESC Armed and Ready! (You should hear confirmation beeps)");
    delay(1000);

    /* 5. Main loop: Control motor speed variations */
    while (1)
    {
        /* Spin the motor lightly at 10% throttle */
        Serial1_println("Throttle: 30% (Low Speed)");
        ESC_SetThrottle(&myESC, 30U);
        delay(3000); /* Hold this speed for 3 seconds */

        /* Increase speed to 20% throttle */
        Serial1_println("Throttle: 50% (Medium Speed)");
        ESC_SetThrottle(&myESC, 50U);
        delay(3000);

        /* Cut off throttle (0%) to stop the motor */
        Serial1_println("Throttle: 0% (Stop)");
        ESC_SetThrottle(&myESC, 0U);
        delay(4000); /* Stop for 4 seconds before repeating the cycle */
    }
}
