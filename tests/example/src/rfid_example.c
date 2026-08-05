/**
 * @file rfid_led_toggle.c
 * @brief MFRC522 authorized-card LED toggle example implementation.
 *
 * @details
 * Hardware connection:
 *
 * @code
 * MFRC522 SDA/SS -> GPIO0
 * MFRC522 RST    -> GPIO1
 * MFRC522 SCK    -> SPI_SCK
 * MFRC522 MOSI   -> SPI_SOUT
 * MFRC522 MISO   -> SPI_SIN
 * MFRC522 3.3V   -> 3.3V
 * MFRC522 GND    -> GND
 * @endcode
 *
 * Behavior:
 *
 * - LED_RED is ON while the controlled state is OFF.
 * - LED_GREEN is ON while the controlled state is ON.
 * - Scanning the authorized card toggles the controlled state.
 * - Unauthorized cards do not change the LED state.
 */

#include "rfid_example.h"

#include "Arduino.h"
#include "rc522.h"

/* ========================================================================= */
/* Example Configuration                                                     */
/* ========================================================================= */

/**
 * @brief Logical GPIO used as MFRC522 software chip-select.
 */
#define RFID_EXAMPLE_CS_PIN (GPIO0)

/**
 * @brief Logical GPIO used as MFRC522 reset.
 */
#define RFID_EXAMPLE_RESET_PIN (GPIO1)

/**
 * @brief Main RFID polling interval.
 */
#define RFID_EXAMPLE_POLL_DELAY_MS (20UL)

/**
 * @brief Delay after processing one card.
 */
#define RFID_EXAMPLE_CARD_DELAY_MS (500UL)

/**
 * @brief UID size supported by the current RC522 implementation.
 */
#define RFID_EXAMPLE_UID_SIZE (4U)

/**
 * @brief Authorized UID.
 *
 * @details
 * Decimal representation of:
 *
 * @code
 * EE 68 17 05
 * @endcode
 */
static const uint8_t s_au8AuthorizedUid[RFID_EXAMPLE_UID_SIZE] =
    {
        238U,
        104U,
        23U,
        5U};

/* ========================================================================= */
/* Private Function Prototypes                                               */
/* ========================================================================= */

static bool RFID_ExampleIsAuthorized(
    const rc522_uid_t *pUid);

static void RFID_ExampleApplyLedState(
    bool bEnabled);

static void RFID_ExamplePrintUid(
    const rc522_uid_t *pUid);

/* ========================================================================= */
/* Private Functions                                                         */
/* ========================================================================= */

/**
 * @brief Compare one detected UID with the authorized UID.
 *
 * @param[in] pUid
 * Pointer to detected UID information.
 *
 * @return Authorization state.
 *
 * @retval true
 * UID matches the authorized UID.
 *
 * @retval false
 * UID is invalid, has an unsupported size, or does not match.
 */
static bool RFID_ExampleIsAuthorized(
    const rc522_uid_t *pUid)
{
    bool bAuthorized = false;
    uint8_t u8Index = 0U;

    if (((const rc522_uid_t *)0 != pUid) &&
        (RFID_EXAMPLE_UID_SIZE == pUid->size))
    {
        bAuthorized = true;

        for (u8Index = 0U;
             u8Index < RFID_EXAMPLE_UID_SIZE;
             u8Index++)
        {
            if (s_au8AuthorizedUid[u8Index] !=
                pUid->bytes[u8Index])
            {
                bAuthorized = false;
                break;
            }
            else
            {
                /* Current UID byte matches. */
            }
        }
    }
    else
    {
        bAuthorized = false;
    }

    return bAuthorized;
}

/**
 * @brief Apply the current controlled state to the board LEDs.
 *
 * @details
 * MaaZEDU LEDs are active LOW.
 *
 * @param[in] bEnabled
 * Controlled state.
 *
 * @return None.
 */
static void RFID_ExampleApplyLedState(
    bool bEnabled)
{
    if (true == bEnabled)
    {
        /*
         * Enabled state:
         * - Green LED ON
         * - Red LED OFF
         */
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, HIGH);
    }
    else
    {
        /*
         * Disabled state:
         * - Green LED OFF
         * - Red LED ON
         */
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_RED, LOW);
    }

    return;
}

/**
 * @brief Print one UID through Serial1.
 *
 * @param[in] pUid
 * Pointer to UID information.
 *
 * @return None.
 */
static void RFID_ExamplePrintUid(
    const rc522_uid_t *pUid)
{
    uint8_t u8Index = 0U;

    if ((const rc522_uid_t *)0 != pUid)
    {
        Serial1_print("UID: ");

        for (u8Index = 0U;
             u8Index < pUid->size;
             u8Index++)
        {
            Serial1_printInt(
                (int)pUid->bytes[u8Index]);

            if ((pUid->size - 1U) != u8Index)
            {
                Serial1_print(" ");
            }
            else
            {
                /* Do not print a separator after the final UID byte. */
            }
        }

        Serial1_println("");
    }
    else
    {
        /* Invalid UID pointer. */
    }

    return;
}

/* ========================================================================= */
/* Public Functions                                                          */
/* ========================================================================= */

/**
 * @copydoc Example_RFID_LedToggle
 */
void Example_RFID_LedToggle(void)
{
    bool bInitialized = false;
    bool bLedEnabled = false;

    rc522_uid_t Uid =
        {
            {0U},
            0U,
            0U};

    /*
     * Configure status LEDs.
     */
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);

    /*
     * Initial state:
     * - Red LED ON
     * - Green LED OFF
     */
    RFID_ExampleApplyLedState(false);

    Serial1_begin(9600UL);
    delay(100UL);

    Serial1_println("");
    Serial1_println("================================");
    Serial1_println("RFID LED TOGGLE EXAMPLE");
    Serial1_println("================================");

    bInitialized =
        RC522_PCD_Init(
            RFID_EXAMPLE_CS_PIN,
            RFID_EXAMPLE_RESET_PIN);

    if (true == bInitialized)
    {
        Serial1_println("RC522 initialization: PASS");

        Serial1_print("VersionReg decimal: ");
        Serial1_printlnInt(
            (int)RC522_PCD_GetVersion());

        Serial1_println("");
        Serial1_println("Waiting for an RFID card...");
    }
    else
    {
        Serial1_println("RC522 initialization: FAILED");
    }

    while (true == bInitialized)
    {
        if (true ==
            RC522_PICC_IsNewCardPresent())
        {
            if (true ==
                RC522_PICC_ReadCardSerial(
                    &Uid))
            {
                Serial1_println("");
                Serial1_println("----------------");
                Serial1_println("Card detected");

                RFID_ExamplePrintUid(&Uid);

                if (true ==
                    RFID_ExampleIsAuthorized(
                        &Uid))
                {
                    bLedEnabled =
                        (bool)(false == bLedEnabled);

                    RFID_ExampleApplyLedState(
                        bLedEnabled);

                    Serial1_println(
                        "Authorization: GRANTED");

                    if (true == bLedEnabled)
                    {
                        Serial1_println(
                            "LED state: GREEN");
                    }
                    else
                    {
                        Serial1_println(
                            "LED state: RED");
                    }
                }
                else
                {
                    Serial1_println(
                        "Authorization: DENIED");

                    Serial1_println(
                        "LED state unchanged");
                }

                RC522_PICC_HaltA();

                /*
                 * The halted card must be removed before REQA can detect it
                 * as a new card again.
                 */
                Serial1_println(
                    "Remove the card before the next scan.");

                delay(
                    RFID_EXAMPLE_CARD_DELAY_MS);
            }
            else
            {
                Serial1_println(
                    "Card UID read: FAILED");
            }
        }
        else
        {
            /* No new card is present. */
        }

        delay(
            RFID_EXAMPLE_POLL_DELAY_MS);
    }

    /*
     * Initialization failure state.
     */
    while (1)
    {
        RFID_ExampleApplyLedState(false);
        delay(1000UL);
    }
}
