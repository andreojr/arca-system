#include "main.h"
#include "press.h"
#include <stdbool.h>
#include <stdio.h>

#define PRESS_CALIBRATION_SAMPLES 10u

extern ADC_HandleTypeDef hadc1;

static uint32_t s_threshold = 0xFFFFFFFFu; /* até Press_Init() calibrar, nunca dispara */

uint32_t _read_adc_value(void) {
    uint32_t adcValue = 0;
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);

    return adcValue;
}

void Press_Init(void) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < PRESS_CALIBRATION_SAMPLES; i++) {
        sum += _read_adc_value();
        HAL_Delay(5);
    }

    uint32_t baseline = sum / PRESS_CALIBRATION_SAMPLES;
    s_threshold = baseline + PRESS_MARGIN;
    printf("[PRESS] calibrado: repouso=%lu threshold=%lu\r\n", baseline, s_threshold);
}

bool Press_IsDoorPressed(void) {
    uint32_t current_pressure = _read_adc_value();
    if (current_pressure < s_threshold) return false;

    HAL_Delay(DEBOUNCE_MS);
    current_pressure = _read_adc_value();
    return current_pressure >= s_threshold;
}
