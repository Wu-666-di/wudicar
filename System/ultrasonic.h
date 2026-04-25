#ifndef __ULTRASONIC_H
#define __ULTRASONIC_H

#include "stm32f10x.h"

#define ULTRASONIC_TIMEOUT  0xFFFF

void Ultrasonic_Init(void);

/**
 * 触发一次测距并返回距离，单位 cm
 * 返回 ULTRASONIC_TIMEOUT (0xFFFF) 表示超时/无回波
 */
uint16_t Ultrasonic_Measure(void);

#endif

