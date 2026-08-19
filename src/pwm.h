#ifndef PWM_H
#define PWM_H

void Pwm_init(void);
void Pwm_setPhaseDutyCycles(float dutyA, float dutyB, float dutyC);
float Pwm_getSwitchingFrequencyHz(void);

#endif
