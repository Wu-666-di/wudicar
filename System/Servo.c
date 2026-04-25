#include "stm32f10x.h"
#include "Servo.h"

/*
 * SG90 舵机驱动
 *   TIM3_CH1 = PA6 = 水平舵机
 *   TIM3_CH2 = PA7 = 垂直舵机
 *
 * 72MHz / 72 / 20000 = 50Hz (周期 20ms)
 * 计数单位 1us，脉宽 500~2500us 对应 0~180°
 */

void Servo_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef g;
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);

    TIM_InternalClockConfig(TIM3);

    TIM_TimeBaseInitTypeDef t;
    t.TIM_ClockDivision     = TIM_CKD_DIV1;
    t.TIM_CounterMode       = TIM_CounterMode_Up;
    t.TIM_Period            = 20000 - 1;
    t.TIM_Prescaler         = 72 - 1;
    t.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &t);

    TIM_OCInitTypeDef o;
    TIM_OCStructInit(&o);
    o.TIM_OCMode      = TIM_OCMode_PWM1;
    o.TIM_OCPolarity  = TIM_OCPolarity_High;
    o.TIM_OutputState = TIM_OutputState_Enable;
    o.TIM_Pulse       = 1500;  /* 中位 90° */
    TIM_OC1Init(TIM3, &o);
    TIM_OC2Init(TIM3, &o);

    TIM_Cmd(TIM3, ENABLE);
}

static uint16_t Servo_AngleToPulse(int angle)
{
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;
    /* 0° -> 500us, 180° -> 2500us */
    return (uint16_t)(500 + angle * 2000 / 180);
}

void Servo_SetH(int angle)
{
    if (angle < 0)   angle = 0;
    if (angle > 180) angle = 180;
    TIM_SetCompare1(TIM3, Servo_AngleToPulse(angle));
}

void Servo_SetV(int angle)
{
    if (angle < 20) angle = 20;
    if (angle > 95) angle = 95;
    TIM_SetCompare2(TIM3, Servo_AngleToPulse(angle));
}


