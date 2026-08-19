#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <stdbool.h>

typedef enum
{
    MOTOR_CONTROL_STATE_OFFSET_CALIBRATION = 0,
    MOTOR_CONTROL_STATE_ALIGNING = 1,
    MOTOR_CONTROL_STATE_CURRENT_CONTROL = 2
} MotorControlStartupState;

void MotorControl_init(void);
void MotorControl_runCurrentLoop(void);
bool MotorControl_isReady(void);
const char *MotorControl_getStateName(void);
void MotorControl_setCurrentReference(float dAxisCurrentA, float qAxisCurrentA);
void MotorControl_setPhaseDutyCycles(float dutyA, float dutyB, float dutyC);
void MotorControl_stop(void);

#endif
