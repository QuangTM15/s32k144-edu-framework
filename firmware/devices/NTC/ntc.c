/**
 * @file ntc.c
 * @brief NTC thermistor device library implementation.
 *
 * @details
 * This implementation avoids math library dependency. Temperature conversion
 * uses a 10 kOhm B3950 lookup table with linear interpolation.
 */

#include "ntc.h"

#include "wiring_analog.h"
#include "wiring_digital.h"
#include "arduino_pins.h"

#include <stddef.h>

/* ========================================================================= */
/* Local Constants                                                            */
/* ========================================================================= */

#define NTC_ERROR_RESISTANCE_OHM (-1.0F)
#define NTC_ERROR_TEMPERATURE_CELSIUS (-273.15F)

#define NTC_C_TO_F_MULTIPLIER (1.8F)
#define NTC_C_TO_F_OFFSET (32.0F)

#define NTC_TABLE_ENTRY_COUNT (19U)

/* ========================================================================= */
/* Local Types                                                                */
/* ========================================================================= */

typedef struct
{
    int16_t s16TemperatureCelsius;
    uint32_t u32ResistanceOhm;
} NTC_TableEntry_t;

/* ========================================================================= */
/* Internal State                                                             */
/* ========================================================================= */

static NTC_Config_t s_NtcConfig =
    {
        NTC_DEFAULT_SERIES_RESISTOR_OHM,
        NTC_DEFAULT_NOMINAL_RESISTANCE_OHM,
        NTC_DEFAULT_NOMINAL_TEMPERATURE_C,
        NTC_DEFAULT_BETA_VALUE,
        NTC_DEFAULT_REFERENCE_VOLTAGE_MV};

/**
 * @brief Approximate lookup table for a common 10 kOhm B3950 NTC.
 *
 * @details
 * Resistance decreases as temperature increases.
 */
static const NTC_TableEntry_t s_NtcTable[NTC_TABLE_ENTRY_COUNT] =
    {
        {-20, 105385UL},
        {-15, 77898UL},
        {-10, 58246UL},
        {-5, 44026UL},
        {0, 33621UL},
        {5, 25892UL},
        {10, 20073UL},
        {15, 15638UL},
        {20, 12218UL},
        {25, 10000UL},
        {30, 8037UL},
        {35, 6506UL},
        {40, 5301UL},
        {45, 4348UL},
        {50, 3588UL},
        {55, 2977UL},
        {60, 2483UL},
        {70, 1759UL},
        {80, 1270UL}};

/* ========================================================================= */
/* Internal Helpers                                                           */
/* ========================================================================= */

static uint8_t NTC_IsValidConfig(const NTC_Config_t *pConfig)
{
    uint8_t u8IsValid = NTC_FALSE;

    if (NULL != pConfig)
    {
        if ((0UL != pConfig->u32SeriesResistorOhm) &&
            (0UL != pConfig->u32NominalResistanceOhm) &&
            (0UL != pConfig->u32BetaValue) &&
            (0UL != pConfig->u32ReferenceVoltageMv))
        {
            u8IsValid = NTC_TRUE;
        }
    }

    return u8IsValid;
}

static float NTC_InterpolateTemperature(uint32_t u32ResistanceOhm)
{
    uint8_t u8Index = 0U;
    uint32_t u32RHigh = 0UL;
    uint32_t u32RLow = 0UL;
    float f32THigh = 0.0F;
    float f32TLow = 0.0F;
    float f32Temperature = NTC_ERROR_TEMPERATURE_CELSIUS;
    float f32Ratio = 0.0F;

    if (s_NtcTable[0U].u32ResistanceOhm <= u32ResistanceOhm)
    {
        f32Temperature = (float)s_NtcTable[0U].s16TemperatureCelsius;
    }
    else if (s_NtcTable[NTC_TABLE_ENTRY_COUNT - 1U].u32ResistanceOhm >= u32ResistanceOhm)
    {
        f32Temperature = (float)s_NtcTable[NTC_TABLE_ENTRY_COUNT - 1U].s16TemperatureCelsius;
    }
    else
    {
        for (u8Index = 0U; u8Index < (NTC_TABLE_ENTRY_COUNT - 1U); u8Index++)
        {
            u32RHigh = s_NtcTable[u8Index].u32ResistanceOhm;
            u32RLow = s_NtcTable[u8Index + 1U].u32ResistanceOhm;

            if ((u32RHigh >= u32ResistanceOhm) &&
                (u32ResistanceOhm >= u32RLow))
            {
                f32THigh = (float)s_NtcTable[u8Index].s16TemperatureCelsius;
                f32TLow = (float)s_NtcTable[u8Index + 1U].s16TemperatureCelsius;

                f32Ratio = ((float)u32RHigh - (float)u32ResistanceOhm) /
                           ((float)u32RHigh - (float)u32RLow);

                f32Temperature = f32THigh +
                                 ((f32TLow - f32THigh) * f32Ratio);

                break;
            }
        }
    }

    return f32Temperature;
}

/* ========================================================================= */
/* Public API                                                                 */
/* ========================================================================= */

void NTC_Init(void)
{
    s_NtcConfig.u32SeriesResistorOhm = NTC_DEFAULT_SERIES_RESISTOR_OHM;
    s_NtcConfig.u32NominalResistanceOhm = NTC_DEFAULT_NOMINAL_RESISTANCE_OHM;
    s_NtcConfig.f32NominalTemperatureCelsius = NTC_DEFAULT_NOMINAL_TEMPERATURE_C;
    s_NtcConfig.u32BetaValue = NTC_DEFAULT_BETA_VALUE;
    s_NtcConfig.u32ReferenceVoltageMv = NTC_DEFAULT_REFERENCE_VOLTAGE_MV;
}

void NTC_SetConfig(const NTC_Config_t *pConfig)
{
    if (NTC_TRUE == NTC_IsValidConfig(pConfig))
    {
        s_NtcConfig = *pConfig;
    }
}

void NTC_GetConfig(NTC_Config_t *pConfig)
{
    if (NULL != pConfig)
    {
        *pConfig = s_NtcConfig;
    }
}

int NTC_ReadRaw(uint8_t u8AnalogPin)
{
    int s32RawValue = NTC_ERROR_VALUE;

    s32RawValue = analogRead(u8AnalogPin);

    return s32RawValue;
}

int NTC_ReadMilliVolts(uint8_t u8AnalogPin)
{
    int s32MilliVolts = NTC_ERROR_VALUE;

    s32MilliVolts = analogReadMilliVolts(u8AnalogPin);

    return s32MilliVolts;
}

float NTC_ReadResistance(uint8_t u8AnalogPin)
{
    int s32VoltageMv = NTC_ERROR_VALUE;
    float f32VoltageMv = 0.0F;
    float f32ReferenceMv = 0.0F;
    float f32ResistanceOhm = NTC_ERROR_RESISTANCE_OHM;

    s32VoltageMv = NTC_ReadMilliVolts(u8AnalogPin);

    if (NTC_ERROR_VALUE != s32VoltageMv)
    {
        f32VoltageMv = (float)s32VoltageMv;
        f32ReferenceMv = (float)s_NtcConfig.u32ReferenceVoltageMv;

        if ((0.0F < f32VoltageMv) && (f32VoltageMv < f32ReferenceMv))
        {
            f32ResistanceOhm =
                ((float)s_NtcConfig.u32SeriesResistorOhm * f32VoltageMv) /
                (f32ReferenceMv - f32VoltageMv);
        }
    }

    return f32ResistanceOhm;
}

float NTC_ReadCelsius(uint8_t u8AnalogPin)
{
    float f32ResistanceOhm = NTC_ERROR_RESISTANCE_OHM;
    float f32TemperatureCelsius = NTC_ERROR_TEMPERATURE_CELSIUS;

    f32ResistanceOhm = NTC_ReadResistance(u8AnalogPin);

    if (0.0F < f32ResistanceOhm)
    {
        f32TemperatureCelsius =
            NTC_InterpolateTemperature((uint32_t)f32ResistanceOhm);
    }

    return f32TemperatureCelsius;
}

float NTC_ReadFahrenheit(uint8_t u8AnalogPin)
{
    float f32TemperatureCelsius = NTC_ERROR_TEMPERATURE_CELSIUS;
    float f32TemperatureFahrenheit = NTC_ERROR_TEMPERATURE_CELSIUS;

    f32TemperatureCelsius = NTC_ReadCelsius(u8AnalogPin);

    if (NTC_ERROR_TEMPERATURE_CELSIUS != f32TemperatureCelsius)
    {
        f32TemperatureFahrenheit =
            (f32TemperatureCelsius * NTC_C_TO_F_MULTIPLIER) +
            NTC_C_TO_F_OFFSET;
    }

    return f32TemperatureFahrenheit;
}

uint8_t NTC_ReadThreshold(uint8_t u8DigitalPin)
{
    uint8_t u8ThresholdState = NTC_FALSE;
    int s32DigitalState = 0;

    if (ARDUINO_VALID_TRUE == Arduino_HasDigitalCapability(u8DigitalPin))
    {
        s32DigitalState = digitalRead(u8DigitalPin);

        if (0 != s32DigitalState)
        {
            u8ThresholdState = NTC_TRUE;
        }
        else
        {
            u8ThresholdState = NTC_FALSE;
        }
    }

    return u8ThresholdState;
}