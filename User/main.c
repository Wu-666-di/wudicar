#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Motor.h"
#include "Servo.h"
#include "Ultrasonic.h"
#include "Bluetooth.h"
#include "LED.h"
#include "Buzzer.h"

/*
 * 蓝牙 + 超声波 麦轮小车主程序
 *
 * 功能:
 *   1. 通过 HC-05 接收手机摇杆数据 (tx,ty 平移;  左转/右转按钮旋转)
 *   2. 通过 HC-05 接收舵机角度 (sx,sy -> 水平/垂直舵机)
 *   3. HC-SR04 实时测距, 结果在 OLED 显示并通过蓝牙回传
 *   4. 距离 < 15 cm: 蜂鸣器响 + 红 LED 亮
 *      距离 >= 15 cm: 绿 LED 亮
 *   5. 超过 500 ms 未收到新数据帧 -> 小车停车 (失控保护)
 */

#define DEADZONE        10      /* 摇杆死区 */
#define SPIN_SPEED      60      /* 原地转速 0~100 */
#define DIST_THRESHOLD  15      /* 报警阈值 cm */
#define BT_TIMEOUT_MS   500     /* 失控超时 */
#define LOOP_MS         20      /* 主循环周期 */
#define SERVO_V_INIT    80      /* 竖直舵机上电初始角 */
#define SERVO_V_LO      20      /* 竖直舵机下限角 */
#define SERVO_V_HI      95      /* 竖直舵机上限角 */
#define SERVO_V_CENTER  80      /* 竖直舵机摇杆中位角 */
#define SERVO_DEADZONE   8      /* 舵机摇杆死区 */

/* 运动补偿（按百分比，调参用）
 * FORWARD_TX_COMP_PCT: 前进时补一点向右，抑制“前进向左偏”
 * STRAFE_TY_COMP_PCT : 横移时补一点向前，抑制“横移向后偏”
 */
#define FORWARD_TX_COMP_PCT  16
#define STRAFE_TY_COMP_PCT   6

/* 将 -128~127 线性映射到 -100~100 */
static int ScaleJoy(int8_t v)
{
    int r = (int)v * 100 / 128;
    if (r >  100) r =  100;
    if (r < -100) r = -100;
    return r;
}

/* 将 -128~127 映射到 [lo, hi] */
static int MapServo(int8_t v, int lo, int hi)
{
    /* (v + 128) * (hi - lo) / 255 + lo */
    int r = ((int)v + 128) * (hi - lo) / 255 + lo;
    if (r < lo) r = lo;
    if (r > hi) r = hi;
    return r;
}

static int Clamp100(int v)
{
    if (v > 100) return 100;
    if (v < -100) return -100;
    return v;
}

/* 将 -128~127 映射到 [lo, hi]，并保证摇杆中位(0)对应 center */
static int MapServoCenter(int8_t v, int lo, int center, int hi)
{
    int r;

    if (center < lo) center = lo;
    if (center > hi) center = hi;

    if (v >= 0)
    {
        r = center + (int)v * (hi - center) / 127;
    }
    else
    {
        r = center + (int)v * (center - lo) / 128;
    }

    if (r < lo) r = lo;
    if (r > hi) r = hi;
    return r;
}

int main(void)
{
    uint32_t loop_ms         = 0;
    uint32_t last_rx_ms      = 0;
    uint32_t last_rx_counter = 0;
    uint32_t last_us_ms      = 0;
    uint16_t distance        = 0;
    uint8_t  dist_for_tx     = 0;

    /* 中断优先级分组 */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* 外设初始化 */
    OLED_Init();
    LED_Init();
    Buzzer_Init();
    Motor_Init();
    Servo_Init();
    Ultrasonic_Init();
    Bluetooth_Init();

    /* 舵机初始位置 */
    Servo_SetH(90);
    Servo_SetV(SERVO_V_INIT);

    /* OLED 固定显示 */
    OLED_ShowString(1, 1, "Mecanum Car");
    OLED_ShowString(2, 1, "Dist:       cm");
    OLED_ShowString(3, 1, "TX:     TY:    ");
    OLED_ShowString(4, 1, "BT:             ");

    while (1)
    {
        /* ---- 1. 检测新数据帧并更新失控时间戳 ---- */
        uint32_t cur_rx_counter = g_bt_rx_counter;
        if (cur_rx_counter != last_rx_counter)
        {
            last_rx_counter = cur_rx_counter;
            last_rx_ms      = loop_ms;
        }

        /* 拷贝一份本地副本避免被中断改动 */
        BT_Data d = g_bt_data;

        /* 必须先收到至少一帧蓝牙数据，才认为在线，避免覆盖舵机上电初始角 */
        uint8_t bt_alive = (last_rx_counter > 0) && ((loop_ms - last_rx_ms) < BT_TIMEOUT_MS);

        /* ---- 2. 运动控制 ---- */
        if (!bt_alive)
        {
            Motor_Stop();
        }
        else if (d.turn_left && !d.turn_right)
        {
            Motor_Rotate(-1, SPIN_SPEED);
        }
        else if (d.turn_right && !d.turn_left)
        {
            Motor_Rotate(+1, SPIN_SPEED);
        }
        else
        {
            int tx = ScaleJoy(d.tx);
            int ty = ScaleJoy(d.ty);

            if (tx > -DEADZONE && tx < DEADZONE) tx = 0;
            if (ty > -DEADZONE && ty < DEADZONE) ty = 0;

            /* 前进左偏补偿：仅对前进(ty>0)叠加少量右移分量 */
            if (ty > 0)
            {
                tx += ty * FORWARD_TX_COMP_PCT / 100;
            }

            /* 横移后偏补偿：左右横移都叠加少量前进分量 */
            if (tx != 0)
            {
                int abs_tx = (tx >= 0) ? tx : -tx;
                ty += abs_tx * STRAFE_TY_COMP_PCT / 100;
            }

            tx = Clamp100(tx);
            ty = Clamp100(ty);
            Motor_SetMecanum(tx, ty);
        }

        /* ---- 3. 舵机控制 ---- */
        if (bt_alive)
        {
            int8_t sy = d.sy;
            Servo_SetH(MapServo(d.sx,  0, 180));
            if (sy > -SERVO_DEADZONE && sy < SERVO_DEADZONE) sy = 0;
            Servo_SetV(MapServoCenter(sy, SERVO_V_LO, SERVO_V_CENTER, SERVO_V_HI));
        }

        /* ---- 4. 超声波 + LED + 蜂鸣器（100ms 一次） ---- */
        if ((loop_ms - last_us_ms) >= 100)
        {
            last_us_ms = loop_ms;
            distance = Ultrasonic_Measure();

            if (distance == ULTRASONIC_TIMEOUT)
            {
                /* 测距失败时不报警，按安全距离处理 */
                LED_Red_Off();
                LED_Green_On();
                Buzzer_OFF();
                dist_for_tx = 0;
            }
            else
            {
                dist_for_tx = (distance > 255) ? 255 : (uint8_t)distance;

                if (distance < DIST_THRESHOLD)
                {
                    LED_Red_On();
                    LED_Green_Off();
                    Buzzer_ON();
                }
                else
                {
                    LED_Red_Off();
                    LED_Green_On();
                    Buzzer_OFF();
                }
            }

            /* 蓝牙回传距离: 0xA5 dist checksum 0x5A */
            Bluetooth_SendDistance(dist_for_tx);

            /* OLED 显示距离 */
            if (distance == ULTRASONIC_TIMEOUT)
            {
                OLED_ShowString(2, 7, "---");
            }
            else
            {
                OLED_ShowNum(2, 7, distance, 3);
            }
        }

        /* ---- 5. OLED 显示摇杆 / 状态 ---- */
        OLED_ShowSignedNum(3, 4, ScaleJoy(d.tx), 3);
        OLED_ShowSignedNum(3, 12, ScaleJoy(d.ty), 3);
        OLED_ShowString(4, 4, bt_alive ? "ONLINE " : "TIMEOUT");

        /* ---- 6. 循环节拍 ---- */
        Delay_ms(LOOP_MS);
        loop_ms += LOOP_MS;
    }
}


