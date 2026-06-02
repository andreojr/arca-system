#include "main.h"
#include "press.h"
#include <stdbool.h>

extern ADC_HandleTypeDef hadc1;

uint32_t _read_adc_value(void) {
    uint32_t adcValue = 0;
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        adcValue = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);

    return adcValue;
}

bool Press_IsDoorPressed(void) {
    uint32_t current_pressure = _read_adc_value();
    if (current_pressure >= THRESHOLD_DOOR) return false;

    HAL_Delay(DEBOUNCE_MS);
    current_pressure = _read_adc_value();
    return current_pressure < THRESHOLD_DOOR;
}