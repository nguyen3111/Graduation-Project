/**
 * @file    motor_control.h
 * @brief   Motor control — PWM output + encoder feedback + speed calculation
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "vehicle_types.h"
#include "cmsis_os.h"

void MotorControl_Init(void);
void Task_MotorControl(void *argument);

#endif /* MOTOR_CONTROL_H */
