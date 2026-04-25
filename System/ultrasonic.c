#include "stm32f10x.h"
#include "Delay.h"
#include "Ultrasonic.h"

/*
 * HC-SR04 超声波模块
 *   Trig = PB0 (输出)
 *   Echo = PB1 (输入 + EXTI1 双边沿)
 *   TIM4 做 1us 计时基准（独立于 TIM2 PWM 和 TIM3 舵机）
 */

#define TRIG_PORT  GPIOB
#define TRIG_PIN   GPIO_Pin_0
#define ECHO_PORT  GPIOB
#define ECHO_PIN   GPIO_Pin_1

static volatile uint16_t echo_start = 0;
static volatile uint16_t echo_end   = 0;
static volatile uint8_t  echo_flag  = 0;

void Ultrasonic_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    GPIO_InitTypeDef g;
    g.GPIO_Speed = GPIO_Speed_50MHz;

    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Pin  = TRIG_PIN;
    GPIO_Init(TRIG_PORT, &g);
    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);

    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin  = ECHO_PIN;
    GPIO_Init(ECHO_PORT, &g);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);

    EXTI_InitTypeDef e;
    e.EXTI_Line    = EXTI_Line1;
    e.EXTI_Mode    = EXTI_Mode_Interrupt;
    e.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    e.EXTI_LineCmd = ENABLE;
    EXTI_Init(&e);

    NVIC_InitTypeDef n;
    n.NVIC_IRQChannel                   = EXTI1_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 2;
    n.NVIC_IRQChannelSubPriority        = 0;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);

    /* TIM4: 1us 计数 */
    TIM_TimeBaseInitTypeDef t;
    t.TIM_ClockDivision     = TIM_CKD_DIV1;
    t.TIM_CounterMode       = TIM_CounterMode_Up;
    t.TIM_Period            = 0xFFFF;
    t.TIM_Prescaler         = 72 - 1;
    t.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &t);
    TIM_Cmd(TIM4, ENABLE);
}

uint16_t Ultrasonic_Measure(void)
{
    uint32_t timeout;
    uint16_t dur;
    uint32_t dist;

    echo_flag = 0;

    /* 触发 10us 高电平 */
    GPIO_SetBits(TRIG_PORT, TRIG_PIN);
    Delay_us(15);
    GPIO_ResetBits(TRIG_PORT, TRIG_PIN);

    /* 等待 EXTI 捕获完成，最多 ~40ms */
    timeout = 40000;
    while (!echo_flag && timeout--)
    {
        Delay_us(1);
    }

    if (!echo_flag) return ULTRASONIC_TIMEOUT;

    dur = (uint16_t)(echo_end - echo_start);   /* us */
    /* distance(cm) = dur(us) * 0.034 / 2 = dur * 17 / 1000 */
    dist = (uint32_t)dur * 17 / 1000;

    if (dist == 0 || dist > 500) return ULTRASONIC_TIMEOUT;
    return (uint16_t)dist;
}

void EXTI1_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line1) != RESET)
    {
        if (GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN) == Bit_SET)
        {
            echo_start = TIM_GetCounter(TIM4);
        }
        else
        {
            echo_end  = TIM_GetCounter(TIM4);
            echo_flag = 1;
        }
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

