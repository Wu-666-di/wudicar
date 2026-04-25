#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

void Motor_Init(void);

/**
 * 麦轮全向平移控制
 * @param tx 横向速度，+向右，范围 -100 ~ 100
 * @param ty 纵向速度，+向前，范围 -100 ~ 100
 */
void Motor_SetMecanum(int tx, int ty);

/**
 * 原地旋转
 * @param dir  +1 = 顺时针(右转)，-1 = 逆时针(左转)，0 = 停
 * @param speed 速度 0 ~ 100
 */
void Motor_Rotate(int dir, int speed);

void Motor_Stop(void);

#endif


