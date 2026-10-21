#pragma once

// M5Stack StopWatch（ESP32-S3R8，16MB flash + 8MB OPI PSRAM）
// 引脚来源：docs.m5stack.com/en/core/StopWatch 官方 PinMap +
//           M5StopWatch-UserDemo main/hal/hal_display.cpp
// 与微雪 1.75 同为 CO5300 466×466 QSPI 屏（窗口 offset_x=6，与
// MotoNav.ino 里 Arduino_CO5300 构造参数一致），面板驱动零改动。

#define LCD_SDIO0 41
#define LCD_SDIO1 42
#define LCD_SDIO2 46
#define LCD_SDIO3 45
#define LCD_SCLK 40
#define LCD_CS 39
// OLED 复位不在 ESP32 GPIO 上，挂在 M5IOE1 扩展芯片 PYG5：
// 这里给 -1（Arduino_GFX 跳过自己的复位），复位由 BoardStopWatchInit.h 负责
#define LCD_RESET -1
#define LCD_WIDTH 466
#define LCD_HEIGHT 466

// TOUCH：CST820B，挂系统 I2C（SDA=47 / SCL=48），地址 0x15（MotoNav.ino touchInit 里改）
#define IIC_SDA 47
#define IIC_SCL 48
#define TP_INT 13
// 触摸复位同样在 M5IOE1 PYG4 上，给 -1，由 BoardStopWatchInit.h 复位
#define TP_RESET -1

// 用户按键（官方 hal_button.cpp：A=GPIO2、B=GPIO1，上拉输入，按下为低电平）
#define BTN_A 2
#define BTN_B 1

// 板上没有 AXP2101（电源管理是 M5Stack 私有 M5PM1）；
// MotoNav.ino 的 batInit 已在本板型里短路为 pmuOK=false，防 0x34 误写。
// 本项目不用音频（ES8311）和 SD，相关引脚不再定义。
