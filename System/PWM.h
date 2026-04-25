#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"

void PWM_Init(void);
void PWM_SetCompare1(uint16_t Compare);  // PA0 左前
void PWM_SetCompare2(uint16_t Compare);  // PA1 右前
void PWM_SetCompare3(uint16_t Compare);  // PA2 左后
void PWM_SetCompare4(uint16_t Compare);  // PA3 右后

#endif

