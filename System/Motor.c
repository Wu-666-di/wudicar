#include "stm32f10x.h"
#include "PWM.h"
#include "Motor.h"

/* 横移抗自转补偿（百分比）
 * 现象: 左移会左转、右移会右转
 * 做法: 按 tx 反向叠加一个旋转分量 wz，抵消横移时的自转
 */
#define STRAFE_ANTI_YAW_PCT  5

/*
 * 电机编号与引脚（TB6612 × 2）
 *   0 = LF 左前: PWM=TIM2_CH1(PA0), IN1=PB12, IN2=PB13
 *   1 = RF 右前: PWM=TIM2_CH2(PA1), IN1=PB14, IN2=PB15
 *   2 = LB 左后: PWM=TIM2_CH3(PA2), IN1=PB10, IN2=PB11
 *   3 = RB 右后: PWM=TIM2_CH4(PA3), IN1=PA4,  IN2=PA5
 *
 * TB6612 STBY 引脚请直接接 3.3V 常开。
 *
 * 麦轮布局（俯视，X 型，辊子朝向按用户提供的图）：
 *       LF(0)          RF(1)
 *         \\            //
 *          \\          //
 *         //            \\
 *       LB(2)          RB(3)
 *
 * 正向运动解算（vy 前进, vx 右移）：
 *   vLF = vy + vx
 *   vRF = vy - vx
 *   vLB = vy - vx
 *   vRB = vy + vx
 *
 * 顺时针旋转（车头向右打）：
 *   LF +, RF -, LB +, RB -
 */

void Motor_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef g;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;

    /* PB10,11,12,13,14,15 方向控制 */
    g.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 |
                 GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOB, &g);

    /* PA4, PA5 方向控制 */
    g.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_Init(GPIOA, &g);

    PWM_Init();

    /* 初始全部停止 */
    GPIO_ResetBits(GPIOB, GPIO_Pin_10 | GPIO_Pin_11 | GPIO_Pin_12 |
                          GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
    GPIO_ResetBits(GPIOA, GPIO_Pin_4 | GPIO_Pin_5);
}

/* 给单个电机设置速度（-100~100），内部做方向 + PWM */
static void Motor_SetOne(int idx, int speed)
{
    if (speed >  100) speed =  100;
    if (speed < -100) speed = -100;

    uint16_t duty = (speed >= 0) ? speed : -speed;

    switch (idx)
    {
        case 0: /* LF */
            if (speed >= 0) { GPIO_SetBits(GPIOB, GPIO_Pin_12);   GPIO_ResetBits(GPIOB, GPIO_Pin_13); }
            else            { GPIO_ResetBits(GPIOB, GPIO_Pin_12); GPIO_SetBits(GPIOB, GPIO_Pin_13);   }
            PWM_SetCompare2(duty);
            break;

        case 1: /* RF */
            if (speed >= 0) { GPIO_SetBits(GPIOB, GPIO_Pin_14);   GPIO_ResetBits(GPIOB, GPIO_Pin_15); }
            else            { GPIO_ResetBits(GPIOB, GPIO_Pin_14); GPIO_SetBits(GPIOB, GPIO_Pin_15);   }
            PWM_SetCompare1(duty);
            break;

        case 2: /* LB */
            if (speed >= 0) { GPIO_SetBits(GPIOB, GPIO_Pin_10);   GPIO_ResetBits(GPIOB, GPIO_Pin_11); }
            else            { GPIO_ResetBits(GPIOB, GPIO_Pin_10); GPIO_SetBits(GPIOB, GPIO_Pin_11);   }
            PWM_SetCompare3(duty);
            break;

        case 3: /* RB */
            if (speed >= 0) { GPIO_SetBits(GPIOA, GPIO_Pin_4);    GPIO_ResetBits(GPIOA, GPIO_Pin_5);  }
            else            { GPIO_ResetBits(GPIOA, GPIO_Pin_4);  GPIO_SetBits(GPIOA, GPIO_Pin_5);    }
            PWM_SetCompare4(duty);
            break;
    }
}

void Motor_SetMecanum(int tx, int ty)
{
    int wz = -tx * STRAFE_ANTI_YAW_PCT / 100;

    int lf = ty + tx + wz;
    int rf = ty - tx - wz;
    int lb = ty - tx + wz;
    int rb = ty + tx - wz;

    /* 若任一轮子超 100 则等比缩放，保持运动方向不失真 */
    int m = 0;
    int v;
    v = lf; if (v < 0) v = -v; if (v > m) m = v;
    v = rf; if (v < 0) v = -v; if (v > m) m = v;
    v = lb; if (v < 0) v = -v; if (v > m) m = v;
    v = rb; if (v < 0) v = -v; if (v > m) m = v;
    if (m > 100)
    {
        lf = lf * 100 / m;
        rf = rf * 100 / m;
        lb = lb * 100 / m;
        rb = rb * 100 / m;
    }

    Motor_SetOne(0, lf);
    Motor_SetOne(1, rf);
    Motor_SetOne(2, lb);
    Motor_SetOne(3, rb);
}

void Motor_Rotate(int dir, int speed)
{
    if (speed < 0) speed = -speed;
    if (speed > 100) speed = 100;

    if (dir > 0)            /* 顺时针 右转 */
    {
        Motor_SetOne(0,  speed);
        Motor_SetOne(1, -speed);
        Motor_SetOne(2,  speed);
        Motor_SetOne(3, -speed);
    }
    else if (dir < 0)       /* 逆时针 左转 */
    {
        Motor_SetOne(0, -speed);
        Motor_SetOne(1,  speed);
        Motor_SetOne(2, -speed);
        Motor_SetOne(3,  speed);
    }
    else
    {
        Motor_Stop();
    }
}

void Motor_Stop(void)
{
    Motor_SetOne(0, 0);
    Motor_SetOne(1, 0);
    Motor_SetOne(2, 0);
    Motor_SetOne(3, 0);
}

