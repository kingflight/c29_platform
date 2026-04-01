#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

void CurrentSensor_init(void);
__attribute__((interrupt("INT"))) void CurrentSensor_adcIsr(void);

#endif
