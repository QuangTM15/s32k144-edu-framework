/**
 * @file main.c
 * @brief Complete MFRC522 functional test.
 *
 * @details
 * This application validates the complete supported MFRC522 flow:
 *
 * - Reader initialization.
 * - Version register communication.
 * - Antenna disable and enable.
 * - Hardware and software reset.
 * - Reader reinitialization.
 * - Card presence detection.
 * - Four-byte UID reading.
 * - SAK reading.
 * - PICC halt.
 *
 * Hardware connection:
 *
 * - MFRC522 SDA/SS -> GPIO0
 * - MFRC522 RST    -> GPIO1
 * - MFRC522 SCK    -> SPI_SCK
 * - MFRC522 MOSI   -> SPI_SOUT
 * - MFRC522 MISO   -> SPI_SIN
 * - MFRC522 3.3V   -> 3.3V
 * - MFRC522 GND    -> GND
 */

#include "Arduino.h"
#include "rc522.h"

/* ========================================================================= */
/* Test Configuration                                                        */
/* ========================================================================= */

/**
 * @brief Logical GPIO used as MFRC522 software chip-select.
 */
#define RC522_TEST_CS_PIN              (GPIO0)

/**
 * @brief Logical GPIO used as MFRC522 reset.
 */
#define RC522_TEST_RESET_PIN           (GPIO1)

/**
 * @brief Delay used during antenna control verification.
 */
#define RC522_TEST_ANTENNA_DELAY_MS    (500UL)

/**
 * @brief Delay after reporting one card.
 */
#define RC522_TEST_CARD_DELAY_MS       (1000UL)

/**
 * @brief Main polling delay.
 */
#define RC522_TEST_POLL_DELAY_MS       (20UL)

/* ========================================================================= */
/* Private Function Prototypes                                               */
/* ========================================================================= */

static void RC522_TestPrintUid(
    const rc522_uid_t *pUid);

static bool RC522_TestReaderSetup(void);

/* ========================================================================= */
/* Private Functions                                                         */
/* ========================================================================= */

/**
 * @brief Print UID and SAK values through Serial1.
 *
 * @param[in] pUid
 * Pointer to UID information.
 *
 * @return None.
 */
static void RC522_TestPrintUid(
    const rc522_uid_t *pUid)
{
    uint8_t u8Index = 0U;

    if ((const rc522_uid_t *)0 != pUid)
    {
        Serial1_print("UID decimal: ");

        for (u8Index = 0U;
             u8Index < pUid->size;
             u8Index++)
        {
            Serial1_printInt(
                (int)pUid->bytes[u8Index]);

            if ((pUid->size - 1U) !=
                u8Index)
            {
                Serial1_print(" ");
            }
            else
            {
                /* Do not print a separator after the final byte. */
            }
        }

        Serial1_println("");

        Serial1_print("UID size: ");
        Serial1_printlnInt(
            (int)pUid->size);

        Serial1_print("SAK decimal: ");
        Serial1_printlnInt(
            (int)pUid->sak);
    }
    else
    {
        /* Invalid UID pointer. */
    }

    return;
}

/**
 * @brief Initialize and validate the MFRC522 reader.
 *
 * @return Setup state.
 *
 * @retval true
 * Initialization and version communication succeeded.
 *
 * @retval false
 * Initialization failed.
 */
static bool RC522_TestReaderSetup(void)
{
    bool bInitialized = false;

    bInitialized =
        RC522_PCD_Init(
            RC522_TEST_CS_PIN,
            RC522_TEST_RESET_PIN);

    if (true == bInitialized)
    {
        Serial1_println(
            "RC522 initialization: PASS");

        Serial1_print(
            "VersionReg decimal: ");

        Serial1_printlnInt(
            (int)RC522_PCD_GetVersion());

        if (true ==
            RC522_PCD_IsInitialized())
        {
            Serial1_println(
                "Initialization state: PASS");
        }
        else
        {
            Serial1_println(
                "Initialization state: FAILED");

            bInitialized = false;
        }
    }
    else
    {
        Serial1_println(
            "RC522 initialization: FAILED");
    }

    return bInitialized;
}

/* ========================================================================= */
/* Main                                                                      */
/* ========================================================================= */

int main(void)
{
    bool bInitialized = false;
    bool bResetCompleted = false;

    rc522_uid_t Uid =
    {
        {0U},
        0U,
        0U
    };

    setup();

    Serial1_begin(9600UL);
    delay(100UL);

    Serial1_println("");
    Serial1_println(
        "================================");

    Serial1_println(
        "RC522 COMPLETE FUNCTIONAL TEST");

    Serial1_println(
        "================================");

    /*
     * Test 1: Initial reader setup.
     */
    Serial1_println("");
    Serial1_println(
        "Test 1: Reader initialization");

    bInitialized =
        RC522_TestReaderSetup();

    /*
     * Test 2: Antenna control.
     */
    if (true == bInitialized)
    {
        Serial1_println("");
        Serial1_println(
            "Test 2: Antenna control");

        RC522_PCD_AntennaOff();

        Serial1_println(
            "Antenna OFF command: DONE");

        delay(
            RC522_TEST_ANTENNA_DELAY_MS);

        RC522_PCD_AntennaOn();

        Serial1_println(
            "Antenna ON command: DONE");

        delay(
            RC522_TEST_ANTENNA_DELAY_MS);
    }
    else
    {
        /* Reader initialization failed. Skip antenna test. */
    }

    /*
     * Test 3: Reset and reinitialization.
     */
    if (true == bInitialized)
    {
        Serial1_println("");
        Serial1_println(
            "Test 3: Reset");

        bResetCompleted =
            RC522_PCD_Reset();

        if (true == bResetCompleted)
        {
            Serial1_println(
                "Hardware/software reset: PASS");
        }
        else
        {
            Serial1_println(
                "Hardware/software reset: FAILED");
        }

        /*
         * RC522_PCD_Reset() invalidates the active configuration.
         * Run complete initialization again.
         */
        Serial1_println("");
        Serial1_println(
            "Test 4: Reinitialization");

        bInitialized =
            RC522_TestReaderSetup();
    }
    else
    {
        /* Reader initialization failed. Skip reset test. */
    }

    if (true == bInitialized)
    {
        Serial1_println("");
        Serial1_println(
            "Test 5: Card detection and UID");

        Serial1_println(
            "Bring an RFID card close...");
    }
    else
    {
        Serial1_println("");
        Serial1_println(
            "Reader unavailable. Test stopped.");
    }

    while (true == bInitialized)
    {
        if (true ==
            RC522_PICC_IsNewCardPresent())
        {
            Serial1_println("");
            Serial1_println(
                "----------------");

            Serial1_println(
                "Card detected");

            if (true ==
                RC522_PICC_ReadCardSerial(
                    &Uid))
            {
                Serial1_println(
                    "UID read: PASS");

                RC522_TestPrintUid(&Uid);

                RC522_PICC_HaltA();

                Serial1_println(
                    "PICC halt command: SENT");

                Serial1_println(
                    "Remove the card before testing again.");

                delay(
                    RC522_TEST_CARD_DELAY_MS);
            }
            else
            {
                Serial1_println(
                    "UID read: FAILED");

                delay(
                    RC522_TEST_CARD_DELAY_MS);
            }
        }
        else
        {
            /* No new card detected. */
        }

        delay(
            RC522_TEST_POLL_DELAY_MS);
    }

    while (1)
    {
        delay(1000UL);
    }
}
