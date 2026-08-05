/**
 * @file rfid_led_toggle.h
 * @brief MFRC522 authorized-card LED toggle example.
 *
 * @details
 * This example demonstrates how to:
 *
 * - Initialize an MFRC522 reader.
 * - Detect an ISO/IEC 14443A card.
 * - Read a four-byte UID.
 * - Compare the UID with an authorized UID.
 * - Toggle the board LED state after an authorized card scan.
 *
 * The example prevents repeated toggling while the same card remains
 * continuously above the reader.
 */

#ifndef RFID_LED_TOGGLE_H
#define RFID_LED_TOGGLE_H

/**
 * @brief Run the MFRC522 authorized-card LED toggle example.
 *
 * @details
 * The function does not return.
 *
 * @return None.
 */
void Example_RFID_LedToggle(void);

#endif /* RFID_LED_TOGGLE_H */