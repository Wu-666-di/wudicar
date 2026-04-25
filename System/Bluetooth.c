#include "stm32f10x.h"
#include "Bluetooth.h"

/*
 * HC-05 蓝牙模块（USART1）
 *   STM32 PA9  (TX) -> HC-05 RXD
 *   STM32 PA10 (RX) -> HC-05 TXD
 *   默认波特率 9600
 *
 * 接收协议（9 字节）:
 *   0xA5  sx  sy  tx  ty  left  right  checksum  0x5A
 *   按用户要求不校验 checksum，只校验包头 0xA5 和包尾 0x5A。
 *
 * 发送协议（4 字节，回传距离 cm，单字节 0~255）：
 *   0xA5  dist  (dist & 0xFF)  0x5A
 */

#define FRAME_LEN  9

volatile BT_Data  g_bt_data = {0, 0, 0, 0, 0, 0};
volatile uint32_t g_bt_rx_counter = 0;

static uint8_t rx_buf[FRAME_LEN];
static uint8_t rx_idx   = 0;
static uint8_t rx_state = 0;   /* 0=等包头, 1=收数据 */

void Bluetooth_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef g;

    /* PA9 = USART1_TX 复用推挽 */
    g.GPIO_Mode  = GPIO_Mode_AF_PP;
    g.GPIO_Pin   = GPIO_Pin_9;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &g);

    /* PA10 = USART1_RX 浮空输入 */
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    g.GPIO_Pin  = GPIO_Pin_10;
    GPIO_Init(GPIOA, &g);

    USART_InitTypeDef u;
    u.USART_BaudRate            = 9600;
    u.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    u.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    u.USART_Parity              = USART_Parity_No;
    u.USART_StopBits            = USART_StopBits_1;
    u.USART_WordLength          = USART_WordLength_8b;
    USART_Init(USART1, &u);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    NVIC_InitTypeDef n;
    n.NVIC_IRQChannel                   = USART1_IRQn;
    n.NVIC_IRQChannelPreemptionPriority = 1;
    n.NVIC_IRQChannelSubPriority        = 0;
    n.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&n);

    USART_Cmd(USART1, ENABLE);
}

void Bluetooth_SendByte(uint8_t b)
{
    USART_SendData(USART1, b);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

void Bluetooth_SendDistance(uint8_t dist_cm)
{
    Bluetooth_SendByte(0xA5);
    Bluetooth_SendByte(dist_cm);
    Bluetooth_SendByte(dist_cm);   /* checksum = 距离的低 8 位 */
    Bluetooth_SendByte(0x5A);
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t b = (uint8_t)USART_ReceiveData(USART1);

        if (rx_state == 0)
        {
            if (b == 0xA5)
            {
                rx_buf[0] = b;
                rx_idx    = 1;
                rx_state  = 1;
            }
        }
        else
        {
            rx_buf[rx_idx++] = b;
            if (rx_idx >= FRAME_LEN)
            {
                rx_state = 0;
                if (rx_buf[FRAME_LEN - 1] == 0x5A)
                {
                    /* 按用户要求不做 checksum 校验 */
                    g_bt_data.sx         = (int8_t)rx_buf[1];
                    g_bt_data.sy         = (int8_t)rx_buf[2];
                    g_bt_data.tx         = (int8_t)rx_buf[3];
                    g_bt_data.ty         = (int8_t)rx_buf[4];
                    g_bt_data.turn_left  = rx_buf[5];
                    g_bt_data.turn_right = rx_buf[6];
                    g_bt_rx_counter++;
                }
                /* 否则丢弃该帧，等下一个包头 */
            }
        }
        /* 注意：USART_IT_RXNE 由读 DR 自动清零，无需手动 */
    }
}



