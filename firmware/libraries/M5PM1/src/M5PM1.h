/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file M5PM1.h
 * @brief M5Stack PM1 电源管理IC驱动库（双平台：ESP-IDF 和 Arduino）
 *        M5Stack PM1 Power Management IC Driver Library (Dual-Platform: ESP-IDF & Arduino)
 *
 * @note PM1 是一款多功能电源管理IC，支持：
 *       PM1 is a multi-function power management IC supporting:
 *       - 电池充电和监控 / Battery charging and monitoring
 *       - 多路电源轨（DCDC 5V，LDO 3.3V, BOOST 5V） / Multiple power rails (DCDC 5V, LDO 3.3V, BOOST 5V)
 *       - 5个GPIO引脚支持多种功能 / 5 GPIO pins with various functions
 *       - PWM输出、ADC输入 / PWM output, ADC input
 *       - NeoPixel LED控制 / NeoPixel LED control
 *       - 看门狗定时器 / Watchdog timer
 *       - RTC RAM（32字节，睡眠保持） / RTC RAM (32 bytes, retained in sleep)
 *       - I2C自动睡眠/唤醒功能 / I2C auto-sleep/wake feature
 */

#ifndef _M5PM1_H_
#define _M5PM1_H_

#include "M5PM1_i2c_compat.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef ARDUINO
// Arduino: FreeRTOS headers included via Arduino framework
#else
// ESP-IDF specific includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#endif

// ============================
// 错误码
// Error Codes
// ============================
typedef enum {
    M5PM1_OK = 0,                   // 成功
                                    // Success
    M5PM1_FAIL = -1,                // 一般失败
                                    // General failure
    M5PM1_ERR_I2C_CONFIG = -2,      // I2C 配置错误 (如频率不支持)
                                    // I2C configuration error
    M5PM1_ERR_RULE_VIOLATION = -3,  // 条件规则错误 (如引脚冲突，互斥功能)
                                    // Condition rule violation
    M5PM1_ERR_INVALID_ARG = -4,     // 无效参数
                                    // Invalid argument
    M5PM1_ERR_TIMEOUT = -5,         // 超时
                                    // Timeout
    M5PM1_ERR_NOT_SUPPORTED = -6,   // 不支持的功能
                                    // Function not supported
    M5PM1_ERR_I2C_COMM = -7,        // I2C 通信错误
                                    // I2C communication error
    M5PM1_ERR_NOT_INIT = -8,        // 设备未初始化
                                    // Device not initialized
    M5PM1_ERR_INTERNAL = -9,        // 内部错误
                                    // Internal error
    M5PM1_ERR_VERIFY_FAILED = -10,  // 配置校验失败（读回值与缓存不匹配）
                                    // Configuration verification failed
} m5pm1_err_t;

// ============================
// 设备常量
// Device Constants
// ============================
#define M5PM1_DEFAULT_ADDR     0x6E  // 默认I2C地址 / Default I2C address
#define M5PM1_MAX_GPIO_PINS    5     // GPIO0-GPIO4，共5个引脚 / GPIO0-GPIO4, total 5 pins
#define M5PM1_MAX_PWM_CHANNELS 2     // PWM0（GPIO3），PWM1（GPIO4） / PWM0 (GPIO3), PWM1 (GPIO4)
#define M5PM1_MAX_ADC_CHANNELS 3     // ADC1（GPIO1），ADC2（GPIO2），温度 / ADC1 (GPIO1), ADC2 (GPIO2), TEMP
#define M5PM1_MAX_LED_COUNT    32    // 最大NeoPixel LED数量 / Maximum NeoPixel LED count
#define M5PM1_RTC_RAM_SIZE     32    // RTC RAM大小（睡眠保持） / RTC RAM size in bytes (retained in sleep)

// ============================
// I2C 频率常量
// I2C Frequency Constants
// ============================
#define M5PM1_I2C_FREQ_100K    100000  // 标准模式 / Standard mode
#define M5PM1_I2C_FREQ_400K    400000  // 快速模式 / Fast mode
#define M5PM1_I2C_FREQ_DEFAULT M5PM1_I2C_FREQ_100K
// I2C 重试参数
// I2C retry settings
#define M5PM1_I2C_RETRY_COUNT    2
#define M5PM1_I2C_RETRY_DELAY_MS 50

// ============================
// 寄存器地址
// Register Addresses
// ============================

// ---- 系统寄存器 ----
// ---- System Registers ----
#define M5PM1_REG_DEVICE_ID    0x00  // R     [7:0] 设备ID / Device ID
#define M5PM1_REG_DEVICE_MODEL 0x01  // R     [7:0] 设备型号 / Device Model
#define M5PM1_REG_HW_REV       0x02  // R     [7:0] 硬件版本 / Hardware version
#define M5PM1_REG_SW_REV       0x03  // R     [7:0] 固件版本 / Software/Firmware version

// ---- 电源状态寄存器 ----
// ---- Power Status Registers ----
#define M5PM1_REG_PWR_SRC \
    0x04  // R     [2:0] 当前供电源 / Power source
          //       0: 5VIN（USB/DC输入） / 0: 5VIN (USB/DC input)
          //       1: 5VINOUT（双向端口） / 1: 5VINOUT (bidirectional port)
          //       2: 电池 / 2: BAT (battery)
#define M5PM1_REG_WAKE_SRC \
    0x05  // R/W   [6:0] 唤醒源标志（写0清除） / Wake source flags (write 0 to clear)
          //       [6] 5VINOUT插入唤醒 / 5VINOUT insertion wake
          //       [5] 外部GPIO唤醒 / External GPIO wake
          //       [4] 命令复位唤醒 / Command reset wake
          //       [3] 复位按钮唤醒 / Reset button wake
          //       [2] 电源按钮唤醒 / Power button wake
          //       [1] VIN插入唤醒 / VIN insertion wake
          //       [0] 定时器唤醒 / Timer wake

// ---- 电源配置 ----
// ---- Power Configuration ----
#define M5PM1_REG_PWR_CFG \
    0x06  // R/W   [7-5] 保留 / Reserved
          //       [4] LED_EN 默认电平状态: 0=低电平 1=高电平 / LED_EN default level: 0=low, 1=high
          //       [3] BOOST_EN - BOOST/GROVE(5VINOUT)电源使能(视实际硬件而定)
          //           BOOST_EN - BOOST/GROVE(5VINOUT) power enable (hardware dependent)
          //       [2] LDO_EN - 3.3V LDO使能: 0=关闭 1=开启
          //           LDO_EN - 3.3V LDO enable: 0=off, 1=on, 0=off, 1=on
          //       [1] DCDC_EN - 5V DCDC使能: 0=关闭 1=开启
          //           DCDC_EN - 5V DCDC enable: 0=off, 1=on
          //       [0] CHG_EN - 充电使能: 0=关闭 1=开启 / CHG_EN - charging enable: 0=off, 1=on
          //       注意：下载模式或复位事件时自动清零 / Note: Auto-clears on download mode or reset events

#define M5PM1_REG_HOLD_CFG \
    0x07  // R/W   电源保持配置 / Power hold configuration
          //       [7] 保留 / Reserved
          //       [6] BOOST/GROVE(5VINOUT)电源保持(视实际硬件而定)
          //           BOOST/GROVE(5VINOUT) power hold (hardware dependent)
          //       [5] LDO(3.3V)电源保持 / LDO(3.3V) power hold
          //       [4] GPIO4输出状态保持 / GPIO4 output state hold
          //       [3] GPIO3输出状态保持 / GPIO3 output state hold
          //       [2] GPIO2输出状态保持 / GPIO2 output state hold
          //       [1] GPIO1输出状态保持 / GPIO1 output state hold
          //       [0] GPIO0输出状态保持 / GPIO0 output state hold
          //       注意：复位/下载模式/关机时自动清零为0x00 / Note: Auto-clears to 0x00 on reset/download/shutdown

#define M5PM1_REG_BATT_LVP \
    0x08  // R/W   电池低压保护阈值 / Battery low voltage protection threshold
          //       电压(mV) = 2000 + n * 7.81 / Voltage (mV) = 2000 + n * 7.81
          //       值 = (voltage_mV - 2000) / 7.81 / Value = (voltage_mV - 2000) / 7.81
          //       范围：2000mV ~ 4000mV / Range: 2000mV ~ 4000mV

#define M5PM1_REG_I2C_CFG \
    0x09  // R/W   I2C配置 / I2C configuration
          //       [7-5] 保留 / Reserved
          //       [4] SPD - I2C速度: 0=100KHz 1=400KHz / SPD - I2C speed: 0=100KHz, 1=400KHz
          //       [3:0] SLP_TO - I2C睡眠超时(秒): 0=禁用 1-15=1-15秒 / SLP_TO - I2C sleep timeout (s): 0=disabled,
          //       1-15=1-15s 注意：I2C空闲超时后PM1进入睡眠模式 / Note: PM1 enters sleep after I2C idle timeout

// ---- 看门狗寄存器 ----
// ---- Watchdog Registers ----
#define M5PM1_REG_WDT_CNT \
    0x0A                        // R/W   看门狗倒计时（秒） / Watchdog countdown (seconds)
                                //       0=禁用 / 0=disabled
                                //       1-255=超时秒数 / 1-255=timeout seconds
                                //       系统复位当倒计时到0 / System resets when countdown reaches 0
#define M5PM1_REG_WDT_KEY 0x0B  // W     写入0xA5喂狗 / Write 0xA5 to feed watchdog
#define M5PM1_REG_SYS_CMD \
    0x0C  // W     系统命令 / System command
          //       [7:4] 密钥必须为0xA / [7:4] KEY must be 0xA
          //       [3:2] 保留 / Reserved
          //       [1:0] CMD - 00:无操作 01:关机 10:复位 11:下载模式
          //             CMD - 00:no-op 01:shutdown 10:reboot 11:download mode

// ---- GPIO寄存器 ----
// ---- GPIO Registers ----
#define M5PM1_REG_GPIO_MODE \
    0x10  // R/W   [4:0] GPIO方向 / GPIO direction
          //       每bit对应一个GPIO: 1=输出 0=输入 / Each bit: 1=output, 0=input
#define M5PM1_REG_GPIO_OUT \
    0x11  // R/W   [4:0] GPIO输出电平 / GPIO output level
          //       每bit对应一个GPIO: 1=高 0=低 / Each bit: 1=high, 0=low
#define M5PM1_REG_GPIO_IN \
    0x12  // R     [4:0] GPIO输入状态 / GPIO input state
          //       每bit对应一个GPIO的当前输入电平 / Each bit: current input level of GPIO
#define M5PM1_REG_GPIO_DRV \
    0x13  // R/W   GPIO驱动模式 / GPIO drive mode
          //       [7-6] 保留 / Reserved
          //       [5] LED_EN_DRV - LED使能引脚驱动: 0=推挽 1=开漏
          //           LED_EN_DRV - LED enable pin drive: 0=push-pull, 1=open-drain
          //       [4:0] GPIO驱动: 1=开漏 0=推挽 / [4:0] GPIO drive: 1=open-drain, 0=push-pull
#define M5PM1_REG_GPIO_PUPD0 \
    0x14  // R/W   GPIO0-3上下拉配置 / Pull-up/down for GPIO0-3
          //       每GPIO占2bit: 00=无 01=上拉 10=下拉 11=保留
          //       Each GPIO uses 2 bits: 00=none, 01=pull-up, 10=pull-down, 11=reserved
          //       [7:6] GPIO3, [5:4] GPIO2, [3:2] GPIO1, [1:0] GPIO0 / [7:6] GPIO3
          //       [5:4] GPIO2, [3:2] GPIO1, [1:0] GPIO0
#define M5PM1_REG_GPIO_PUPD1 \
    0x15  // R/W   GPIO4上下拉配置 / Pull-up/down for GPIO4
          //       [1:0] GPIO4: 00=无 01=上拉 10=下拉 11=保留 / [1:0] GPIO4: 00=none, 01=pull-up, 10=pull-down,
          //       11=reserved
#define M5PM1_REG_GPIO_FUNC0 \
    0x16  // R/W   GPIO0-3功能选择 / GPIO0-3 function
          //       每GPIO占2bit: 00=GPIO 01=IRQ 10=WAKE 11=特殊功能
          //       Each GPIO uses 2 bits: 00=GPIO, 01=IRQ, 10=WAKE, 11=special
          //       GPIO0: 11=LED_EN (NeoPixel)
          //       GPIO1: 11=ADC1
          //       GPIO2: 11=ADC2
          //       GPIO3: 11=PWM0
#define M5PM1_REG_GPIO_FUNC1 \
    0x17  // R/W   GPIO4功能选择 / GPIO4 function
          //       [1:0] GPIO4: 00=GPIO 01=IRQ 10=WAKE 11=PWM1 / [1:0] GPIO4: 00=GPIO, 01=IRQ, 10=WAKE, 11=PWM1
#define M5PM1_REG_GPIO_WAKE_EN \
    0x18  // R/W   [4:0] GPIO唤醒使能 / GPIO wake enable
          //       每bit使能对应GPIO的唤醒功能 / Each bit enables wake function for corresponding GPIO
#define M5PM1_REG_GPIO_WAKE_CFG \
    0x19  // R/W   [4:0] GPIO唤醒边沿配置 / GPIO wake edge config
          //       每bit: 0=下降沿唤醒 1=上升沿唤醒 / Each bit: 0=falling edge, 1=rising edge

// ---- 电压读取寄存器 ----
// ---- Voltage Reading Registers ----
#define M5PM1_REG_VREF_L    0x20  // R     参考电压低字节 / VREF low byte (mV)
#define M5PM1_REG_VREF_H    0x21  // R     参考电压高字节 / VREF high byte
#define M5PM1_REG_VBAT_L    0x22  // R     电池电压低字节 / Battery voltage low byte (mV)
#define M5PM1_REG_VBAT_H    0x23  // R     电池电压高4位 / Battery voltage high 4 bits
#define M5PM1_REG_VIN_L     0x24  // R     VIN电压低字节 / VIN voltage low byte (mV)
#define M5PM1_REG_VIN_H     0x25  // R     VIN电压高4位 / VIN voltage high 4 bits
#define M5PM1_REG_5VINOUT_L 0x26  // R     5VINOUT电压低字节 / 5VINOUT voltage low byte (mV)
#define M5PM1_REG_5VINOUT_H 0x27  // R     5VINOUT电压高4位 / 5VINOUT voltage high 4 bits
#define M5PM1_REG_ADC_RES_L 0x28  // R     ADC结果低字节 / ADC result low byte (mV)
#define M5PM1_REG_ADC_RES_H 0x29  // R     ADC结果高4位 / ADC result high 4 bits
#define M5PM1_REG_ADC_CTRL \
    0x2A  // R/W   ADC控制 / ADC control
          //       [7-4] 保留 / Reserved
          //       [3:1] 通道: 1=ADC1(GPIO1) 2=ADC2(GPIO2) 6=芯片内部温度
          //       [3:1] Channel: 1=ADC1(GPIO1), 2=ADC2(GPIO2), 6=internal temperature
          //       [0]   START - 写1启动转换 / START - write 1 to start conversion

// ---- PWM寄存器 ----
// ---- PWM Registers ----
#define M5PM1_REG_PWM0_L 0x30  // R/W   PWM0占空比低字节 / PWM0 duty low byte
#define M5PM1_REG_PWM0_HC \
    0x31                       // R/W   PWM0高字节+控制 / PWM0 high byte + control
                               //       [7-6] 保留 / Reserved
                               //       [5] POL - 极性: 0=正常 1=反转 / POL - polarity: 0=normal, 1=invert
                               //       [4] EN - 使能: 0=关闭 1=开启 / EN - enable: 0=off, 1=on
                               //       [3:0] 占空比高4位 / Duty high 4 bits
                               //       12位占空比: 0-4095 (0-100%) / 12-bit duty: 0-4095 (0-100%)
#define M5PM1_REG_PWM1_L 0x32  // R/W   PWM1占空比低字节 / PWM1 duty low byte
#define M5PM1_REG_PWM1_HC \
    0x33                           // R/W   PWM1高字节+控制 / PWM1 high byte + control
                                   //       格式同PWM0_HC / Same format as PWM0_HC
#define M5PM1_REG_PWM_FREQ_L 0x34  // R/W   PWM频率低字节 / PWM frequency low byte (Hz)
#define M5PM1_REG_PWM_FREQ_H \
    0x35  // R/W   PWM频率高字节 / PWM frequency high byte
          //       范围：1-65535 Hz / Range: 1-65535 Hz

// ---- 定时器寄存器 ----
// ---- Timer Registers ----
#define M5PM1_REG_TIM_CNT_0 0x38  // R/W   定时器计数字节0 / Timer counter byte 0 (LSB)
#define M5PM1_REG_TIM_CNT_1 0x39  // R/W   定时器计数字节1 / Timer counter byte 1
#define M5PM1_REG_TIM_CNT_2 0x3A  // R/W   定时器计数字节2 / Timer counter byte 2
#define M5PM1_REG_TIM_CNT_3 \
    0x3B  // R/W   定时器计数字节3（bit 6:0，最大31位） / Timer counter byte 3 (bit 6:0, max 31 bits)
          //       定时器，单位秒，最大214748364秒(约6.8年) / Timer in seconds, max 214748364 (~6.8 years)
#define M5PM1_REG_TIM_CFG \
    0x3C  // R/W   定时器配置 / Timer configuration
          //       [7-4] 保留 / Reserved
          //       [3] ARM - 自动重装: 0=单次 1=自动重装 / ARM - auto reload: 0=one-shot, 1=auto
          //       [2:0] ACTION - 超时动作: 000=停止 001=标志 010=复位 011=开机 100=关机
          //             ACTION: 000=stop, 001=flag, 010=reboot, 011=power on, 100=shutdown
#define M5PM1_REG_TIM_KEY 0x3D  // W     写入0xA5重载定时器 / Write 0xA5 to reload timer

// ---- 中断寄存器 ----
// ---- IRQ Registers ----
// 注意：状态寄存器写0清除对应位 / Note: Write 0 to clear status bits
#define M5PM1_REG_IRQ_STATUS1 \
    0x40  // R/W   [4:0] GPIO中断状态 / GPIO interrupt status
          //       触发条件：GPIO配置为IRQ功能时的边沿触发 / Trigger: Edge trigger when GPIO configured as IRQ
#define M5PM1_REG_IRQ_STATUS2 \
    0x41  // R/W   系统中断状态 / System interrupt status
          //       [7-6] 保留 / Reserved
          //       [5] 电池移除 / Battery removed
          //       [4] 电池插入 / Battery inserted
          //       [3] 5VINOUT移除 / 5VINOUT removed
          //       [2] 5VINOUT插入 / 5VINOUT inserted
          //       [1] 5VIN移除 / 5VIN removed
          //       [0] 5VIN插入 / 5VIN inserted
          //       备注：电池事件仅充电使能时有效，5VINOUT事件仅输入模式时有效 / Note: Battery events only when charging
          //       enabled; 5VINOUT events only when in input mode
#define M5PM1_REG_IRQ_STATUS3 \
    0x42  // R/W   按钮中断状态 / Button interrupt status
          //       [7-3] 保留 / Reserved
          //       [2] 双击事件 / Double click event
          //       [1] 唤醒事件 / Wakeup event
          //       [0] 单击事件 / Single click event
#define M5PM1_REG_IRQ_MASK1 \
    0x43  // R/W   GPIO中断屏蔽 / GPIO interrupt mask
          //       [4:0] 每bit: 0=使能中断 1=屏蔽中断 / [4:0] Each bit: 0=enable interrupt, 1=mask interrupt
#define M5PM1_REG_IRQ_MASK2 0x44  // R/W   系统中断屏蔽 / System interrupt mask
#define M5PM1_REG_IRQ_MASK3 0x45  // R/W   按钮中断屏蔽 / Button interrupt mask

// ---- 按钮寄存器 ----
// ---- Button Registers ----
#define M5PM1_REG_BTN_STATUS \
    0x48                          // R     按钮状态 / Button status
                                  //       [7] BTN_FLAG - 按钮曾被按下标志（读取后自动清除）
                                  //           BTN_FLAG - button pressed flag (auto-clears on read)
                                  //       [6-1] 保留 / Reserved
                                  //       [0] BTN_STATE - 当前按钮状态: 0=释放 1=按下
                                  //           BTN_STATE - current button state: 0=release, 1=pressed
#define M5PM1_REG_BTN_CFG_1 0x49  // R/W   按钮配置 / Button configuration
//       [7] DL_LOCK - 下载模式锁定: 0=正常 1=锁定（禁止进入下载模式）
//           DL_LOCK - download mode lock: 0=normal, 1=locked (disable download mode)
//       [6:5] DBL_DLY - 双击间隔: 00=125ms 01=250ms 10=500ms 11=1s
//             DBL_DLY - double-click gap: 00=125ms, 01=250ms, 10=500ms, 11=1s
//       [4:3] LONG_DLY - 长按延时: 00=1s 01=2s 10=3s 11=4s
//             LONG_DLY - long-press delay: 00=1s, 01=2s, 10=3s, 11=4s
//       [2:1] CLK_DLY - 单击延时: 00=125ms 01=250ms 10=500ms 11=1s
//             CLK_DLY - click delay: 00=125ms, 01=250ms, 10=500ms, 11=1s
//       [0] SINGLE_RST_DIS - 单击复位禁用: 0=使能 1=禁用
//           SINGLE_RST_DIS - single-click reset disable: 0=enable, 1=disable
#define M5PM1_REG_BTN_CFG_2 \
    0x4A  // R/W   按钮配置2 / Button configuration 2
          //       [7-1] 保留 / Reserved
          //       [0] DOUBLE_OFF_DIS - 双击关机禁用: 0=使能 1=禁用
          //           DOUBLE_OFF_DIS - double-click shutdown disable: 0=enable, 1=disable

// ---- NeoPixel寄存器 ----
// ---- NeoPixel Registers ----
#define M5PM1_REG_NEO_CFG \
    0x50  // R/W   NeoPixel配置 / NeoPixel configuration
          //       [7] 保留 / Reserved
          //       [6] REFRESH - 写1刷新LED / REFRESH - write 1 to refresh LEDs
          //       [5:0] LED_CNT - LED数量 (1-32，固件最大支持32) / LED count (1-32, firmware max 32)
#define M5PM1_REG_AW8737A_PULSE \
    0x53  // R/W   AW8737A脉冲控制 / AW8737A pulse control
          //       用于控制AW8737A音频放大器增益 / Used to control AW8737A audio amplifier gain
          //       [7] REFRESH - 写1执行脉冲 / REFRESH - write 1 to execute pulse
          //       [6:5] NUM - 脉冲数量: 00=0 01=1 10=2 11=3 / NUM - pulse count: 00=0, 01=1, 10=2, 11=3
          //       [4:0] GPIO - 输出GPIO引脚号 / GPIO - output GPIO number

// ---- NeoPixel数据区 ----
// ---- NeoPixel Data Area ----
#define M5PM1_REG_NEO_DATA_START 0x60  // R/W   数据起始地址 / NeoPixel RGB565 data start
#define M5PM1_REG_NEO_DATA_END \
    0x9F  // R/W   数据结束地址 / NeoPixel RGB565 data end
          //       每LED占2字节(RGB565格式), 共32个LED / Each LED: 2 bytes (RGB565), total 32 LEDs

// ---- RTC RAM区域 ----
// ---- RTC RAM Area ----
#define M5PM1_REG_RTC_RAM_START 0xA0  // R/W   RTC RAM起始 / RTC RAM start (32 bytes)
#define M5PM1_REG_RTC_RAM_END \
    0xBF  // R/W   RTC RAM结束 / RTC RAM end
          //       睡眠期间数据保持，可用于存储小量重要数据
          //       Data retained during sleep, for storing small important data

// ============================
// 位定义
// Bit Definitions
// ============================

// ---- I2C配置寄存器位 ----
// ---- I2C_CFG Register Bits ----
#define M5PM1_I2C_CFG_SLEEP_MASK \
    0x0F  // I2C睡眠超时掩码 / I2C sleep timeout mask
          // 0=禁用 1-15=1-15秒 / 0=disabled, 1-15=1-15s
#define M5PM1_I2C_CFG_SPEED_400K \
    (1 << 4)  // I2C速度选择 / I2C speed select
              // 0=100KHz 1=400KHz / 0=100KHz, 1=400KHz

// ---- 系统命令寄存器值 ----
// ---- SYS_CMD Register Values ----
#define M5PM1_SYS_CMD_KEY      0xA0  // 命令密钥 / Command key (must be 0xA in high nibble)
#define M5PM1_SYS_CMD_SHUTDOWN 0x01  // 关机命令 / Shutdown command
#define M5PM1_SYS_CMD_REBOOT   0x02  // 复位命令 / Reboot command
#define M5PM1_SYS_CMD_JTAG     0x03  // 下载模式命令 / Download mode command

// ---- 密钥值 ----
// ---- Key Values ----
#define M5PM1_WDT_FEED_KEY   0xA5  // 喂狗密钥 / Watchdog feed key
#define M5PM1_TIM_RELOAD_KEY 0xA5  // 定时器重载密钥 / Timer reload key

// ---- NeoPixel配置寄存器位 ----
// ---- NEO_CFG Register Bits ----
#define M5PM1_NEO_CFG_REFRESH    (1 << 6)  // 刷新标志 / Refresh flag (write 1 to update LEDs)
#define M5PM1_NEO_CFG_COUNT_MASK 0x3F      // LED数量掩码 / LED count mask (0-63, firmware max 32)

// ============================
// 枚举类型
// Enumerations
// ============================

/**
 * @brief GPIO引脚编号 / GPIO pin number
 * @note GPIO0: LED_EN功能 / LED_EN function
 *       GPIO1: ADC1功能 / ADC1 function
 *       GPIO2: ADC2功能 / ADC2 function
 *       GPIO3: PWM0功能 / PWM0 function
 *       GPIO4: PWM1功能 / PWM1 function
 */
typedef enum {
    M5PM1_GPIO_NUM_0 = 0,    // GPIO0 (可用于LED_EN)
                             // (can be LED_EN)
    M5PM1_GPIO_NUM_1 = 1,    // GPIO1 (可用于ADC1)
                             // (can be ADC1)
    M5PM1_GPIO_NUM_2 = 2,    // GPIO2 (可用于ADC2)
                             // (can be ADC2)
    M5PM1_GPIO_NUM_3 = 3,    // GPIO3 (可用于PWM0)
                             // (can be PWM0)
    M5PM1_GPIO_NUM_4 = 4,    // GPIO4 (可用于PWM1)
                             // (can be PWM1)
    M5PM1_GPIO_NUM_NC = 255  // 未连接
                             // Not connected
} m5pm1_gpio_num_t;

/**
 * @brief GPIO方向模式 / GPIO direction mode
 */
typedef enum {
    M5PM1_GPIO_MODE_INPUT = 0,  // 输入模式
                                // Input mode
    M5PM1_GPIO_MODE_OUTPUT = 1  // 输出模式
                                // Output mode
} m5pm1_gpio_mode_t;

/**
 * @brief GPIO功能选择 / GPIO function selection
 * @note 特殊功能(0b11)因引脚而异:
 *       Special function (0b11) varies by pin:
 *       - GPIO0: LED_EN (NeoPixel使能)
 *       - GPIO1: ADC1 (模拟输入)
 *       - GPIO2: ADC2 (模拟输入)
 *       - GPIO3: PWM0 (脉宽调制输出)
 *       - GPIO4: PWM1 (脉宽调制输出)
 */
typedef enum {
    M5PM1_GPIO_FUNC_GPIO = 0b00,  // 普通GPIO功能
                                  // Normal GPIO function
    M5PM1_GPIO_FUNC_IRQ = 0b01,   // 中断触发功能
                                  // Interrupt trigger function
    M5PM1_GPIO_FUNC_WAKE = 0b10,  // 唤醒功能
                                  // Wake function
    M5PM1_GPIO_FUNC_OTHER = 0b11  // 特殊功能(LED/PWM/ADC)
                                  // Special function
} m5pm1_gpio_func_t;

/**
 * @brief GPIO上下拉配置 / GPIO pull-up/pull-down configuration
 */
typedef enum {
    M5PM1_GPIO_PULL_NONE = 0b00,  // 无上下拉
                                  // No pull
    M5PM1_GPIO_PULL_UP = 0b01,    // 上拉使能
                                  // Pull-up enabled
    M5PM1_GPIO_PULL_DOWN = 0b10   // 下拉使能
                                  // Pull-down enabled
} m5pm1_gpio_pull_t;

/**
 * @brief GPIO输出驱动模式 / GPIO output drive mode
 */
typedef enum {
    M5PM1_GPIO_DRIVE_PUSHPULL = 0,  // 推挽输出
                                    // Push-pull output
    M5PM1_GPIO_DRIVE_OPENDRAIN = 1  // 开漏输出
                                    // Open-drain output
} m5pm1_gpio_drive_t;

/**
 * @brief GPIO唤醒边沿配置 / GPIO wake edge configuration
 */
typedef enum {
    M5PM1_GPIO_WAKE_FALLING = 0,  // 下降沿唤醒
                                  // Wake on falling edge
    M5PM1_GPIO_WAKE_RISING = 1    // 上升沿唤醒
                                  // Wake on rising edge
} m5pm1_gpio_wake_edge_t;

/**
 * @brief ADC通道选择 / ADC channel selection
 * @note GPIO必须配置为FUNC_OTHER(0b11)才能使用ADC功能
 *       GPIO must be configured as FUNC_OTHER (0b11) to use ADC function
 */
typedef enum {
    M5PM1_ADC_CH_1 = 1,    // ADC通道1 (GPIO1)
                           // ADC channel 1 (GPIO1)
    M5PM1_ADC_CH_2 = 2,    // ADC通道2 (GPIO2)
                           // ADC channel 2 (GPIO2)
    M5PM1_ADC_CH_TEMP = 6  // 内部温度传感器
                           // Internal temperature sensor
} m5pm1_adc_channel_t;

/**
 * @brief PWM通道选择 / PWM channel selection
 * @note GPIO必须配置为FUNC_OTHER(0b11)才能使用PWM功能
 *       GPIO must be configured as FUNC_OTHER (0b11) to use PWM function
 */
typedef enum {
    M5PM1_PWM_CH_0 = 0,  // PWM通道0 (GPIO3)
                         // PWM channel 0 (GPIO3)
    M5PM1_PWM_CH_1 = 1   // PWM通道1 (GPIO4)
                         // PWM channel 1 (GPIO4)
} m5pm1_pwm_channel_t;

/**
 * @brief 当前供电源 / Current power source
 */
typedef enum {
    M5PM1_PWR_SRC_5VIN = 0,     // USB/DC 5V输入
                                // USB/DC 5V input
    M5PM1_PWR_SRC_5VINOUT = 1,  // 5V双向端口输入
                                // 5V bidirectional port input
    M5PM1_PWR_SRC_BAT = 2,      // 电池供电
                                // Battery power
    M5PM1_PWR_SRC_UNKNOWN = 3   // 未知/无供电
                                // Unknown/no power
} m5pm1_pwr_src_t;

/**
 * @brief 唤醒源标志 / Wake source flags
 * @note 可组合使用(位掩码) / Can be combined (bitmask)
 *       写1清除对应唤醒标志 / Write 1 to clear corresponding flag
 */
typedef enum {
    M5PM1_WAKE_SRC_TIM = 0x01,       // 定时器唤醒
                                     // Timer wake
    M5PM1_WAKE_SRC_VIN = 0x02,       // VIN插入唤醒
                                     // VIN insertion wake
    M5PM1_WAKE_SRC_PWRBTN = 0x04,    // 电源按钮唤醒
                                     // Power button wake
    M5PM1_WAKE_SRC_RSTBTN = 0x08,    // 复位按钮唤醒
                                     // Reset button wake
    M5PM1_WAKE_SRC_CMD_RST = 0x10,   // 命令复位唤醒
                                     // Command reset wake
    M5PM1_WAKE_SRC_EXT_WAKE = 0x20,  // 外部GPIO唤醒
                                     // External GPIO wake
    M5PM1_WAKE_SRC_5VINOUT = 0x40    // 5VINOUT插入唤醒
                                     // 5VINOUT insertion wake
} m5pm1_wake_src_t;

/**
 * @brief 系统命令 / System command
 * @note 命令需要与密钥(0xA0)组合使用
 *       Command needs to be combined with key (0xA0)
 */
typedef enum {
    M5PM1_SYS_CMD_NONE = 0x00,   // 无操作
                                 // No operation
    M5PM1_SYS_CMD_OFF = 0x01,    // 关机
                                 // Shutdown
    M5PM1_SYS_CMD_RESET = 0x02,  // 复位
                                 // Reset
    M5PM1_SYS_CMD_DL = 0x03      // 进入下载模式
                                 // Enter download mode
} m5pm1_sys_cmd_t;

/**
 * @brief 定时器超时动作 / Timer timeout action
 */
typedef enum {
    M5PM1_TIM_ACTION_STOP = 0b000,     // 停止，无动作
                                       // Stop, no action
    M5PM1_TIM_ACTION_FLAG = 0b001,     // 仅设置标志
                                       // Set flag only
    M5PM1_TIM_ACTION_REBOOT = 0b010,   // 系统复位
                                       // System reboot
    M5PM1_TIM_ACTION_POWERON = 0b011,  // 开机
                                       // Power on
    M5PM1_TIM_ACTION_POWEROFF = 0b100  // 关机
                                       // Power off
} m5pm1_tim_action_t;

/**
 * @brief 按钮事件类型 / Button event type
 */
typedef enum {
    M5PM1_BTN_TYPE_CLICK = 0,   // 单击
                                // Single click
    M5PM1_BTN_TYPE_DOUBLE = 1,  // 双击
                                // Double click
    M5PM1_BTN_TYPE_LONG = 2     // 长按
                                // Long press
} m5pm1_btn_type_t;

/**
 * @brief 按钮延时配置 / Button delay/timeout configuration
 */
typedef enum {
    M5PM1_BTN_CLICK_DELAY_125MS = 0x00,          // 125毫秒
                                                 // 125 milliseconds
    M5PM1_BTN_CLICK_DELAY_250MS = 0x01,          // 250毫秒
                                                 // 250 milliseconds
    M5PM1_BTN_CLICK_DELAY_500MS = 0x02,          // 500毫秒
                                                 // 500 milliseconds
    M5PM1_BTN_CLICK_DELAY_1000MS = 0x03,         // 1000毫秒
                                                 // 1000 milliseconds
    M5PM1_BTN_DOUBLE_CLICK_DELAY_125MS = 0x00,   // 双击125毫秒
                                                 // Double click 125 milliseconds
    M5PM1_BTN_DOUBLE_CLICK_DELAY_250MS = 0x01,   // 双击250毫秒
                                                 // Double click 250 milliseconds
    M5PM1_BTN_DOUBLE_CLICK_DELAY_500MS = 0x02,   // 双击500毫秒
                                                 // Double click 500 milliseconds
    M5PM1_BTN_DOUBLE_CLICK_DELAY_1000MS = 0x03,  // 双击1000毫秒
                                                 // Double click 1000 milliseconds
    M5PM1_BTN_LONG_PRESS_DELAY_1000MS = 0x00,    // 长按1000毫秒
                                                 // Long press 1000 milliseconds
    M5PM1_BTN_LONG_PRESS_DELAY_2000MS = 0x01,    // 长按2000毫秒
                                                 // Long press 2000 milliseconds
    M5PM1_BTN_LONG_PRESS_DELAY_3000MS = 0x02,    // 长按3000毫秒
                                                 // Long press 3000 milliseconds
    M5PM1_BTN_LONG_PRESS_DELAY_4000MS = 0x03     // 长按4000毫秒
                                                 // Long press 4000 milliseconds
} m5pm1_btn_delay_t;

/**
 * @brief 中断屏蔽控制类型 / IRQ Mask Control Types
 */
typedef enum {
    M5PM1_IRQ_MASK_DISABLE = 0x00,  // 不屏蔽中断（允许中断）
                                    // Don't mask (Enable interrupt)
    M5PM1_IRQ_MASK_ENABLE = 0x01    // 屏蔽中断（禁止中断）
                                    // Mask interrupt (Disable interrupt)
} m5pm1_irq_mask_ctrl_t;

/**
 * @brief I2C速度选择 / I2C speed selection
 * @note PM1上电默认100KHz，切换400KHz需要先写入配置再重新初始化I2C
 *       PM1 defaults to 100KHz on power-up, switching to 400KHz requires
 *       writing config first then re-initializing I2C
 */
typedef enum {
    M5PM1_I2C_SPEED_100K = 0,  // 100KHz 标准模式
                               // Standard mode
    M5PM1_I2C_SPEED_400K = 1   // 400KHz 快速模式
                               // Fast mode
} m5pm1_i2c_speed_t;

// 日志级别定义
// Log level definitions
typedef enum {
    M5PM1_LOG_LEVEL_NONE = 0,  // 无日志输出
                               // No log output
    M5PM1_LOG_LEVEL_ERROR,     // 仅错误消息
                               // Error messages only
    M5PM1_LOG_LEVEL_WARN,      // 警告和错误消息
                               // Warning and error messages
    M5PM1_LOG_LEVEL_INFO,      // 信息、警告和错误消息（默认）
                               // Info, warning and error messages (default)
    M5PM1_LOG_LEVEL_DEBUG,     // 调试、信息、警告和错误消息
                               // Debug, info, warning and error messages
    M5PM1_LOG_LEVEL_VERBOSE    // 所有消息包括详细输出
                               // All messages including verbose
} m5pm1_log_level_t;

/**
 * @brief AW8737A pulse count for gain control / AW8737A增益控制脉冲数
 * @note AW8737A是一款音频功率放大器，通过脉冲数设置增益
 *       AW8737A is an audio power amplifier, gain is set by pulse count
 */
typedef enum {
    M5PM1_AW8737A_PULSE_0 = 0,  // 0个脉冲 (静音)
                                // 0 pulses (mute)
    M5PM1_AW8737A_PULSE_1 = 1,  // 1个脉冲
                                // 1 pulse
    M5PM1_AW8737A_PULSE_2 = 2,  // 2个脉冲
                                // 2 pulses
    M5PM1_AW8737A_PULSE_3 = 3   // 3个脉冲
                                // 3 pulses
} m5pm1_aw8737a_pulse_t;

/**
 * @brief AW8737A pulse refresh mode / AW8737A脉冲刷新模式
 */
typedef enum {
    M5PM1_AW8737A_REFRESH_WAIT = 0,  // 等待手动刷新
                                     // Wait for manual refresh
    M5PM1_AW8737A_REFRESH_NOW = 1    // 立即执行
                                     // Execute immediately
} m5pm1_aw8737a_refresh_t;

// ============================
// AW8737A 模式类型
// AW8737A Mode Types
// ============================
/**
 * @brief AW8737A 增益模式 / AW8737A gain mode
 */
typedef enum {
    M5PM1_AW8737A_MODE_1 = 0,  // 模式1: 0脉冲 (关闭/静音)
                               // Mode 1: 0 pulse (off/mute)
    M5PM1_AW8737A_MODE_2 = 1,  // 模式2: 1脉冲 (低增益)
                               // Mode 2: 1 pulse (low gain)
    M5PM1_AW8737A_MODE_3 = 2,  // 模式3: 2脉冲 (中增益)
                               // Mode 3: 2 pulses (medium gain)
    M5PM1_AW8737A_MODE_4 = 3   // 模式4: 3脉冲 (高增益)
                               // Mode 4: 3 pulses (high gain)
} m5pm1_aw8737a_mode_t;

// ============================
// 清除类型枚举（用于 IRQ/WAKE 状态读取）
// Clean Type Enumeration (for IRQ/WAKE status read)
// ============================
/**
 * @brief 状态读取后的清除行为
 *        Clean behavior after status read
 * @note 用于 getWakeSource(), irqGetGpioStatus(), irqGetSysStatus(), irqGetBtnStatus()
 *       Used for getWakeSource(), irqGetGpioStatus(), irqGetSysStatus(), irqGetBtnStatus()
 */
typedef enum {
    M5PM1_CLEAN_NONE = 0x00,  // 不清除
                              // No clean
    M5PM1_CLEAN_ONCE = 0x01,  // 清除一位（从低位往高位逐个清除）
                              // Clean one bit (from low to high, one at a time)
    M5PM1_CLEAN_ALL = 0x02    // 清除所有位
                              // Clean all bits
} m5pm1_clean_type_t;

// ============================
// GPIO 中断标志枚举
// GPIO IRQ Flag Enumeration
// ============================
/**
 * @brief GPIO 中断标志位
 *        GPIO interrupt flag bits
 * @note 用于 irqGetGpioStatus(), irqClearGpioAll()
 *       Used for irqGetGpioStatus(), irqClearGpioAll()
 */
typedef enum {
    M5PM1_IRQ_GPIO0 = (1 << 0),  // GPIO0 中断标志
                                 // GPIO0 interrupt flag
    M5PM1_IRQ_GPIO1 = (1 << 1),  // GPIO1 中断标志
                                 // GPIO1 interrupt flag
    M5PM1_IRQ_GPIO2 = (1 << 2),  // GPIO2 中断标志
                                 // GPIO2 interrupt flag
    M5PM1_IRQ_GPIO3 = (1 << 3),  // GPIO3 中断标志
                                 // GPIO3 interrupt flag
    M5PM1_IRQ_GPIO4 = (1 << 4),  // GPIO4 中断标志
                                 // GPIO4 interrupt flag
    M5PM1_IRQ_GPIO_ALL = 0x1F,   // 所有 GPIO 中断标志
                                 // All GPIO interrupt flags
    M5PM1_IRQ_GPIO_NONE = 0x00   // 无中断
                                 // No interrupt
} m5pm1_irq_gpio_t;

/**
 * @brief 按钮中断状态位掩码
 *        Button interrupt status bitmask
 * @note 用于 irqGetBtnStatus(), irqSetBtnMask(), irqGetBtnMask()
 *       Used for irqGetBtnStatus(), irqSetBtnMask(), irqGetBtnMask()
 */
typedef enum {
    M5PM1_IRQ_BTN_CLICK = (1 << 0),   // 单击事件
                                      // Single click event
    M5PM1_IRQ_BTN_WAKE = (1 << 1),    // 唤醒事件
                                      // Wakeup event
    M5PM1_IRQ_BTN_DOUBLE = (1 << 2),  // 双击事件
                                      // Double click event
    M5PM1_IRQ_BTN_ALL = 0x07,         // 所有按钮中断标志
                                      // All button interrupt flags
    M5PM1_IRQ_BTN_NONE = 0x00         // 无中断
                                      // No interrupt
} m5pm1_irq_btn_t;

// ============================
// 系统中断标志枚举
// System IRQ Flag Enumeration
// ============================
/**
 * @brief 系统中断标志位
 *        System interrupt flag bits
 * @note 用于 irqGetSysStatus(), irqClearSysAll()
 *       Used for irqGetSysStatus(), irqClearSysAll()
 */
typedef enum {
    M5PM1_IRQ_SYS_5VIN_INSERT = (1 << 0),     // 5VIN 插入
                                              // 5VIN insertion
    M5PM1_IRQ_SYS_5VIN_REMOVE = (1 << 1),     // 5VIN 移除
                                              // 5VIN removal
    M5PM1_IRQ_SYS_5VINOUT_INSERT = (1 << 2),  // 5VINOUT 插入
                                              // 5VINOUT insertion
    M5PM1_IRQ_SYS_5VINOUT_REMOVE = (1 << 3),  // 5VINOUT 移除
                                              // 5VINOUT removal
    M5PM1_IRQ_SYS_BAT_INSERT = (1 << 4),      // 电池插入
                                              // Battery insertion
    M5PM1_IRQ_SYS_BAT_REMOVE = (1 << 5),      // 电池移除
                                              // Battery removal
    M5PM1_IRQ_SYS_ALL = 0x3F,                 // 所有系统中断标志
                                              // All system interrupt flags
    M5PM1_IRQ_SYS_NONE = 0x00                 // 无中断
                                              // No interrupt
} m5pm1_irq_sys_t;

// ============================
// 电源配置位掩码枚举
// Power Configuration Bitmask Enumeration
// ============================
/**
 * @brief 电源配置位掩码
 *        Power configuration bitmask
 * @note 用于 setPowerConfig(), getPowerConfig()
 *       Used for setPowerConfig(), getPowerConfig()
 */
typedef enum {
    M5PM1_PWR_CFG_CHG_EN = (1 << 0),    // 充电使能
                                        // Charge enable
    M5PM1_PWR_CFG_DCDC_EN = (1 << 1),   // 5V DCDC 使能
                                        // 5V DCDC enable
    M5PM1_PWR_CFG_LDO_EN = (1 << 2),    // 3.3V LDO 使能
                                        // 3.3V LDO enable
    M5PM1_PWR_CFG_BOOST_EN = (1 << 3),  // BOOST/GROVE(5VINOUT)电源保持(视实际硬件而定)
                                        // BOOST/GROVE(5VINOUT) power hold (hardware dependent)
    M5PM1_PWR_CFG_LED_CTRL = (1 << 4)   // LED_EN 默认电平状态 0:低电平 1:高电平
                                        // LED_EN default level state 0: low 1: high
} m5pm1_pwr_cfg_t;

// ============================
// GPIO 输出状态枚举
// GPIO Output State Enumeration
// ============================
/**
 * @brief GPIO 输出状态
 *        GPIO output state
 * @note 用于 gpioSetOutput()
 *       Used for gpioSetOutput()
 */
typedef enum {
    M5PM1_GPIO_STATE_LOW = 0,  // 低电平
                               // Low level
    M5PM1_GPIO_STATE_HIGH = 1  // 高电平
                               // High level
} m5pm1_gpio_state_t;

// ============================
// GPIO 电平定义
// GPIO Level Definitions
// ============================
#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif

// ============================
// GPIO 模式定义（Arduino 兼容）
// GPIO Mode Definitions (Arduino-compatible)
// ============================
#ifndef INPUT
#define INPUT 0x01
#endif
#ifndef OUTPUT
#define OUTPUT 0x03
#endif
#ifndef PULLUP
#define PULLUP 0x04
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x05
#endif
#ifndef PULLDOWN
#define PULLDOWN 0x08
#endif
#ifndef INPUT_PULLDOWN
#define INPUT_PULLDOWN 0x09
#endif
#ifndef OPEN_DRAIN
#define OPEN_DRAIN 0x10
#endif
#ifndef OUTPUT_OPEN_DRAIN
#define OUTPUT_OPEN_DRAIN 0x13
#endif
#ifndef ANALOG
#define ANALOG 0xC0
#endif
#ifndef M5PM1_OTHER
#define M5PM1_OTHER 0xFF
#endif

// ============================
// RGB 颜色结构
// RGB Color Structure
// ============================
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} m5pm1_rgb_t;

// ============================
// Pin 状态结构
// Pin Status Structure
// ============================
/**
 * @brief 缓存的 GPIO 引脚状态 / Cached GPIO pin status
 * @note 此结构维护软件层引脚状态以减少 I2C 事务
 *       This structure maintains software-level pin state to reduce I2C transactions
 */
typedef struct {
    m5pm1_gpio_func_t func;            ///< GPIO 功能 / GPIO function (GPIO/IRQ/WAKE/OTHER)
    m5pm1_gpio_mode_t mode;            ///< 方向 / Direction (INPUT/OUTPUT)
    uint8_t output;                    ///< 输出值 / Output value (0/1)
    m5pm1_gpio_pull_t pull;            ///< 上下拉 / Pull-up/pull-down (NONE/UP/DOWN)
    bool wake_en;                      ///< 唤醒使能 / Wake enabled
    m5pm1_gpio_wake_edge_t wake_edge;  ///< 唤醒边沿 / Wake edge (RISING/FALLING)
    m5pm1_gpio_drive_t drive;          ///< 驱动模式 / Drive mode (PUSHPULL/OPENDRAIN)
    bool power_hold;                   ///< 电源保持状态 / Power hold state
} m5pm1_pin_status_t;

// ============================
// 用于验证的配置类型
// Configuration Type for Validation
// ============================
typedef enum {
    M5PM1_CONFIG_GPIO_INPUT = 0,
    M5PM1_CONFIG_GPIO_OUTPUT,
    M5PM1_CONFIG_GPIO_INTERRUPT,
    M5PM1_CONFIG_GPIO_WAKE,
    M5PM1_CONFIG_ADC,
    M5PM1_CONFIG_PWM,
    M5PM1_CONFIG_NEOPIXEL
} m5pm1_config_type_t;

// ============================
// 快照更新域
// Snapshot Update Domains
// ============================
typedef enum {
    M5PM1_SNAPSHOT_DOMAIN_GPIO       = 1 << 0,
    M5PM1_SNAPSHOT_DOMAIN_PWM        = 1 << 1,
    M5PM1_SNAPSHOT_DOMAIN_ADC        = 1 << 2,
    M5PM1_SNAPSHOT_DOMAIN_POWER      = 1 << 3,
    M5PM1_SNAPSHOT_DOMAIN_BUTTON     = 1 << 4,
    M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK   = 1 << 5,
    M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS = 1 << 6,
    M5PM1_SNAPSHOT_DOMAIN_I2C        = 1 << 7,
    M5PM1_SNAPSHOT_DOMAIN_NEO        = 1 << 8,
    M5PM1_SNAPSHOT_DOMAIN_AW8737A    = 1 << 9,
    M5PM1_SNAPSHOT_DOMAIN_ALL        = 0x3FF
} m5pm1_snapshot_domain_t;

// ============================
// 配置验证结果
// Configuration Validation Result
// ============================
typedef struct {
    bool valid;
    char error_msg[64];
    uint8_t conflicting_pin;
} m5pm1_validation_t;

// ============================
// 快照验证结果
// Snapshot Verification Result
// ============================
typedef struct {
    bool consistent;             // 缓存与寄存器一致时为 true
                                 // True if cached values match registers
    bool gpio_mismatch;          // GPIO 快照不一致
                                 // GPIO snapshot mismatch
    bool pwm_mismatch;           // PWM 快照不一致
                                 // PWM snapshot mismatch
    bool adc_mismatch;           // ADC 快照不一致
                                 // ADC snapshot mismatch
    bool power_mismatch;         // 电源配置快照不一致
                                 // Power config snapshot mismatch
    bool button_mismatch;        // 按钮配置快照不一致
                                 // Button config snapshot mismatch
    bool irq_mask_mismatch;      // IRQ 掩码快照不一致
                                 // IRQ mask snapshot mismatch
    bool i2c_mismatch;           // I2C 配置快照不一致
                                 // I2C config snapshot mismatch
    bool neo_mismatch;           // Neo 配置快照不一致
                                 // Neo config snapshot mismatch
    bool aw8737a_mismatch;       // AW8737A 快照不一致
                                 // AW8737A snapshot mismatch
    uint8_t expected_gpio_mode;  // 缓存的 GPIO 模式值
                                 // Cached GPIO mode value
    uint8_t actual_gpio_mode;    // 实际的 GPIO 模式值
                                 // Actual GPIO mode value
    uint8_t expected_gpio_out;   // 缓存的 GPIO 输出值
                                 // Cached GPIO output value
    uint8_t actual_gpio_out;     // 实际的 GPIO 输出值
                                 // Actual GPIO output value
    uint8_t expected_pwr_cfg;    // 缓存的 PWR_CFG 值
                                 // Cached PWR_CFG value
    uint8_t actual_pwr_cfg;      // 实际的 PWR_CFG 值
                                 // Actual PWR_CFG value
    uint8_t expected_hold_cfg;   // 缓存的 HOLD_CFG 值
                                 // Cached HOLD_CFG value
    uint8_t actual_hold_cfg;     // 实际的 HOLD_CFG 值
                                 // Actual HOLD_CFG value
    uint8_t expected_i2c_cfg;    // 缓存的 I2C_CFG 值
                                 // Cached I2C_CFG value
    uint8_t actual_i2c_cfg;      // 实际的 I2C_CFG 值
                                 // Actual I2C_CFG value
    uint8_t expected_neo_cfg;    // 缓存的 NEO_CFG 值
                                 // Cached NEO_CFG value
    uint8_t actual_neo_cfg;      // 实际的 NEO_CFG 值
                                 // Actual NEO_CFG value
    uint8_t expected_aw8737a;    // 缓存的 AW8737A 寄存器值
                                 // Cached AW8737A register value
    uint8_t actual_aw8737a;      // 实际的 AW8737A 寄存器值
                                 // Actual AW8737A register value
} m5pm1_snapshot_verify_t;

// ============================
// M5PM1 类
// M5PM1 Class
// ============================
class M5PM1 {
public:
    /**
     * @brief 构造 M5PM1 对象
     *        Construct M5PM1 object
     */
    M5PM1();
    /**
     * @brief 析构 M5PM1 对象
     *        Destroy M5PM1 object
     */
    ~M5PM1();

    // ========================
    // 初始化
    // Initialization
    // ========================
#ifdef ARDUINO
    /**
     * @brief Initialize the device (Arduino)
     * @param wire Pointer to TwoWire instance
     * @param addr I2C address (default 0x6E)
     * @param sda SDA pin (default -1, uses default I2C pins)
     * @param scl SCL pin (default -1, uses default I2C pins)
     * @param speed I2C speed in Hz (default 100000)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t begin(TwoWire* wire = &Wire, uint8_t addr = M5PM1_DEFAULT_ADDR, int8_t sda = -1, int8_t scl = -1,
                      uint32_t speed = M5PM1_I2C_FREQ_100K);
#if M5PM1_HAS_M5UNIFIED_I2C
    /**
     * @brief Initialize with M5Unified I2C_Class instance (Arduino)
     * @note  The I2C bus must already be initialized (i2c->begin() called) by the caller.
     *        M5PM1 borrows the I2C_Class; caller retains ownership and lifecycle.
     *        不负责驱动安装或释放，I2C 由调用方全程管理。
     * @param i2c   已 begin() 的 m5::I2C_Class 指针
     *              Pointer to an already begin()-ed m5::I2C_Class
     * @param addr  I2C 地址 / I2C address (default 0x6E)
     * @param speed I2C 速率（Hz，100000 或 400000）/ I2C speed in Hz (100000 or 400000)
     * @return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t begin(m5::I2C_Class* i2c, uint8_t addr = M5PM1_DEFAULT_ADDR, uint32_t speed = M5PM1_I2C_FREQ_DEFAULT);
#endif  // M5PM1_HAS_M5UNIFIED_I2C
#else   // ESP-IDF
    /**
     * @brief Initialize with self-created I2C bus (ESP-IDF)
     * @param port I2C port number
     * @param addr I2C address (default 0x6E)
     * @param sda SDA pin
     * @param scl SCL pin
     * @param speed I2C speed in Hz
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t begin(i2c_port_t port = I2C_NUM_0, uint8_t addr = M5PM1_DEFAULT_ADDR, int sda = 21, int scl = 22,
                      uint32_t speed = M5PM1_I2C_FREQ_100K);

#if M5PM1_HAS_I2C_MASTER
    /**
     * @brief Initialize with existing i2c_master_bus handle (ESP-IDF native, IDF >= 5.3.0)
     * @param bus Existing i2c_master_bus_handle_t
     * @param addr I2C address (default 0x6E)
     * @param speed I2C speed in Hz
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t begin(i2c_master_bus_handle_t bus, uint8_t addr = M5PM1_DEFAULT_ADDR,
                      uint32_t speed = M5PM1_I2C_FREQ_100K);
#endif  // M5PM1_HAS_I2C_MASTER

#if M5PM1_HAS_I2C_BUS
    /**
     * @brief Initialize with existing i2c_bus handle (esp-idf-lib)
     * @param bus Existing i2c_bus_handle_t
     * @param addr I2C address (default 0x6E)
     * @param speed I2C speed in Hz
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t begin(i2c_bus_handle_t bus, uint8_t addr = M5PM1_DEFAULT_ADDR, uint32_t speed = M5PM1_I2C_FREQ_100K);
#else
    /**
     * @brief i2c_bus 重载在不可用时直接返回错误
     *        i2c_bus overload returns error when unavailable
     * @note i2c_bus API 不可用时此重载仍保留，以避免链接期符号缺失。
     *       This overload is kept to avoid missing symbol errors at link time.
     */
    inline m5pm1_err_t begin(i2c_bus_handle_t bus, uint8_t addr = M5PM1_DEFAULT_ADDR,
                             uint32_t speed = M5PM1_I2C_FREQ_100K)
    {
        (void)bus;
        (void)addr;
        (void)speed;
        return M5PM1_ERR_NOT_SUPPORTED;
    }
#endif

#if M5PM1_HAS_M5UNIFIED_I2C
    /**
     * @brief Initialize with M5Unified I2C_Class instance (ESP-IDF)
     * @note  The I2C bus must already be initialized (i2c->begin() called) by the caller.
     *        M5PM1 borrows the I2C_Class; caller retains ownership and lifecycle.
     *        不负责驱动安装或释放，I2C 由调用方全程管理。
     * @param i2c   已 begin() 的 m5::I2C_Class 指针
     *              Pointer to an already begin()-ed m5::I2C_Class
     * @param addr  I2C 地址 / I2C address (default 0x6E)
     * @param speed I2C 速率（Hz，100000 或 400000）/ I2C speed in Hz (100000 or 400000)
     * @return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t begin(m5::I2C_Class* i2c, uint8_t addr = M5PM1_DEFAULT_ADDR, uint32_t speed = M5PM1_I2C_FREQ_DEFAULT);
#endif  // M5PM1_HAS_M5UNIFIED_I2C

#endif

    /**
     * @brief 设置全局日志级别
     *        Set global log level
     * @param level 日志级别
     *        Log level
     */
    static void setLogLevel(m5pm1_log_level_t level);

    /**
     * @brief 获取当前日志级别
     *        Get current log level
     * @return 日志级别
     *         Log level
     */
    static m5pm1_log_level_t getLogLevel();

    // ========================
    // 设备信息
    // Device Information
    // ========================
    /**
     * @brief 读取设备 ID
     *        Read device ID
     * @param id 输出：设备 ID (通常为 0x6E)
     *           Output: device ID (typically 0x6E)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getDeviceId(uint8_t* id);

    /**
     * @brief 读取设备型号
     *        Read device model
     * @param model 输出：设备型号
     *              Output: device model
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getDeviceModel(uint8_t* model);

    /**
     * @brief 读取硬件版本
     *        Read hardware version
     * @param version 输出：硬件版本号
     *                Output: hardware version number
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getHwVersion(uint8_t* version);

    /**
     * @brief 读取软件版本
     *        Read software version
     * @param version 输出：软件版本号
     *                Output: software version number
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getSwVersion(uint8_t* version);

    // ========================
    // GPIO 功能 (Arduino风格 - 带返回值)
    // GPIO Functions (Arduino-style - WithRes)
    // ========================
    /**
     * @brief 设置引脚模式（带错误返回）
     *        Set pin mode (with error return)
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param mode 模式：INPUT (0) 或 OUTPUT (1)
     *             Mode: INPUT (0) or OUTPUT (1)
     * @param err 输出：错误码指针，如果为 NULL 则忽略
     *            Output: error code pointer, ignored if NULL
     * @note 此函数会检查引脚冲突并返回错误码
     *       This function checks for pin conflicts and returns error code
     */
    void pinModeWithRes(uint8_t pin, uint8_t mode, m5pm1_err_t* err);

    /**
     * @brief 写入数字电平（带错误返回）
     *        Write digital level (with error return)
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param value 电平值：LOW (0) 或 HIGH (1)
     *              Level: LOW (0) or HIGH (1)
     * @param err 输出：错误码指针，如果为 NULL 则忽略
     *            Output: error code pointer, ignored if NULL
     */
    void digitalWriteWithRes(uint8_t pin, uint8_t value, m5pm1_err_t* err);

    /**
     * @brief 读取数字电平（带错误返回）
     *        Read digital level (with error return)
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param err 输出：错误码指针，如果为 NULL 则忽略
     *            Output: error code pointer, ignored if NULL
     * @return 电平值：LOW (0) 或 HIGH (1)，错误时返回 -1
     *         Level: LOW (0) or HIGH (1), returns -1 on error
     */
    int digitalReadWithRes(uint8_t pin, m5pm1_err_t* err);

    // ========================
    // GPIO 功能 (Arduino风格)
    // GPIO Functions (Arduino-style)
    // ========================
    /**
     * @brief 设置引脚模式（Arduino 兼容）
     *        Set pin mode (Arduino compatible)
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param mode 模式：INPUT (0) 或 OUTPUT (1)
     *             Mode: INPUT (0) or OUTPUT (1)
     * @note 此函数不返回错误码，错误会被忽略
     *       This function does not return error code, errors are ignored
     * @note 如果需要错误处理，请使用 pinModeWithRes()
     *       Use pinModeWithRes() if error handling is needed
     */
    void pinMode(uint8_t pin, uint8_t mode);

    /**
     * @brief 写入数字电平（Arduino 兼容）
     *        Write digital level (Arduino compatible)
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param value 电平值：LOW (0) 或 HIGH (1)
     *              Level: LOW (0) or HIGH (1)
     * @note 此函数不返回错误码，错误会被忽略
     *       This function does not return error code, errors are ignored
     */
    void digitalWrite(uint8_t pin, uint8_t value);

    /**
     * @brief 读取数字电平（Arduino 兼容）
     *        Read digital level (Arduino compatible)
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @return 电平值：LOW (0) 或 HIGH (1)，错误时返回 -1
     *         Level: LOW (0) or HIGH (1), returns -1 on error
     * @note 此函数不返回错误码，错误会被忽略
     *       This function does not return error code, errors are ignored
     */
    int digitalRead(uint8_t pin);

    // ========================
    // 高级 GPIO 功能
    // Advanced GPIO Functions
    // ========================
    /**
     * @brief 一键配置 GPIO / Configure GPIO in one call
     * @param pin GPIO 引脚号 (0-4) / GPIO pin number (0-4)
     * @param mode Mode: M5PM1_GPIO_MODE_INPUT / OUTPUT
     * @param value Output level (only for output mode): 0/1
     * @param pull Pull mode: M5PM1_GPIO_PULL_NONE / UP / DOWN
     * @param drive Drive mode: M5PM1_GPIO_DRIVE_PUSHPULL / OPENDRAIN
     * @return M5PM1_OK if successful, error code otherwise
     */
    m5pm1_err_t gpioSet(m5pm1_gpio_num_t pin, m5pm1_gpio_mode_t mode, uint8_t value, m5pm1_gpio_pull_t pull,
                        m5pm1_gpio_drive_t drive);

    /**
     * @brief 设置 GPIO 功能模式
     *        Set GPIO function mode
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param func 功能模式：M5PM1_GPIO_FUNC_GPIO / IRQ / WAKE / OTHER
     *             Function mode: M5PM1_GPIO_FUNC_GPIO / IRQ / WAKE / OTHER
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 切换功能会检查冲突（如 PWM、ADC、NeoPixel）
     *       Switching function checks for conflicts (PWM, ADC, NeoPixel)
     */
    m5pm1_err_t gpioSetFunc(m5pm1_gpio_num_t pin, m5pm1_gpio_func_t func);

    /**
     * @brief 设置 GPIO 输入/输出模式
     *        Set GPIO input/output mode
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param mode 模式：M5PM1_GPIO_MODE_INPUT / OUTPUT
     *             Mode: M5PM1_GPIO_MODE_INPUT / OUTPUT
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t gpioSetMode(m5pm1_gpio_num_t pin, m5pm1_gpio_mode_t mode);

    /**
     * @brief 设置 GPIO 输出电平
     *        Set GPIO output level
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param value 电平值：0 (LOW) 或 1 (HIGH)
     *              Level: 0 (LOW) or 1 (HIGH)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 引脚必须先设置为输出模式
     *       Pin must be set to output mode first
     */
    m5pm1_err_t gpioSetOutput(m5pm1_gpio_num_t pin, uint8_t value);

    /**
     * @brief 读取 GPIO 输入电平
     *        Read GPIO input level
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param value 输出：电平值 (0 或 1)
     *              Output: level value (0 or 1)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t gpioGetInput(m5pm1_gpio_num_t pin, uint8_t* value);

    /**
     * @brief 设置 GPIO 上下拉模式
     *        Set GPIO pull mode
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param pull 上下拉模式：M5PM1_GPIO_PULL_NONE / UP / DOWN
     *             Pull mode: M5PM1_GPIO_PULL_NONE / UP / DOWN
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t gpioSetPull(m5pm1_gpio_num_t pin, m5pm1_gpio_pull_t pull);

    /**
     * @brief 设置 GPIO 驱动模式
     *        Set GPIO drive mode
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param drive 驱动模式：M5PM1_GPIO_DRIVE_PUSHPULL / OPENDRAIN
     *              Drive mode: M5PM1_GPIO_DRIVE_PUSHPULL / OPENDRAIN
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t gpioSetDrive(m5pm1_gpio_num_t pin, m5pm1_gpio_drive_t drive);

    /**
     * @brief 设置 GPIO 唤醒使能
     *        Set GPIO wake enable
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param enable true 启用唤醒，false 禁用
     *               true to enable wake, false to disable
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 唤醒功能用于从睡眠模式唤醒设备
     *       Wake function is used to wake device from sleep mode
     */
    m5pm1_err_t gpioSetWakeEnable(m5pm1_gpio_num_t pin, bool enable);

    /**
     * @brief 设置 GPIO 唤醒边沿
     *        Set GPIO wake edge
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param edge 唤醒边沿：M5PM1_GPIO_WAKE_FALLING / RISING
     *             Wake edge: M5PM1_GPIO_WAKE_FALLING / RISING
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t gpioSetWakeEdge(m5pm1_gpio_num_t pin, m5pm1_gpio_wake_edge_t edge);

    /**
     * @brief 设置 LED_EN 引脚驱动模式 / Set LED_EN pin drive mode
     * @param drive Drive mode: M5PM1_GPIO_DRIVE_PUSHPULL / OPENDRAIN
     * @return M5PM1_OK if successful, error code otherwise
     * @note LED_EN is a special pin (bit 5 in GPIO_DRV register)
     */
    m5pm1_err_t ledEnSetDrive(m5pm1_gpio_drive_t drive);

    /**
     * @brief 打印所有引脚状态（调试用） / Dump all pin status for debugging
     * @return M5PM1_OK if successful, error code otherwise
     */
    m5pm1_err_t dumpPinStatus();

    /**
     * @brief 读回并校验GPIO配置 / Read back and verify GPIO configuration
     * @param enableLog 启用不匹配的警告日志 / Enable warning logs for mismatches
     * @return M5PM1_OK if successful, error code otherwise
     * @note 此函数读取所有GPIO寄存器并与缓存值比对。
     *       如果发现不匹配且enableLog为true，将记录警告日志。
     *       This function reads all GPIO registers and compares with cached values.
     *       If mismatches are found and enableLog is true, warnings will be logged.
     */
    m5pm1_err_t verifyPinConfig(bool enableLog = true);

    /**
     * @brief 获取缓存的引脚状态 / Get cached pin status
     * @param pin GPIO 引脚号 (0-4) / GPIO pin number (0-4)
     * @param status 输出: 缓存的引脚状态 / Output: cached pin status
     * @return M5PM1_OK if successful, error code otherwise
     * @note 此函数从软件缓存读取，无 I2C 事务
     *       This reads from software cache, no I2C transaction
     */
    m5pm1_err_t getPinStatus(m5pm1_gpio_num_t pin, m5pm1_pin_status_t* status);

    /**
     * @brief 获取缓存引脚状态数组指针 / Get cached pin status array pointer
     * @return Pointer to internal pin status array (size: 5)
     * @note 返回内部缓存指针，请勿在外部修改
     *       Returns pointer to internal cache, do not modify externally
     */
    const m5pm1_pin_status_t* getPinStatusArray() const;

    // ========================
    // 电源保持功能
    // Power Hold Functions
    // ========================
    /**
     * @brief 设置 GPIO 电源保持功能
     *        Set GPIO power hold function
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param enable true=启用电源保持，false=禁用电源保持
     *               true=enable power hold, false=disable power hold
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 启用后，即使按下关机按钮，该 GPIO 也会保持供电
     *       When enabled, the GPIO will remain powered even if power-off button is pressed
     */
    m5pm1_err_t gpioSetPowerHold(m5pm1_gpio_num_t pin, bool enable);

    /**
     * @brief 获取 GPIO 电源保持功能状态
     *        Get GPIO power hold function state
     * @param pin GPIO 引脚号 (0-4)
     *            GPIO pin number (0-4)
     * @param enable 输出参数，true=已启用，false=已禁用
     *               Output parameter, true=enabled, false=disabled
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t gpioGetPowerHold(m5pm1_gpio_num_t pin, bool* enable);

    /**
     * @brief 设置 LDO 3.3V 电源保持功能
     *        Set LDO 3.3V power hold function
     * @param enable true=启用电源保持，false=禁用电源保持
     *               true=enable power hold, false=disable power hold
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 启用后，即使按下关机按钮，LDO 3.3V 也会保持输出
     *       When enabled, LDO 3.3V will remain powered even if power-off button is pressed
     */
    m5pm1_err_t ldoSetPowerHold(bool enable);

    /**
     * @brief 获取 LDO 3.3V 电源保持功能状态
     *        Get LDO 3.3V power hold function state
     * @param enable 输出参数，true=已启用，false=已禁用
     *               Output parameter, true=enabled, false=disabled
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t ldoGetPowerHold(bool* enable);

    /**
     * @brief 设置 DCDC 5V 电源保持功能
     *        Set DCDC 5V power hold function
     * @param enable true=启用电源保持，false=禁用电源保持
     *               true=enable power hold, false=disable power hold
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 启用后，即使按下关机按钮，DCDC 5V 也会保持输出
     *       When enabled, DCDC 5V will remain powered even if power-off button is pressed
     */
    m5pm1_err_t boostSetPowerHold(bool enable);

    /**
     * @brief 获取 DCDC 5V 电源保持功能状态
     *        Get DCDC 5V power hold function state
     * @param enable 输出参数，true=已启用，false=已禁用
     *               Output parameter, true=enabled, false=disabled
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t boostGetPowerHold(bool* enable);

    // ========================
    // ADC 功能
    // ADC Functions
    // ========================
    /**
     * @brief 读取 ADC 通道值
     *        Read ADC channel value
     * @param channel ADC 通道：M5PM1_ADC_CH_1 (GPIO1) / M5PM1_ADC_CH_2 (GPIO2) / M5PM1_ADC_CH_TEMP (温度传感器)
     *                ADC channel: M5PM1_ADC_CH_1 (GPIO1) / M5PM1_ADC_CH_2 (GPIO2) / M5PM1_ADC_CH_TEMP (temperature
     * sensor)
     * @param value 输出参数，存储读取的 ADC 值 (0-4095, 12位)
     *              Output parameter, stores the read ADC value (0-4095, 12-bit)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note GPIO 必须配置为 FUNC_OTHER 才能使用 ADC 功能
     *       GPIO must be configured as FUNC_OTHER to use ADC function
     */
    m5pm1_err_t analogRead(m5pm1_adc_channel_t channel, uint16_t* value);

    /**
     * @brief 检查 ADC 是否正在转换
     *        Check if ADC is busy converting
     * @param busy 输出参数，true 表示 ADC 正在转换，false 表示空闲
     *             Output parameter, true if ADC is converting, false if idle
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t isAdcBusy(bool* busy);

    /**
     * @brief 禁用 ADC 功能
     *        Disable ADC function
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 禁用后可降低功耗
     *       Disabling ADC can reduce power consumption
     */
    m5pm1_err_t disableAdc();

    // ========================
    // 温度传感器
    // Temperature Sensor
    // ========================
    /**
     * @brief 读取内部温度传感器值
     *        Read internal temperature sensor value
     * @param temperature 输出参数，存储温度值 (单位：0.1°C，例如 250 表示 25.0°C)
     *                    Output parameter, stores temperature value (unit: 0.1°C, e.g., 250 means 25.0°C)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 内部温度传感器测量的是芯片温度，可能高于环境温度
     *       Internal temperature sensor measures chip temperature, may be higher than ambient temperature
     */
    m5pm1_err_t readTemperature(uint16_t* temperature);

    // ========================
    // PWM 功能
    // PWM Functions
    // ========================
    /**
     * @brief 设置 PWM 频率（全通道共享）
     *        Set PWM frequency (shared by all channels)
     * @param frequency PWM 频率 (Hz)，范围 0-65535
     *                  PWM frequency (Hz), range 0-65535
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 修改频率会影响所有 PWM 通道
     *       Changing frequency affects all PWM channels
     */
    m5pm1_err_t setPwmFrequency(uint16_t frequency);

    /**
     * @brief 获取当前 PWM 频率
     *        Get current PWM frequency
     * @param frequency 输出参数，存储当前 PWM 频率 (Hz)
     *                  Output parameter, stores current PWM frequency (Hz)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getPwmFrequency(uint16_t* frequency);

    /**
     * @brief 设置 PWM 占空比（百分比 0-100）
     *        Set PWM duty cycle (percentage 0-100)
     * @param channel PWM 通道：M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     *                PWM channel: M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     * @param duty 占空比百分比 (0-100)，0=0%, 100=100%
     *             Duty cycle percentage (0-100), 0=0%, 100=100%
     * @param polarity 极性，false=正常，true=反相（默认 false）
     *                 Polarity, false=normal, true=inverted (default false)
     * @param enable 输出使能，true=启用，false=禁用（默认 true）
     *               Output enable, true=enable, false=disable (default true)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note GPIO 必须配置为 FUNC_OTHER 才能使用 PWM 功能
     *       GPIO must be configured as FUNC_OTHER to use PWM function
     */
    m5pm1_err_t setPwmDuty(m5pm1_pwm_channel_t channel, uint8_t duty, bool polarity = false, bool enable = true);

    /**
     * @brief 获取 PWM 占空比（百分比 0-100）
     *        Get PWM duty cycle (percentage 0-100)
     * @param channel PWM 通道：M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     *                PWM channel: M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     * @param duty 输出参数，存储占空比百分比 (0-100)
     *             Output parameter, stores duty cycle percentage (0-100)
     * @param polarity 输出参数，存储极性（false=正常，true=反相）
     *                 Output parameter, stores polarity (false=normal, true=inverted)
     * @param enable 输出参数，存储使能状态（true=启用，false=禁用）
     *               Output parameter, stores enable state (true=enable, false=disable)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getPwmDuty(m5pm1_pwm_channel_t channel, uint8_t* duty, bool* polarity, bool* enable);

    /**
     * @brief 设置 PWM 占空比（12位精度）
     *        Set PWM duty cycle (12-bit precision)
     * @param channel PWM 通道：M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     *                PWM channel: M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     * @param duty12 占空比 (0-4095)，0=0%, 4095=100%
     *               Duty cycle (0-4095), 0=0%, 4095=100%
     * @param polarity 极性，false=正常，true=反相（默认 false）
     *                 Polarity, false=normal, true=inverted (default false)
     * @param enable 输出使能，true=启用，false=禁用（默认 true）
     *               Output enable, true=enable, false=disable (default true)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note GPIO 必须配置为 FUNC_OTHER 才能使用 PWM 功能
     *       GPIO must be configured as FUNC_OTHER to use PWM function
     */
    m5pm1_err_t setPwmDuty12bit(m5pm1_pwm_channel_t channel, uint16_t duty12, bool polarity = false,
                                bool enable = true);

    /**
     * @brief 获取 PWM 占空比（12位精度）
     *        Get PWM duty cycle (12-bit precision)
     * @param channel PWM 通道：M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     *                PWM channel: M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     * @param duty12 输出参数，存储占空比 (0-4095)
     *               Output parameter, stores duty cycle (0-4095)
     * @param polarity 输出参数，存储极性（false=正常，true=反相）
     *                 Output parameter, stores polarity (false=normal, true=inverted)
     * @param enable 输出参数，存储使能状态（true=启用，false=禁用）
     *               Output parameter, stores enable state (true=enable, false=disable)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getPwmDuty12bit(m5pm1_pwm_channel_t channel, uint16_t* duty12, bool* polarity, bool* enable);
    /**
     * @brief Configure PWM in one call
     *        一次性配置 PWM 参数
     * @param channel PWM channel (0-1)
     *               PWM 通道（0-1）
     * @param enable Enable output (true=enable, false=disable)
     *              输出使能（true=启用，false=禁用）
     * @param polarity Polarity (false=normal, true=inverted)
     *                极性（false=正常，true=反相）
     * @param frequency PWM frequency in Hz (0-65535, shared by all channels)
     *                  PWM 频率（0-65535，全通道共享）
     * @param duty12 12-bit duty (0-4095)
     *               12 位占空比（0-4095）
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note This API warns on conflicts but still applies settings
     *       此 API 对冲突仅告警，仍会继续配置
     * @note Changing frequency affects all channels
     *       变更频率会影响所有通道
     */
    m5pm1_err_t setPwmConfig(m5pm1_pwm_channel_t channel, bool enable, bool polarity, uint16_t frequency,
                             uint16_t duty12);

    /**
     * @brief 设置 PWM 输出（Arduino 兼容）
     *        Set PWM output (Arduino compatible)
     * @param channel PWM 通道：M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     *                PWM channel: M5PM1_PWM_CH_0 (GPIO3) / M5PM1_PWM_CH_1 (GPIO4)
     * @param value 占空比 (0-255)，0=0%, 255=100%
     *              Duty cycle (0-255), 0=0%, 255=100%
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 此函数会自动启用 PWM 输出，极性为正常
     *       This function automatically enables PWM output with normal polarity
     * @note GPIO 必须配置为 FUNC_OTHER 才能使用 PWM 功能
     *       GPIO must be configured as FUNC_OTHER to use PWM function
     */
    m5pm1_err_t analogWrite(m5pm1_pwm_channel_t channel, uint8_t value);

    // ========================
    // 电压读取功能
    // Voltage Reading Functions
    // ========================
    /**
     * @brief 读取参考电压
     *        Read reference voltage
     * @param mv 输出参数，存储参考电压值 (mV)
     *           Output parameter, stores reference voltage (mV)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t readVref(uint16_t* mv);

    /**
     * @brief 获取参考电压（readVref 的别名）
     *        Get reference voltage (alias of readVref)
     * @param mv 输出参数，存储参考电压值 (mV)
     *           Output parameter, stores reference voltage (mV)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getRefVoltage(uint16_t* mv);

    /**
     * @brief 读取电池电压
     *        Read battery voltage
     * @param mv 输出参数，存储电池电压值 (mV)
     *           Output parameter, stores battery voltage (mV)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 可用于监测电池电量
     *       Can be used to monitor battery level
     */
    m5pm1_err_t readVbat(uint16_t* mv);

    /**
     * @brief 读取输入电压（VIN）
     *        Read input voltage (VIN)
     * @param mv 输出参数，存储输入电压值 (mV)
     *           Output parameter, stores input voltage (mV)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 可用于检测外部电源是否连接
     *       Can be used to detect if external power is connected
     */
    m5pm1_err_t readVin(uint16_t* mv);

    /**
     * @brief 读取 5V 输入/输出电压
     *        Read 5V input/output voltage
     * @param mv 输出参数，存储 5V 电压值 (mV)
     *           Output parameter, stores 5V voltage (mV)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t read5VInOut(uint16_t* mv);

    // ========================
    // 电源管理
    // Power Management
    // ========================
    /**
     * @brief 获取当前电源来源
     *        Get current power source
     * @param src 输出参数，存储电源来源：M5PM1_PWR_SRC_5VIN / M5PM1_PWR_SRC_5VINOUT / M5PM1_PWR_SRC_BAT /
     * M5PM1_PWR_SRC_UNKNOWN Output parameter, stores power source: M5PM1_PWR_SRC_5VIN / M5PM1_PWR_SRC_5VINOUT /
     * M5PM1_PWR_SRC_BAT / M5PM1_PWR_SRC_UNKNOWN
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getPowerSource(m5pm1_pwr_src_t* src);

    /**
     * @brief 读取唤醒源
     *        Read wake source
     * @param src 输出：唤醒源位掩码
     *            Output: wake source bitmask
     * @param cleanType 读取后的清除行为
     *                  Clean behavior after read
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除本次读到的唤醒源位
     *                  - M5PM1_CLEAN_ONCE: Clear wake source bits read this time
     *                  - M5PM1_CLEAN_ALL: 清除所有唤醒源位
     *                  - M5PM1_CLEAN_ALL: Clear all wake source bits
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getWakeSource(uint8_t* src, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 清除唤醒源标志
     *        Clear wake source flags
     * @param mask 要清除的唤醒源位掩码
     *             Wake source bitmask to clear
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t clearWakeSource(uint8_t mask);

    /**
     * @brief 设置电源配置寄存器
     *        Set power configuration register
     * @param mask 要修改的位掩码
     *             Bitmask of bits to modify
     * @param value 要设置的值
     *              Value to set
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setPowerConfig(uint8_t mask, uint8_t value);

    /**
     * @brief 获取电源配置寄存器
     *        Get power configuration register
     * @param config 输出参数，存储电源配置值
     *               Output parameter, stores power configuration value
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getPowerConfig(uint8_t* config);

    /**
     * @brief 清除电源配置位 / Clear power config bits
     * @param mask 要清除的位 / Bits to clear
     * @return M5PM1_OK if successful, error code otherwise
     */
    m5pm1_err_t clearPowerConfig(uint8_t mask);

    /**
     * @brief 设置充电功能使能
     *        Set charge function enable
     * @param enable true=启用充电，false=禁用充电
     *               true=enable charging, false=disable charging
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setChargeEnable(bool enable);

    /**
     * @brief 设置 DCDC 5V 输出使能
     *        Set DCDC 5V output enable
     * @param enable true=启用 DCDC，false=禁用 DCDC
     *               true=enable DCDC, false=disable DCDC
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setDcdcEnable(bool enable);

    /**
     * @brief 设置 LDO 3.3V 输出使能
     *        Set LDO 3.3V output enable
     * @param enable true=启用 LDO，false=禁用 LDO
     *               true=enable LDO, false=disable LDO
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setLdoEnable(bool enable);

    /**
     * @brief 设置 BOOST 5V 输入/输出使能
     *        Set BOOST 5V input/output enable
     * @param enable true=启用 BOOST 5V，false=禁用 BOOST 5V
     *               true=enable BOOST 5V, false=disable BOOST 5V
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setBoostEnable(bool enable);

    /**
     * @brief 设置 LED_EN 引脚默认电平
     *        Set LED_EN pin default level
     * @param level true=高电平, false=低电平
     *              true=High Level, false=Low Level
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setLedEnLevel(bool level);

    // ========================
    // 电池功能
    // Battery Functions
    // ========================
    /**
     * @brief 设置电池低压保护阈值
     *        Set battery low voltage protection threshold
     * @param mv 低压保护阈值 (mV)
     *           Low voltage protection threshold (mV)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 当电池电压低于此阈值时，系统会自动关机
     *       System will automatically shut down when battery voltage is below this threshold
     */
    m5pm1_err_t setBatteryLvp(uint16_t mv);

    // ========================
    // 看门狗功能
    // Watchdog Functions
    // ========================
    /**
     * @brief 设置看门狗超时时间
     *        Set watchdog timeout
     * @param timeout_sec 超时时间（秒），0=禁用看门狗
     *                    Timeout in seconds, 0=disable watchdog
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 超时后系统会自动复位，需要定期调用 wdtFeed() 喂狗
     *       System will auto-reboot after timeout, need to call wdtFeed() periodically
     */
    m5pm1_err_t wdtSet(uint8_t timeout_sec);

    /**
     * @brief 喂狗（重置看门狗计数器）
     *        Feed watchdog (reset watchdog counter)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 需要在超时前定期调用此函数，防止系统复位
     *       Must call this function periodically before timeout to prevent system reboot
     */
    m5pm1_err_t wdtFeed();

    /**
     * @brief 获取看门狗计数值
     *        Get watchdog counter value
     * @param count 输出参数，存储当前计数值（秒）
     *              Output parameter, stores current counter value (seconds)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t wdtGetCount(uint8_t* count);

    // ========================
    // 定时器功能
    // Timer Functions
    // ========================
    /**
     * @brief 设置定时器
     *        Set timer
     * @param seconds 定时时间（秒，最大214748364）
     *                Timer duration in seconds (max 214748364)
     * @param action 定时器到期后的动作：
     *               Action after timer expires:
     *               - M5PM1_TIM_ACTION_STOP: 停止，无动作 / Stop, no action
     *               - M5PM1_TIM_ACTION_FLAG: 仅设置标志 / Set flag only
     *               - M5PM1_TIM_ACTION_REBOOT: 系统复位 / System reboot
     *               - M5PM1_TIM_ACTION_POWERON: 开机 / Power on
     *               - M5PM1_TIM_ACTION_POWEROFF: 关机 / Power off
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t timerSet(uint32_t seconds, m5pm1_tim_action_t action);

    /**
     * @brief 清除定时器
     *        Clear timer
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t timerClear();

    // ========================
    // 按钮功能
    // Button Functions
    // ========================
    /**
     * @brief 配置按钮事件类型和延时
     *        Configure button event type and delay
     * @param type 按钮事件类型：M5PM1_BTN_TYPE_CLICK (单击) / M5PM1_BTN_TYPE_DOUBLE (双击) / M5PM1_BTN_TYPE_LONG (长按)
     *             Button event type: M5PM1_BTN_TYPE_CLICK (single click) / M5PM1_BTN_TYPE_DOUBLE (double click) /
     * M5PM1_BTN_TYPE_LONG (long press)
     * @param delay 延时配置：
     *              - 单击: M5PM1_BTN_CLICK_DELAY_125MS / 250MS / 500MS / 1000MS
     *              - 双击: M5PM1_BTN_DOUBLE_CLICK_DELAY_125MS / 250MS / 500MS / 1000MS
     *              - 长按: M5PM1_BTN_LONG_PRESS_DELAY_1000MS / 2000MS / 3000MS / 4000MS
     *              Delay configuration:
     *              - Click: M5PM1_BTN_CLICK_DELAY_125MS / 250MS / 500MS / 1000MS
     *              - Double click: M5PM1_BTN_DOUBLE_CLICK_DELAY_125MS / 250MS / 500MS / 1000MS
     *              - Long press: M5PM1_BTN_LONG_PRESS_DELAY_1000MS / 2000MS / 3000MS / 4000MS
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t btnSetConfig(m5pm1_btn_type_t type, m5pm1_btn_delay_t delay);

    /**
     * @brief 获取按钮当前状态
     *        Get button current state
     * @param pressed 输出参数，true=按钮当前被按下，false=按钮未被按下
     *                Output parameter, true=button is currently pressed, false=button is not pressed
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 此函数读取按钮的实时状态，不会清除标志
     *       This function reads the real-time button state, does not clear flags
     */
    m5pm1_err_t btnGetState(bool* pressed);

    /**
     * @brief 获取按钮标志（读后自动清除）
     *        Get button flag (auto-clear after read)
     * @param wasPressed 输出参数，true=按钮曾被按下，false=按钮未被按下
     *                   Output parameter, true=button was pressed, false=button was not pressed
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 此函数读取后会自动清除标志，适合用于检测按钮事件
     *       This function auto-clears the flag after reading, suitable for detecting button events
     */
    m5pm1_err_t btnGetFlag(bool* wasPressed);

    /**
     * @brief 设置单击复位功能禁用
     *        Set single click reset function disable
     * @param disable true=禁用单击复位，false=启用单击复位
     *                true=disable single click reset, false=enable single click reset
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 禁用后，单击按钮不会触发系统复位
     *       After disabling, single click will not trigger system reset
     */
    m5pm1_err_t setSingleResetDisable(bool disable);

    /**
     * @brief 获取单击复位功能禁用状态
     *        Get single click reset function disable state
     * @param disabled 输出参数，true=已禁用，false=已启用
     *                 Output parameter, true=disabled, false=enabled
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getSingleResetDisable(bool* disabled);

    /**
     * @brief 设置双击关机功能禁用
     *        Set double click power-off function disable
     * @param disable true=禁用双击关机，false=启用双击关机
     *                true=disable double click power-off, false=enable double click power-off
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 禁用后，双击按钮不会触发系统关机
     *       After disabling, double click will not trigger system power-off
     */
    m5pm1_err_t setDoubleOffDisable(bool disable);

    /**
     * @brief 获取双击关机功能禁用状态
     *        Get double click power-off function disable state
     * @param disabled 输出参数，true=已禁用，false=已启用
     *                 Output parameter, true=disabled, false=enabled
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getDoubleOffDisable(bool* disabled);

    // ========================
    // 中断功能
    // IRQ Functions
    // ========================
    /**
     * @brief 读取 GPIO 中断状态
     *        Read GPIO interrupt status
     * @param status 输出：状态位掩码
     *               Output: status bitmask
     * @param cleanType 读取后的清除行为
     *                  Clean behavior after read
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除已触发位（一次清除）
     *                  - M5PM1_CLEAN_ONCE: Clear triggered bits in this read (single operation)
     *                  - M5PM1_CLEAN_ALL: 清除所有位
     *                  - M5PM1_CLEAN_ALL: Clear all bits
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetGpioStatus(uint8_t* status, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 读取 GPIO 中断状态（返回枚举值，用户友好）
     *        Read GPIO interrupt status (returns enum, user-friendly)
     * @param gpio_num 输出：触发中断的 GPIO 枚举
     *                 Output: triggered GPIO enum
     * @param cleanType 清除类型
     *                  Clean type
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除当前返回的 GPIO 中断（从低位往高位逐个）
     *                  - M5PM1_CLEAN_ONCE: Clean current GPIO (low to high, one at a time)
     *                  - M5PM1_CLEAN_ALL: 清除所有 GPIO 中断
     *                  - M5PM1_CLEAN_ALL: Clean all GPIO interrupts
     * @return M5PM1_OK 成功，M5PM1_ERR_xxx 失败
     *         M5PM1_OK on success, error code otherwise
     * @note 如果有多个 GPIO 同时触发，每次调用返回一个（从低位到高位）
     *       If multiple GPIOs triggered, returns one per call (low to high)
     * @note 使用 M5PM1_CLEAN_ONCE 可以逐个处理多个中断
     *       Use M5PM1_CLEAN_ONCE to handle multiple interrupts one by one
     * @note 如果没有中断，返回 M5PM1_IRQ_GPIO_NONE
     *       Returns M5PM1_IRQ_GPIO_NONE if no interrupt
     */
    m5pm1_err_t irqGetGpioStatusEnum(m5pm1_irq_gpio_t* gpio_num, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 清除所有 GPIO 中断状态
     *        Clear all GPIO interrupt status
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqClearGpioAll();

    /**
     * @brief 读取系统中断状态
     *        Read system interrupt status
     * @param status 输出：状态位掩码
     *               Output: status bitmask
     * @param cleanType 读取后的清除行为
     *                  Clean behavior after read
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除已触发位（一次清除）
     *                  - M5PM1_CLEAN_ONCE: Clear triggered bits in this read (single operation)
     *                  - M5PM1_CLEAN_ALL: 清除所有位
     *                  - M5PM1_CLEAN_ALL: Clear all bits
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetSysStatus(uint8_t* status, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 读取系统中断状态（返回枚举值，用户友好）
     *        Read system interrupt status (returns enum, user-friendly)
     * @param sys_irq 输出：触发中断的系统事件枚举
     *                Output: triggered system event enum
     * @param cleanType 清除类型
     *                  Clean type
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除当前返回的系统中断（从低位往高位逐个）
     *                  - M5PM1_CLEAN_ONCE: Clean current event (low to high, one at a time)
     *                  - M5PM1_CLEAN_ALL: 清除所有系统中断
     *                  - M5PM1_CLEAN_ALL: Clean all system interrupts
     * @return M5PM1_OK 成功，M5PM1_ERR_xxx 失败
     *         M5PM1_OK on success, error code otherwise
     * @note 如果有多个事件同时触发，每次调用返回一个（从低位到高位）
     *       If multiple events triggered, returns one per call (low to high)
     * @note 使用 M5PM1_CLEAN_ONCE 可以逐个处理多个中断
     *       Use M5PM1_CLEAN_ONCE to handle multiple interrupts one by one
     * @note 如果没有中断，返回 M5PM1_IRQ_SYS_NONE
     *       Returns M5PM1_IRQ_SYS_NONE if no interrupt
     */
    m5pm1_err_t irqGetSysStatusEnum(m5pm1_irq_sys_t* sys_irq, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 清除所有系统中断状态
     *        Clear all system interrupt status
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqClearSysAll();

    /**
     * @brief 读取按钮中断状态
     *        Read button interrupt status
     * @param status 输出：状态位掩码
     *               Output: status bitmask
     * @param cleanType 读取后的清除行为
     *                  Clean behavior after read
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除已触发位（一次清除）
     *                  - M5PM1_CLEAN_ONCE: Clear triggered bits in this read (single operation)
     *                  - M5PM1_CLEAN_ALL: 清除所有位
     *                  - M5PM1_CLEAN_ALL: Clear all bits
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetBtnStatus(uint8_t* status, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 读取按钮中断状态（返回枚举值，用户友好）
     *        Read button interrupt status (returns enum, user-friendly)
     * @param btn_irq 输出：触发中断的按钮事件枚举
     *                Output: triggered button event enum
     * @param cleanType 清除类型
     *                  Clean type
     *                  - M5PM1_CLEAN_NONE: 不清除
     *                  - M5PM1_CLEAN_NONE: No clean
     *                  - M5PM1_CLEAN_ONCE: 清除当前返回的按钮中断（从低位往高位逐个）
     *                  - M5PM1_CLEAN_ONCE: Clean current event (low to high, one at a time)
     *                  - M5PM1_CLEAN_ALL: 清除所有按钮中断
     *                  - M5PM1_CLEAN_ALL: Clean all button interrupts
     * @return M5PM1_OK 成功，M5PM1_ERR_xxx 失败
     *         M5PM1_OK on success, error code otherwise
     * @note 如果有多个事件同时触发，每次调用返回一个（从低位到高位）
     *       If multiple events triggered, returns one per call (low to high)
     * @note 使用 M5PM1_CLEAN_ONCE 可以逐个处理多个中断
     *       Use M5PM1_CLEAN_ONCE to handle multiple interrupts one by one
     * @note 如果没有中断，返回 M5PM1_IRQ_BTN_NONE
     *       Returns M5PM1_IRQ_BTN_NONE if no interrupt
     */
    m5pm1_err_t irqGetBtnStatusEnum(m5pm1_irq_btn_t* btn_irq, m5pm1_clean_type_t cleanType = M5PM1_CLEAN_NONE);

    /**
     * @brief 清除所有按钮中断状态
     *        Clear all button interrupt status
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqClearBtnAll();

    /**
     * @brief Set single GPIO pin interrupt mask
     *        设置单个 GPIO 引脚中断屏蔽
     * @param pin GPIO pin number (0-4) / GPIO 引脚号
     * @param mask Mask control: M5PM1_IRQ_MASK_DISABLE(unmask) / M5PM1_IRQ_MASK_ENABLE(mask)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqSetGpioMask(m5pm1_irq_gpio_t pin, m5pm1_irq_mask_ctrl_t mask);

    /**
     * @brief 获取单个 GPIO 引脚中断屏蔽状态
     *        Get single GPIO pin interrupt mask state
     * @param gpio GPIO 中断标志位 (M5PM1_IRQ_GPIO0 ~ M5PM1_IRQ_GPIO4)
     *             GPIO interrupt flag (M5PM1_IRQ_GPIO0 ~ M5PM1_IRQ_GPIO4)
     * @note 不支持 M5PM1_IRQ_GPIO_ALL / M5PM1_IRQ_GPIO_NONE，请使用 irqGetGpioMaskBits()
     *       M5PM1_IRQ_GPIO_ALL / M5PM1_IRQ_GPIO_NONE not supported, use irqGetGpioMaskBits()
     * @param mask 输出参数，存储屏蔽状态：M5PM1_IRQ_MASK_DISABLE (未屏蔽) / M5PM1_IRQ_MASK_ENABLE (已屏蔽)
     *             Output parameter, stores mask state: M5PM1_IRQ_MASK_DISABLE (unmasked) / M5PM1_IRQ_MASK_ENABLE
     * (masked)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetGpioMask(m5pm1_irq_gpio_t gpio, m5pm1_irq_mask_ctrl_t* mask);

    /**
     * @brief 一次性设置所有 GPIO 中断屏蔽 / Set all GPIO interrupt mask at once
     * @param mask 屏蔽控制：M5PM1_IRQ_MASK_DISABLE (全部不屏蔽) / M5PM1_IRQ_MASK_ENABLE (全部屏蔽)
     *             Mask control: M5PM1_IRQ_MASK_DISABLE (unmask all) / M5PM1_IRQ_MASK_ENABLE (mask all)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqSetGpioMaskAll(m5pm1_irq_mask_ctrl_t mask);

    /**
     * @brief 获取所有 GPIO 中断屏蔽状态
     *        Get all GPIO interrupt mask state
     * @param mask 输出参数，存储所有 GPIO 引脚的屏蔽位掩码
     *             Output parameter, stores bitmask for all GPIO pins
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetGpioMaskBits(uint8_t* mask);

    /**
     * @brief Set single system event interrupt mask
     *        设置单个系统事件中断屏蔽
     * @param event 系统事件枚举 (M5PM1_IRQ_SYS_5VIN_INSERT ~ M5PM1_IRQ_SYS_BAT_REMOVE)
     *              System event enum (M5PM1_IRQ_SYS_5VIN_INSERT ~ M5PM1_IRQ_SYS_BAT_REMOVE)
     * @note 不支持 M5PM1_IRQ_SYS_ALL / M5PM1_IRQ_SYS_NONE，请使用 irqSetSysMaskAll()
     *       M5PM1_IRQ_SYS_ALL / M5PM1_IRQ_SYS_NONE not supported, use irqSetSysMaskAll()
     * @param mask Mask control: M5PM1_IRQ_MASK_DISABLE(unmask) / M5PM1_IRQ_MASK_ENABLE(mask)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqSetSysMask(m5pm1_irq_sys_t event, m5pm1_irq_mask_ctrl_t mask);

    /**
     * @brief 获取单个系统事件中断屏蔽状态
     *        Get single system event interrupt mask state
     * @param event 系统事件枚举 (M5PM1_IRQ_SYS_5VIN_INSERT ~ M5PM1_IRQ_SYS_BAT_REMOVE)
     *              System event enum (M5PM1_IRQ_SYS_5VIN_INSERT ~ M5PM1_IRQ_SYS_BAT_REMOVE)
     * @note 不支持 M5PM1_IRQ_SYS_ALL / M5PM1_IRQ_SYS_NONE，请使用 irqGetSysMaskBits()
     *       M5PM1_IRQ_SYS_ALL / M5PM1_IRQ_SYS_NONE not supported, use irqGetSysMaskBits()
     * @param mask 输出参数，存储屏蔽状态：M5PM1_IRQ_MASK_DISABLE (未屏蔽) / M5PM1_IRQ_MASK_ENABLE (已屏蔽)
     *             Output parameter, stores mask state: M5PM1_IRQ_MASK_DISABLE (unmasked) / M5PM1_IRQ_MASK_ENABLE
     * (masked)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetSysMask(m5pm1_irq_sys_t event, m5pm1_irq_mask_ctrl_t* mask);

    /**
     * @brief 一次性设置所有系统中断屏蔽 / Set all system interrupt mask at once
     * @param mask 屏蔽控制：M5PM1_IRQ_MASK_DISABLE (全部不屏蔽) / M5PM1_IRQ_MASK_ENABLE (全部屏蔽)
     *             Mask control: M5PM1_IRQ_MASK_DISABLE (unmask all) / M5PM1_IRQ_MASK_ENABLE (mask all)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqSetSysMaskAll(m5pm1_irq_mask_ctrl_t mask);

    /**
     * @brief 获取所有系统中断屏蔽状态
     *        Get all system interrupt mask state
     * @param mask 输出参数，存储所有系统事件的屏蔽位掩码
     *             Output parameter, stores bitmask for all system events
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetSysMaskBits(uint8_t* mask);

    /**
     * @brief Set single button event interrupt mask
     *        设置单个按钮事件中断屏蔽
     * @param type 按钮事件类型：M5PM1_IRQ_BTN_CLICK / WAKE / DOUBLE
     *             Button event type: M5PM1_IRQ_BTN_CLICK / WAKE / DOUBLE
     * @note 不支持 M5PM1_IRQ_BTN_ALL / M5PM1_IRQ_BTN_NONE，请使用 irqSetBtnMaskAll()
     *       M5PM1_IRQ_BTN_ALL / M5PM1_IRQ_BTN_NONE not supported, use irqSetBtnMaskAll()
     * @param mask Mask control: M5PM1_IRQ_MASK_DISABLE(unmask) / M5PM1_IRQ_MASK_ENABLE(mask)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqSetBtnMask(m5pm1_irq_btn_t type, m5pm1_irq_mask_ctrl_t mask);

    /**
     * @brief 获取单个按钮事件中断屏蔽状态
     *        Get single button event interrupt mask state
     * @param type 按钮事件类型：M5PM1_IRQ_BTN_CLICK / WAKE / DOUBLE
     *             Button event type: M5PM1_IRQ_BTN_CLICK / WAKE / DOUBLE
     * @note 不支持 M5PM1_IRQ_BTN_ALL / M5PM1_IRQ_BTN_NONE，请使用 irqGetBtnMaskBits()
     *       M5PM1_IRQ_BTN_ALL / M5PM1_IRQ_BTN_NONE not supported, use irqGetBtnMaskBits()
     * @param mask 输出参数，存储屏蔽状态：M5PM1_IRQ_MASK_DISABLE (未屏蔽) / M5PM1_IRQ_MASK_ENABLE (已屏蔽)
     *             Output parameter, stores mask state: M5PM1_IRQ_MASK_DISABLE (unmasked) / M5PM1_IRQ_MASK_ENABLE
     * (masked)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetBtnMask(m5pm1_irq_btn_t type, m5pm1_irq_mask_ctrl_t* mask);

    /**
     * @brief 一次性设置所有按钮中断屏蔽 / Set all button interrupt mask at once
     * @param mask 屏蔽控制：M5PM1_IRQ_MASK_DISABLE (全部不屏蔽) / M5PM1_IRQ_MASK_ENABLE (全部屏蔽)
     *             Mask control: M5PM1_IRQ_MASK_DISABLE (unmask all) / M5PM1_IRQ_MASK_ENABLE (mask all)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqSetBtnMaskAll(m5pm1_irq_mask_ctrl_t mask);
    /**
     * @brief 获取所有按钮中断屏蔽状态
     *        Get all button interrupt mask state
     * @param mask 输出参数，存储所有按钮事件的屏蔽位掩码
     *             Output parameter, stores bitmask for all button events
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t irqGetBtnMaskBits(uint8_t* mask);

    // ========================
    // 系统命令
    // System Commands
    // ========================
    /**
     * @brief 发送系统命令
     *        Send system command
     * @param cmd 系统命令类型
     *            System command type
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t sysCmd(m5pm1_sys_cmd_t cmd);
    /**
     * @brief 请求关机
     *        Request shutdown
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t shutdown();
    /**
     * @brief 请求重启
     *        Request reboot
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t reboot();
    /**
     * @brief 进入下载模式
     *        Enter download mode
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t enterDownloadMode();

    /**
     * @brief 设置下载模式锁 / Set download mode lock
     * @param lock true=lock(disable download), false=unlock(enable download)
     *             true=锁定(禁止下载), false=解锁(允许下载)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note This controls bit 7 (DL_LOCK) in BTN_CFG register
     */
    m5pm1_err_t setDownloadLock(bool lock);
    /**
     * @brief 获取下载模式锁状态
     *        Get download mode lock state
     * @param lock 输出参数，存储锁定状态
     *             Output parameter, stores lock state
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getDownloadLock(bool* lock);

    // ========================
    // NeoPixel 功能
    // NeoPixel Functions
    // ========================
    /**
     * @brief 一键配置 NeoPixel LED
     *        Configure NeoPixel LEDs in one call
     * @param colors RGB 颜色数组（m5pm1_rgb_t 格式）
     *               RGB color array (m5pm1_rgb_t format)
     * @param arraySize 颜色数组大小
     *                  Size of color array
     * @param count LED 数量 (1-32)
     *              LED count (1-32)
     * @param autoRefresh 是否立即刷新显示（默认 true）
     *                    Whether to refresh immediately (default true)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setLeds(const m5pm1_rgb_t* colors, uint8_t arraySize, uint8_t count, bool autoRefresh = true);

    /**
     * @brief 设置 NeoPixel LED 数量
     *        Set NeoPixel LED count
     * @param count LED 数量 (1-32)
     *              LED count (1-32)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 寄存器为6位宽度[5:0]，固件最大支持32个LED
     *       Register is 6-bit wide [5:0], firmware supports max 32 LEDs
     */
    m5pm1_err_t setLedCount(uint8_t count);

    /**
     * @brief 设置单个 LED 颜色（RGB 分量）
     *        Set single LED color (RGB components)
     * @param index LED 索引 (0-31)
     *              LED index (0-31)
     * @param r 红色分量 (0-255)
     *          Red component (0-255)
     * @param g 绿色分量 (0-255)
     *          Green component (0-255)
     * @param b 蓝色分量 (0-255)
     *          Blue component (0-255)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 设置后需要调用 refreshLeds() 才会生效
     *       Need to call refreshLeds() to apply changes
     */
    m5pm1_err_t setLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief 设置单个 LED 颜色（RGB 结构体）
     *        Set single LED color (RGB struct)
     * @param index LED 索引 (0-31)
     *              LED index (0-31)
     * @param color RGB 颜色结构体
     *              RGB color struct
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 设置后需要调用 refreshLeds() 才会生效
     *       Need to call refreshLeds() to apply changes
     */
    m5pm1_err_t setLedColor(uint8_t index, m5pm1_rgb_t color);

    /**
     * @brief 刷新 LED 显示
     *        Refresh LED display
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 调用此函数后，之前设置的颜色才会显示到 LED 上
     *       Call this function to apply previously set colors to LEDs
     */
    m5pm1_err_t refreshLeds();

    /**
     * @brief 禁用 NeoPixel LED
     *        Disable NeoPixel LEDs
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 禁用后 LED 会熄灭，可降低功耗
     *       LEDs will turn off after disabling, reducing power consumption
     */
    m5pm1_err_t disableLeds();

    // ========================
    // AW8737A 脉冲功能
    // AW8737A Pulse Functions
    // ========================
    /**
     * @brief 设置 AW8737A 脉冲次数并可选刷新
     *        Set AW8737A pulse count with optional refresh
     * @param pin GPIO 引脚号
     *            GPIO pin number
     * @param num 脉冲次数
     *            Pulse count
     * @param refresh 刷新方式
     *                Refresh control
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 如果引脚不是输出模式，会自动配置为推挽输出；如果已是输出模式则保持原有配置
     *       If pin is not output mode, it will be auto-configured as push-pull output; if already output, keeps current
     * config
     * @note 如果使用开漏输出，需要外部上拉电阻
     *       If using open-drain output, external pull-up is required
     * @note 当 refresh=NOW 时，执行后会有 20ms 延迟
     *       When refresh=NOW, there will be a 20ms delay after execution
     */
    m5pm1_err_t setAw8737aPulse(m5pm1_gpio_num_t pin, m5pm1_aw8737a_pulse_t num,
                                m5pm1_aw8737a_refresh_t refresh = M5PM1_AW8737A_REFRESH_NOW);
    /**
     * @brief 刷新 AW8737A 脉冲配置
     *        Refresh AW8737A pulse configuration
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 需要先调用 setAw8737aPulse 并设置 refresh=WAIT，然后调用此函数触发
     *       Call setAw8737aPulse with refresh=WAIT first, then call this to trigger
     * @note 如果引脚不是输出模式，会自动配置为推挽输出；如果已是输出模式则保持原有配置
     *       If pin is not output mode, it will be auto-configured as push-pull output; if already output, keeps current
     * config
     * @note 如果使用开漏输出，需要外部上拉电阻
     *       If using open-drain output, external pull-up is required
     * @note 执行后会有 20ms 延迟
     *       There will be a 20ms delay after execution
     */
    m5pm1_err_t refreshAw8737aPulse();

    /**
     * @brief 设置 AW8737A 增益模式并可选刷新
     *        Set AW8737A gain mode with optional refresh
     * @param pin GPIO 引脚号
     *            GPIO pin number
     * @param mode 增益模式 (MODE_1 到 MODE_4)
     *             Gain mode (MODE_1 to MODE_4)
     * @param refresh 刷新方式
     *                Refresh control
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 如果引脚不是输出模式，会自动配置为推挽输出；如果已是输出模式则保持原有配置
     *       If pin is not output mode, it will be auto-configured as push-pull output; if already output, keeps current
     * config
     * @note 如果使用开漏输出，需要外部上拉电阻
     *       If using open-drain output, external pull-up is required
     * @note 当 refresh=NOW 时，执行后会有 20ms 延迟
     *       When refresh=NOW, there will be a 20ms delay after execution
     */
    m5pm1_err_t setAw8737aMode(m5pm1_gpio_num_t pin, m5pm1_aw8737a_mode_t mode,
                               m5pm1_aw8737a_refresh_t refresh = M5PM1_AW8737A_REFRESH_NOW);

    /**
     * @brief 刷新 AW8737A 模式配置
     *        Refresh AW8737A mode configuration
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 需要先调用 setAw8737aMode 并设置 refresh=WAIT，然后调用此函数触发
     *       Call setAw8737aMode with refresh=WAIT first, then call this to trigger
     * @note 如果引脚不是输出模式，会自动配置为推挽输出；如果已是输出模式则保持原有配置
     *       If pin is not output mode, it will be auto-configured as push-pull output; if already output, keeps current
     * config
     * @note 如果使用开漏输出，需要外部上拉电阻
     *       If using open-drain output, external pull-up is required
     * @note 执行后会有 20ms 延迟
     *       There will be a 20ms delay after execution
     */
    m5pm1_err_t refreshAw8737aMode();

    // ========================
    // RTC RAM 功能
    // RTC RAM Functions
    // ========================
    /**
     * @brief 写入 RTC RAM
     *        Write to RTC RAM
     * @param offset 偏移地址 (0-31)
     *               Offset address (0-31)
     * @param data 要写入的数据
     *             Data to write
     * @param len 数据长度（字节）
     *            Data length (bytes)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note RTC RAM 总共 32 字节，掉电和睡眠时数据保持
     *       RTC RAM has 32 bytes total, data persists during power-off and sleep
     */
    m5pm1_err_t writeRtcRAM(uint8_t offset, const uint8_t* data, uint8_t len);

    /**
     * @brief 读取 RTC RAM
     *        Read from RTC RAM
     * @param offset 偏移地址 (0-31)
     *               Offset address (0-31)
     * @param data 存储读取数据的缓冲区
     *             Buffer to store read data
     * @param len 要读取的数据长度（字节）
     *            Length of data to read (bytes)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note RTC RAM 总共 32 字节，掉电和睡眠时数据保持
     *       RTC RAM has 32 bytes total, data persists during power-off and sleep
     */
    m5pm1_err_t readRtcRAM(uint8_t offset, uint8_t* data, uint8_t len);

    // ========================
    // I2C 配置
    // I2C Configuration
    // ========================
    /**
     * @brief 配置 I2C 参数（睡眠时间和速度）
     *        Configure I2C parameters (sleep time and speed)
     * @param sleepTime I2C 睡眠时间（秒），超时后进入低功耗模式
     *                  I2C sleep time (seconds), enter low power mode after timeout
     * @param speed I2C 速度：M5PM1_I2C_SPEED_100K (标准模式) / M5PM1_I2C_SPEED_400K (快速模式)，默认 100KHz
     *              I2C speed: M5PM1_I2C_SPEED_100K (standard mode) / M5PM1_I2C_SPEED_400K (fast mode), default 100KHz
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 切换到 400KHz 后需要重新初始化 I2C 总线
     *       Need to re-initialize I2C bus after switching to 400KHz
     */
    m5pm1_err_t setI2cConfig(uint8_t sleepTime, m5pm1_i2c_speed_t speed = M5PM1_I2C_SPEED_100K);

    /**
     * @brief 切换 I2C 速度
     *        Switch I2C speed
     * @param speed I2C 速度：M5PM1_I2C_SPEED_100K (标准模式) / M5PM1_I2C_SPEED_400K (快速模式)
     *              I2C speed: M5PM1_I2C_SPEED_100K (standard mode) / M5PM1_I2C_SPEED_400K (fast mode)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     * @note 切换速度后需要重新初始化 I2C 总线
     *       Need to re-initialize I2C bus after switching speed
     */
    m5pm1_err_t switchI2cSpeed(m5pm1_i2c_speed_t speed);

    /**
     * @brief 获取当前 I2C 速度
     *        Get current I2C speed
     * @param speed 输出参数，存储当前 I2C 速度
     *              Output parameter, stores current I2C speed
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getI2cSpeed(m5pm1_i2c_speed_t* speed);

    /**
     * @brief 设置 I2C 睡眠时间
     *        Set I2C sleep time
     * @param seconds 睡眠时间（秒），超时后进入低功耗模式
     *                Sleep time (seconds), enter low power mode after timeout
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t setI2cSleepTime(uint8_t seconds);

    /**
     * @brief 获取 I2C 睡眠时间
     *        Get I2C sleep time
     * @param seconds 输出参数，存储睡眠时间（秒）
     *                Output parameter, stores sleep time (seconds)
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getI2cSleepTime(uint8_t* seconds);

    // ========================
    // 自动唤醒功能
    // Auto Wake Feature
    // ========================
    /**
     * @brief 启用/禁用 I2C 操作前的自动唤醒信号
     *        Enable/disable automatic wake signal before I2C operations
     * @param enable true to enable auto-wake, false to disable
     * @note 当 PM1 进入睡眠模式（I2C 睡眠超时）后，需要在 SDA 上发送
     *       START 信号来唤醒。此功能会在需要时自动发送唤醒信号。
     *       When PM1 enters sleep mode (I2C sleep timeout), it needs a
     *       START signal on SDA to wake up. This feature automatically
     *       sends the wake signal when needed.
     * @note 即使不启用此选项，通讯在大多数情况下也能成功，因为第一次
     *       I2C 传输本身就能唤醒设备。启用此选项可确保可靠性。
     *       Even without enabling this option, communication will likely
     *       succeed in most cases, as the first I2C transaction itself
     *       can wake the device. Enable this for guaranteed reliability.
     */
    void setAutoWakeEnable(bool enable);

    /**
     * @brief 检查自动唤醒是否启用 / Check if auto wake is enabled
     * @return true if enabled
     */
    bool isAutoWakeEnabled() const;

    /**
     * @brief 手动发送唤醒信号到 PM1 / Manually send wake signal to PM1
     * @return M5PM1_OK if successful, error code otherwise
     */
    m5pm1_err_t sendWakeSignal();

    // ========================
    // 状态快照功能
    // State Snapshot Functions
    // ========================
    /**
     * @brief 设置自动快照功能
     *        Enable or disable auto snapshot
     * @param enable 是否启用
     *               Enable or disable
     */
    void setAutoSnapshot(bool enable);
    /**
     * @brief 获取自动快照是否启用
     *        Check whether auto snapshot is enabled
     * @return true 表示已启用，false 表示未启用
     *         true if enabled, false otherwise
     */
    bool isAutoSnapshotEnabled() const;
    /**
     * @brief 更新状态快照
     *        Update state snapshot
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t updateSnapshot();

    // ========================
    // 快照验证
    // Snapshot Verification
    // ========================
    /**
     * @brief 验证当前快照一致性
     *        Verify snapshot consistency
     * @return 快照验证结果
     *         Snapshot verification result
     */
    m5pm1_snapshot_verify_t verifySnapshot();

    // ========================
    // 配置验证
    // Configuration Validation
    // ========================
    /**
     * @brief 校验配置项与当前状态
     *        Validate configuration against current state
     * @param pin GPIO 引脚号
     *            GPIO pin number
     * @param configType 配置类型
     *                   Configuration type
     * @param enable 是否启用校验（默认启用）
     *               Enable validation (default true)
     * @return 校验结果
     *         Validation result
     */
    m5pm1_validation_t validateConfig(uint8_t pin, m5pm1_config_type_t configType, bool enable = true);

    // ========================
    // 缓存状态查询函数
    // Cached State Query Functions
    // ========================
    /**
     * @brief 获取缓存的 PWM 频率
     *        Get cached PWM frequency
     * @param frequency 输出参数，存储缓存频率
     *                  Output parameter, stores cached frequency
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedPwmFrequency(uint16_t* frequency);
    /**
     * @brief 获取缓存的 PWM 状态
     *        Get cached PWM state
     * @param channel PWM 通道
     *                PWM channel
     * @param duty12 输出参数，存储 12 位占空比
     *               Output parameter, stores 12-bit duty
     * @param enable 输出参数，存储使能状态
     *               Output parameter, stores enable state
     * @param polarity 输出参数，存储极性
     *                 Output parameter, stores polarity
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedPwmState(m5pm1_pwm_channel_t channel, uint16_t* duty12, bool* enable, bool* polarity);
    /**
     * @brief 获取缓存的 ADC 状态
     *        Get cached ADC state
     * @param channel 输出参数，存储 ADC 通道
     *                Output parameter, stores ADC channel
     * @param busy 输出参数，存储忙状态
     *             Output parameter, stores busy state
     * @param lastValue 输出参数，存储最近一次采样值
     *                  Output parameter, stores last sampled value
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedAdcState(m5pm1_adc_channel_t* channel, bool* busy, uint16_t* lastValue);
    /**
     * @brief 获取缓存的电源配置
     *        Get cached power configuration
     * @param pwrCfg 输出参数，存储 PWR_CFG 缓存值
     *               Output parameter, stores PWR_CFG cached value
     * @param holdCfg 输出参数，存储 HOLD_CFG 缓存值
     *                Output parameter, stores HOLD_CFG cached value
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedPowerConfig(uint8_t* pwrCfg, uint8_t* holdCfg);
    /**
     * @brief 获取缓存的按钮配置
     *        Get cached button configuration
     * @param cfg1 输出参数，存储 BTN_CFG_1 缓存值
     *             Output parameter, stores BTN_CFG_1 cached value
     * @param cfg2 输出参数，存储 BTN_CFG_2 缓存值
     *             Output parameter, stores BTN_CFG_2 cached value
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedButtonConfig(uint8_t* cfg1, uint8_t* cfg2);
    /**
     * @brief 获取缓存的中断屏蔽状态
     *        Get cached interrupt mask state
     * @param mask1 输出参数，存储 IRQ_MASK1 缓存值
     *              Output parameter, stores IRQ_MASK1 cached value
     * @param mask2 输出参数，存储 IRQ_MASK2 缓存值
     *              Output parameter, stores IRQ_MASK2 cached value
     * @param mask3 输出参数，存储 IRQ_MASK3 缓存值
     *              Output parameter, stores IRQ_MASK3 cached value
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedIrqMasks(uint8_t* mask1, uint8_t* mask2, uint8_t* mask3);
    /**
     * @brief 获取缓存的中断状态
     *        Get cached interrupt status
     * @param status1 输出参数，存储 IRQ_STATUS1 缓存值
     *                Output parameter, stores IRQ_STATUS1 cached value
     * @param status2 输出参数，存储 IRQ_STATUS2 缓存值
     *                Output parameter, stores IRQ_STATUS2 cached value
     * @param status3 输出参数，存储 IRQ_STATUS3 缓存值
     *                Output parameter, stores IRQ_STATUS3 cached value
     * @return 成功返回 M5PM1_OK，否则返回错误码
     *         Return M5PM1_OK on success, error code otherwise
     */
    m5pm1_err_t getCachedIrqStatus(uint8_t* status1, uint8_t* status2, uint8_t* status3);

private:
    // 设备状态
    // Device state
    uint8_t _addr;
    bool _initialized;
    bool _autoWakeEnabled;
    bool _autoSnapshot;
    uint8_t _i2cSleepTime;
    uint32_t _requestedSpeed;
    struct {
        uint8_t sleepTime;
        bool speed400k;
    } _i2cConfig;
    bool _i2cConfigValid;
    uint32_t _lastCommTime;

    // PWM 状态缓存
    // PWM state cache
    struct {
        uint16_t duty12;
        bool enabled;
        bool polarity;
    } _pwmStates[M5PM1_MAX_PWM_CHANNELS];
    uint16_t _pwmFrequency;
    bool _pwmStatesValid;

    // ADC 状态缓存
    // ADC state cache
    struct {
        uint8_t channel;
        bool busy;
        uint16_t lastValue;
    } _adcState;
    bool _adcStateValid;

    // 电源配置缓存
    // Power config cache
    uint8_t _powerCfg;
    uint8_t _holdCfg;
    bool _powerConfigValid;

    // 按钮配置缓存
    // Button config cache
    uint8_t _btnCfg1;
    uint8_t _btnCfg2;
    bool _btnConfigValid;

    // 按钮标志缓存（用于处理 BTN_FLAG 读后清除问题）
    // Button flag cache (for BTN_FLAG read-clear handling)
    bool _btnFlagCache;

    // IRQ 缓存
    // IRQ cache
    uint8_t _irqMask1;
    uint8_t _irqMask2;
    uint8_t _irqMask3;
    bool _irqMaskValid;
    uint8_t _irqStatus1;
    uint8_t _irqStatus2;
    uint8_t _irqStatus3;
    bool _irqStatusValid;

    // Neo 配置缓存
    // Neo config cache
    uint8_t _neoCfg;
    bool _neoConfigValid;

    // AW8737A 配置状态缓存
    // AW8737A configuration state cache
    bool _aw8737aConfigured;                 // 是否已调用 setAw8737aPulse 配置
                                             // Whether setAw8737aPulse has been called
    m5pm1_gpio_num_t _aw8737aPin;            // 配置的引脚号
                                             // Configured pin number
    m5pm1_aw8737a_pulse_t _aw8737aPulseNum;  // 配置的脉冲数
                                             // Configured pulse count
    uint8_t _aw8737aRegValue;                // 缓存的寄存器值（不含 REFRESH 位）
                                             // Cached register value (without REFRESH bit)
    bool _aw8737aStateValid;                 // 缓存有效性标志
                                             // Cache validity flag

    // Pin 状态缓存
    // Pin status cache
    m5pm1_pin_status_t _pinStatus[M5PM1_MAX_GPIO_PINS];  // Cached status for GPIO0-4
    bool _cacheValid;                                    // 缓存有效性标志 / Cache validity flag

#ifdef ARDUINO
    TwoWire* _wire;
    int8_t _sda;
    int8_t _scl;
#if M5PM1_HAS_M5UNIFIED_I2C
    m5::I2C_Class* _m5_i2c;
    uint32_t _commFreq;  // M5UNIFIED 路径专用：init 时从 100K 起，完成后切换至 _requestedSpeed
                         // M5UNIFIED path: starts at 100K during init, switches to _requestedSpeed after
#endif
#else
    // I2C 驱动类型选择
    // I2C driver type selection
    m5pm1_i2c_driver_t _i2cDriverType;

    // I2C 句柄
    // I2C handles
#if M5PM1_HAS_I2C_MASTER
    i2c_master_bus_handle_t _i2c_master_bus;
    i2c_master_dev_handle_t _i2c_master_dev;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
    i2c_bus_handle_t _i2c_bus;
    i2c_bus_device_handle_t _i2c_device;
#endif

    // I2C 管理标志
    // I2C management flags
    bool _busExternal;

    // 自创建总线引脚
    // Self-created bus pins
    int _sda;
    int _scl;
    i2c_port_t _port;

    // M5Unified I2C_Class 借用句柄及当前通信频率
    // Borrowed M5Unified I2C_Class handle and current communication frequency
#if M5PM1_HAS_M5UNIFIED_I2C
    m5::I2C_Class* _m5_i2c;
    uint32_t _commFreq;  // M5UNIFIED 路径专用：init 时从 100K 起，完成后切换至 _requestedSpeed
                         // M5UNIFIED path: starts at 100K during init, switches to _requestedSpeed after
#endif
#endif

    // 内部辅助函数
    // Internal helper functions
    bool _writeReg(uint8_t reg, uint8_t value);
    bool _writeReg16(uint8_t reg, uint16_t value);
    bool _readReg(uint8_t reg, uint8_t* value);
    bool _readReg16(uint8_t reg, uint16_t* value);
    bool _writeBytes(uint8_t reg, const uint8_t* data, uint8_t len);
    bool _readBytes(uint8_t reg, uint8_t* data, uint8_t len);

    bool _isValidPin(uint8_t pin);
    bool _isAdcPin(uint8_t pin);
    bool _isPwmPin(uint8_t pin);
    bool _isNeoPin(uint8_t pin);
    bool _hasActiveAdc(uint8_t pin);
    bool _hasActivePwm(uint8_t pin);
    bool _hasActiveIrq(uint8_t pin);
    bool _hasActiveWake(uint8_t pin);
    bool _hasActiveNeo(uint8_t pin);
    bool _isValidI2cFrequency(uint32_t speed);
    void _checkAutoWake();
    bool _initDevice();
    void _clearPinStates();
    void _clearPwmStates();
    void _clearAdcState();
    void _clearPowerConfig();
    void _clearButtonConfig();
    void _clearIrqMasks();
    void _clearIrqStatus();
    void _clearAll();
    bool _snapshotPinStates();
    bool _snapshotPwmStates();
    bool _snapshotAdcState();
    bool _snapshotPowerConfig();
    bool _snapshotButtonConfig();
    bool _snapshotIrqMasks();
    bool _snapshotIrqStatus();
    bool _snapshotAll();
    void _autoSnapshotUpdate(uint16_t domains);
    bool _snapshotI2cConfig();
    void _clearI2cConfig();
    bool _snapshotNeoConfig();
    void _clearNeoConfig();
    bool _snapshotAw8737a();
    void _clearAw8737a();

    // 缓存管理函数
    // Cache management functions
    void _initPinCache();
    void _updatePinCache(m5pm1_gpio_num_t pin);

    // 按钮状态内部函数
    // Button status internal function
    bool _readBtnStatus(uint8_t* rawValue);
};

#endif  // _M5PM1_H_
