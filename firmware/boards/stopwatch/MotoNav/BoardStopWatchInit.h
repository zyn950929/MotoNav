#pragma once

// M5Stack StopWatch 板级初始化垫片
// 依据官方 M5StopWatch-UserDemo（MIT）main/hal/hal_ioe.cpp 移植：
// OLED 复位(PYG5)、触摸复位(PYG4)、屏供电 3V3_L3B(PYG8)、USB/UART mux(PYG1)
// 都挂在 M5IOE1 I2C 扩展芯片（0x4F，备用 0x6F）上，必须在 gfx->begin()/touchInit()
// 之前完成上电和复位，否则屏和触摸都摸不到。
// M5IOE1 库以 vendor 方式放在 firmware/libraries/M5IOE1（v1.0.8，与 UserDemo 一致）。
#include <Arduino.h>
#include <Wire.h>
#include <M5IOE1.h>
#include <M5PM1.h>

class StopWatchBoard {
public:
  static M5IOE1 ioe;
  static M5PM1 pm1;
  static bool ok;
  static bool pm1OK;

  // 在 setup() 的 Wire.begin(IIC_SDA, IIC_SCL) 之后立刻调用
  static void init() {
    // M5PM1 电源管理（0x6E）：照官方 UserDemo hal_pmic.cpp 的初始化序列。
    // 关键项：wdtSet(0) 关看门狗（默认开着，不关可能被它复位）、使能充电、
    // PMG3 推挽输出低（CHG_PROG）、PMG2 输入（CHG_STAT 充电状态脚）。
    pm1OK = (pm1.begin(&Wire, 0x6E, IIC_SDA, IIC_SCL, 400000) == M5PM1_OK);
    if (pm1OK) {
      pm1.setI2cSleepTime(0);
      pm1.wdtSet(0);
      pm1.setChargeEnable(true);
      pm1.gpioSet(M5PM1_GPIO_NUM_3, M5PM1_GPIO_MODE_OUTPUT, 0,
                  M5PM1_GPIO_PULL_NONE, M5PM1_GPIO_DRIVE_PUSHPULL);
      pm1.gpioSetFunc(M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_GPIO);
      pm1.gpioSetMode(M5PM1_GPIO_NUM_2, M5PM1_GPIO_MODE_INPUT);
      pm1.gpioSetPull(M5PM1_GPIO_NUM_2, M5PM1_GPIO_PULL_NONE);
    }
    // M5IOE1 扩展芯片：官方顺序：先试 0x4F，失败换 0x6F
    ok = (ioe.begin(&Wire, 0x4F, IIC_SDA, IIC_SCL, 400000) == M5IOE1_OK) ||
         (ioe.begin(&Wire, 0x6F, IIC_SDA, IIC_SCL, 400000) == M5IOE1_OK);
    if (!ok) {
      Serial.println("M5IOE1 not found (screen/touch power unavailable)");
      return;
    }
    ioe.setI2cSleepTime(0);
    // 只碰本项目需要的脚；马达(PIN_9)/功放(PIN_10)不动
    ioe.pinMode(M5IOE1_PIN_8, OUTPUT);   // L3B 屏供电
    ioe.pinMode(M5IOE1_PIN_5, OUTPUT);   // OLED_RST
    ioe.pinMode(M5IOE1_PIN_4, OUTPUT);   // TP_RST
    ioe.pinMode(M5IOE1_PIN_1, OUTPUT);   // USB/UART mux
    ioe.pinMode(M5IOE1_PIN_3, OUTPUT);   // 音频使能（本项目不用音频）
    ioe.digitalWrite(M5IOE1_PIN_3, 0);
    // mux 保持官方默认档 0：UserDemo 即置 0，USB-C 串口/烧录不受影响
    // （mux 只影响尾部扩展口的 USB/UART 复用方向）
    ioe.digitalWrite(M5IOE1_PIN_1, 0);
    // 屏电源使能：官方要求回读确认，失败每 80ms 重试（UserDemo 专门写的循环）
    for (int i = 0; i < 20; i++) {
      ioe.digitalWrite(M5IOE1_PIN_8, 1);
      delay(80);
      if (ioe.digitalRead(M5IOE1_PIN_8) == 1) break;
    }
    // OLED 硬复位：参考 185b 板型的时序（低 20ms → 高 120ms）
    ioe.digitalWrite(M5IOE1_PIN_5, 0);
    delay(20);
    ioe.digitalWrite(M5IOE1_PIN_5, 1);
    delay(120);
    // 释放触摸复位；触摸的完整复位时序在 tpReset()（touchInit 里还会做一次）
    ioe.digitalWrite(M5IOE1_PIN_4, 1);
    // 官方在 ioe_init 之后 delay(50) 再初始化屏
    delay(50);
  }

  // 触摸硬复位（官方时序：低 10ms → 高 50ms），touchInit() 里调用
  static void tpReset() {
    if (!ok) return;
    ioe.digitalWrite(M5IOE1_PIN_4, 0);
    delay(10);
    ioe.digitalWrite(M5IOE1_PIN_4, 1);
    delay(50);
  }
};

M5IOE1 StopWatchBoard::ioe;
M5PM1 StopWatchBoard::pm1;
bool StopWatchBoard::ok = false;
bool StopWatchBoard::pm1OK = false;
