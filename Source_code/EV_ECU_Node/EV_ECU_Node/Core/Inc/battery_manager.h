/**
 * @file    battery_manager.h
 */
#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include "vehicle_types.h"
#include "cmsis_os.h"

void BatteryManager_Init(void);
void Task_BatteryManager(void *argument);

#endif
