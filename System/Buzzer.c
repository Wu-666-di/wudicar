#include "stm32f10x.h"
#include "Buzzer.h"

/*
 * 蜂鸣器模块（有源低电平触发）
 *   引脚 = PB5
 * 如果你的模块是高电平触发，把 ON/OFF 的 Set/Reset 对调。
 */

void Buzzer_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef g;
    g.GPIO_Mode  = GPIO_Mode_Out_PP;
    g.GPIO_Pin   = GPIO_Pin_5;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &g);

    GPIO_SetBits(GPIOB, GPIO_Pin_5);   /* 默认关闭 */
}

void Buzzer_ON(void)  { GPIO_ResetBits(GPIOB, GPIO_Pin_5); }
void Buzzer_OFF(void) { GPIO_SetBits  (GPIOB, GPIO_Pin_5); }

