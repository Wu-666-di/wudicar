# wudicar

STM32F103 蓝牙遥控麦轮测距小车。

- 主控：STM32F103C8
- 通信：HC-05 (USART1, 9600)
- 运动：4 路 TB6612 驱动 + 4 麦轮
- 感知：HC-SR04 超声波
- 执行：双舵机云台（水平/竖直）
- 反馈：OLED 显示 + 蓝牙距离回传 + LED + 蜂鸣器

## 项目简介

本项目实现以下功能：

1. 手机通过蓝牙发送摇杆数据，控制小车全向平移与原地旋转。
2. 手机通过蓝牙发送云台摇杆数据，控制水平与竖直舵机。
3. 超声波实时测距，OLED 本地显示距离。
4. 距离小于阈值触发红灯与蜂鸣器报警，距离正常点亮绿灯。
5. 蓝牙链路超时自动停车，避免失控。

主逻辑在 `User/main.c`。

## 硬件连接

### 1) 蓝牙 HC-05

- PA9 (USART1_TX) -> HC-05 RXD
- PA10 (USART1_RX) <- HC-05 TXD
- 波特率：9600

参考：`System/Bluetooth.c`

### 2) 电机 + TB6612 (x2)

PWM 输出（TIM2）：

- PA0 = TIM2_CH1
- PA1 = TIM2_CH2
- PA2 = TIM2_CH3
- PA3 = TIM2_CH4

方向控制：

- 左前 LF：PB12, PB13
- 右前 RF：PB14, PB15
- 左后 LB：PB10, PB11
- 右后 RB：PA4, PA5

说明：TB6612 的 STBY 建议常高使能（接 3.3V）。

参考：`System/Motor.c`, `System/PWM.c`

### 3) 舵机（SG90）

- 水平舵机：PA6 (TIM3_CH1)
- 竖直舵机：PA7 (TIM3_CH2)
- PWM：50Hz（20ms 周期）
- 脉宽：500us~2500us 对应 0~180 度

软件限位：

- 水平：0~180
- 竖直：20~95

参考：`System/Servo.c`

### 4) 超声波 HC-SR04

- Trig：PB0
- Echo：PB1 (EXTI1 双边沿)
- 计时：TIM4，1us 计数

参考：`System/ultrasonic.c`

### 5) 指示灯与蜂鸣器

- 红灯：PB6（低电平点亮）
- 绿灯：PB7（低电平点亮）
- 蜂鸣器：PB5（低电平触发）

参考：`System/LED.c`, `System/Buzzer.c`

## 蓝牙协议

### 1) 下行（手机 -> 小车）

固定 9 字节：

`0xA5 sx sy tx ty left right checksum 0x5A`

字段说明：

- `sx`：水平舵机摇杆，int8，范围 -128~127
- `sy`：竖直舵机摇杆，int8，范围 -128~127
- `tx`：平移 X，int8，范围 -128~127（正值向右）
- `ty`：平移 Y，int8，范围 -128~127（正值向前）
- `left`：左转按键（非 0 视为按下）
- `right`：右转按键（非 0 视为按下）
- `checksum`：当前版本不校验（仅保留字段）

解析策略：

- 校验帧头 `0xA5` 与帧尾 `0x5A`
- 不做 checksum 校验

参考：`System/Bluetooth.c`

### 2) 上行（小车 -> 手机）

固定 4 字节（距离回传）：

`0xA5 dist dist 0x5A`

- `dist`：距离（cm），uint8，0~255
- 第 3 字节当前直接发送 `dist`（作为简化校验位）

## 调参说明

调参入口主要在 `User/main.c` 与 `System/Motor.c`。

### A. 基础运动参数（User/main.c）

- `DEADZONE`：平移摇杆死区
- `SPIN_SPEED`：原地旋转速度
- `BT_TIMEOUT_MS`：蓝牙超时自动停车时间
- `LOOP_MS`：主循环周期

建议：

- 车体抖动：适当增大 `DEADZONE`
- 转向太猛：减小 `SPIN_SPEED`
- 安全性优先：减小 `BT_TIMEOUT_MS`

### B. 漂移补偿参数（User/main.c）

- `FORWARD_TX_COMP_PCT`
  - 作用：前进时叠加少量右移，修正“前进向左偏”
- `STRAFE_TY_COMP_PCT`
  - 作用：横移时叠加少量前进，修正“横移向后偏”

建议步长：每次改 2~4。

### C. 横移自转补偿（System/Motor.c）

- `STRAFE_ANTI_YAW_PCT`
  - 作用：抑制“左移时左转、右移时右转”

现象与方向：

- 若左右横移仍同向自转，增大该值
- 若变成反向自转，减小该值

### D. 竖直舵机中位与范围（User/main.c）

- `SERVO_V_INIT`：上电初始角
- `SERVO_V_LO` / `SERVO_V_HI`：竖直舵机范围（建议保持机械安全范围）
- `SERVO_V_CENTER`：竖直摇杆中位对应角度
- `SERVO_DEADZONE`：竖直摇杆死区，抑制中位抖动

说明：

- 竖直摇杆中位 (`sy=0`) 对应 `SERVO_V_CENTER`
- 当前映射会保持在 `[SERVO_V_LO, SERVO_V_HI]`

## 编译与烧录

使用 Keil 打开工程：

- `Project.uvprojx`

建议流程：

1. 选择目标芯片与下载器。
2. Rebuild 工程。
3. 下载到板子。
4. 上电后先检查蓝牙连接、舵机初始角和超声波回传。

## 目录简述

- `User/`：主程序与中断入口
- `System/`：业务模块（电机、蓝牙、舵机、超声波、OLED、LED、蜂鸣器）
- `Library/`：STM32 标准外设库
- `Start/`：启动文件与系统时钟

## 备注

- 当前电机 PWM 频率为 20kHz，占空比分辨率 0~100。
- 如果后续需要更细速度控制，可把 PWM 分辨率提升到 0~1000 并同步修改调速接口。
