#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f10x.h"

void Servo_Init(void);
void Servo_SetH(int angle);   /* 水平舵机 PA6, 范围 0~180 */
void Servo_SetV(int angle);   /* 垂直舵机 PA7, 范围 20~95 */

#endif
