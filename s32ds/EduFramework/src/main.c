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
#include "MPU6050.h"

#include "mpu6050_demo.h"

int main(void)
{
    Example_MPU6050_Run();
    while(1){

    }

    return 0;
}
