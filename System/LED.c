#include "stm32f10x.h"
#include "LED.h"

/*
 * LED 模块
 *   红 LED = PB6 (低电平点亮)
 *   绿 LED = PB7 (低电平点亮)
 * 如果你的 LED 模块是共阴（高电平点亮），把下面的 Set/Reset 对调即可。
 */

void LED_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef g;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);

    /* 默认全灭 */
    GPIO_SetBits(GPIOB, GPIO_Pin_6 | GPIO_Pin_7);
}

void LED_Red_On(void)    { GPIO_ResetBits(GPIOB, GPIO_Pin_6); }
void LED_Red_Off(void)   { GPIO_SetBits  (GPIOB, GPIO_Pin_6); }
void LED_Green_On(void)  { GPIO_ResetBits(GPIOB, GPIO_Pin_7); }
void LED_Green_Off(void) { GPIO_SetBits  (GPIOB, GPIO_Pin_7); }

