#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include <stdint.h>

typedef struct
{
    volatile uint16_t phase_current_a_raw;
    volatile uint16_t phase_current_c_raw;
    volatile uint16_t dc_bus_raw;
    volatile uint32_t adc_isr_count;
    volatile uint32_t encoder_position;
    volatile uint32_t encoder_position_latched;
    volatile int16_t encoder_direction;
} MotorControlState;

extern volatile MotorControlState g_motorControlState;

#endif
