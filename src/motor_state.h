#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t control_state;
    volatile uint16_t phase_current_a_raw;
    volatile uint16_t phase_current_c_raw;
    volatile uint16_t dc_bus_raw;
    volatile uint32_t adc_isr_count;
    volatile uint32_t encoder_position;
    volatile uint32_t encoder_position_latched;
    volatile int16_t encoder_direction;
    volatile float phase_current_a;
    volatile float phase_current_b;
    volatile float phase_current_c;
    volatile float current_alpha;
    volatile float current_beta;
    volatile float current_d;
    volatile float current_q;
    volatile float current_offset_a_counts;
    volatile float current_offset_c_counts;
    volatile float current_d_ref;
    volatile float current_q_ref;
    volatile float dc_bus_voltage;
    volatile float voltage_d;
    volatile float voltage_q;
    volatile float voltage_d_ff;
    volatile float voltage_q_ff;
    volatile float electrical_angle_rad;
    volatile float electrical_zero_offset_rad;
    volatile float mechanical_speed_rad_per_sec;
    volatile float electrical_speed_rad_per_sec;
    volatile float pwm_duty_a;
    volatile float pwm_duty_b;
    volatile float pwm_duty_c;
} MotorControlState;

extern volatile MotorControlState g_motorControlState;

#endif
