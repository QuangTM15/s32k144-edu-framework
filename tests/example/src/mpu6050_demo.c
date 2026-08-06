/*
 * mpu6050_demo.c
 *
 *  Created on: Aug 6, 2026
 *      Author: Admin
 */


/**
 * @file mpu6050_demo.c
 * @brief MPU6050 demonstration implementation.
 */

#include "mpu6050_demo.h"
#include "Arduino.h"         //
#include "hardware_serial.h" //
#include "wire.h"            //
#include "MPU6050.h"

/* Global sensor object instance */
static MPU6050_t mySensor;

/**
 * @copydoc Example_MPU6050_Run
 */
void Example_MPU6050_Run(void)
{
    /* 1. Mandatory framework setup (Clocks and Time) */
    setup();

    /* 2. Initialize Serial1 for debugging output (Hercules terminal) */
    Serial1_begin(9600); //[cite: 4]
    Serial1_println("--- MPU6050 Sensor Demo ---"); //[cite: 4]

    /* 3. Initialize I2C Bus in Master mode[cite: 4] */
    Wire_begin(); //[cite: 4]

    /* 4. Initialize MPU6050 Sensor */
    if (MPU6050_begin(&mySensor, MPU6050_ADDRESS_DEFAULT))
    {
        Serial1_println("MPU6050 Initialized Successfully!"); //[cite: 4]
    }
    else
    {
        Serial1_println("ERROR: MPU6050 not found. Check wiring!"); //[cite: 4]
        while (1)
        {
            /* Halt program if sensor initialization fails */
            delay(100); //[cite: 4]
        }
    }

    /* 5. Set custom ranges (Optional, default is 2G and 250 Deg/s) */
    MPU6050_setAccelerometerRange(&mySensor, MPU6050_RANGE_4_G);
    MPU6050_setGyroRange(&mySensor, MPU6050_RANGE_500_DEG);

    /* 6. Calibration process */
    Serial1_println("Please keep the sensor FLAT and STILL for calibration..."); //[cite: 4]
    delay(2000); /* Give user 2 seconds to release the board[cite: 4] */

    Serial1_println("Calibrating..."); //[cite: 4]
    /* Take 500 samples to calculate the zero-bias offset */
    MPU6050_calibrate(&mySensor, 500U);
    Serial1_println("Calibration Done!"); //[cite: 4]
    Serial1_println("---------------------------------"); //[cite: 4]

    /* 7. Main loop: Read data and print it via Serial1 */
    while (1)
    {
        /* Fetch all latest data from the sensor and apply offsets */
        MPU6050_read(&mySensor);

        /* Print Accelerometer Data using Getters */
        Serial1_print("Accel (g)   | X: ");                  //[cite: 4]
        Serial1_printFloat(MPU6050_getAccelerationX(&mySensor)); //[cite: 4]
        Serial1_print("  Y: ");                              //[cite: 4]
        Serial1_printFloat(MPU6050_getAccelerationY(&mySensor)); //[cite: 4]
        Serial1_print("  Z: ");                              //[cite: 4]
        Serial1_printlnFloat(MPU6050_getAccelerationZ(&mySensor)); //[cite: 4]

        /* Print Gyroscope Data using Getters */
        Serial1_print("Gyro (d/s)  | X: ");                  //[cite: 4]
        Serial1_printFloat(MPU6050_getGyroX(&mySensor));         //[cite: 4]
        Serial1_print("  Y: ");                              //[cite: 4]
        Serial1_printFloat(MPU6050_getGyroY(&mySensor));         //[cite: 4]
        Serial1_print("  Z: ");                              //[cite: 4]
        Serial1_printlnFloat(MPU6050_getGyroZ(&mySensor));       //[cite: 4]

        /* Print Temperature */
        Serial1_print("Temp (C)    | ");                     //[cite: 4]
        Serial1_printlnFloat(MPU6050_getTemperature(&mySensor)); //[cite: 4]

        Serial1_println(""); //[cite: 4]

        /* Read every 500ms[cite: 4] */
        delay(500); //[cite: 4]
    }
}
