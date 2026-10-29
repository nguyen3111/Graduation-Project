/**
 * @file    can_handler.h
 */
#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#include "vehicle_types.h"
#include "cmsis_os.h"

void CANHandler_Init(void);
void Task_CANTransmit(void *argument);
void Task_CANReceive(void *argument);

#endif /* CAN_HANDLER_H */
