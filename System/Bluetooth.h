#ifndef __BLUETOOTH_H
#define __BLUETOOTH_H

#include "stm32f10x.h"

typedef struct
{
    int8_t  sx;          /* 水平舵机 -128~127 */
    int8_t  sy;          /* 垂直舵机 -128~127 */
    int8_t  tx;          /* 横向速度 -128~127 */
    int8_t  ty;          /* 纵向速度 -128~127 */
    uint8_t turn_left;   /* 1=左转 */
    uint8_t turn_right;  /* 1=右转 */
} BT_Data;

extern volatile BT_Data g_bt_data;
extern volatile uint32_t g_bt_rx_counter;  /* 每收到一帧有效数据就自增 */

void Bluetooth_Init(void);
void Bluetooth_SendByte(uint8_t b);
void Bluetooth_SendDistance(uint8_t dist_cm);

#endif

