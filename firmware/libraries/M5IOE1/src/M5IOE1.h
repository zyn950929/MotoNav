/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _M5IOE1_H_
#define _M5IOE1_H_

#include "M5IOE1_i2c_compat.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef ARDUINO
// Arduino：FreeRTOS 头文件通过 Arduino 框架包含
// Arduino: FreeRTOS headers included via Arduino framework
#else
// ESP-IDF 专用包含文件
// ESP-IDF specific includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#endif

// ============================
// IO 引脚定义
// IO Pin Definitions
// ============================
typedef enum {
    M5IOE1_PIN_NC = -1,
    M5IOE1_PIN_1  = 0,
    M5IOE1_PIN_2  = 1,
    M5IOE1_PIN_3  = 2,
    M5IOE1_PIN_4  = 3,
    M5IOE1_PIN_5  = 4,
    M5IOE1_PIN_6  = 5,
    M5IOE1_PIN_7  = 6,
    M5IOE1_PIN_8  = 7,
    M5IOE1_PIN_9  = 8,
    M5IOE1_PIN_10 = 9,
    M5IOE1_PIN_11 = 10,
    M5IOE1_PIN_12 = 11,
    M5IOE1_PIN_13 = 12,
    M5IOE1_PIN_14 = 13
} m5ioe1_pin_t;

// ============================
// 错误码
// Error Codes
// ============================
typedef enum {
    M5IOE1_OK = 0,                   // 成功
                                     // Success
    M5IOE1_FAIL = -1,                // 一般失败
                                     // General failure
    M5IOE1_ERR_I2C_CONFIG = -2,      // I2C 配置错误 (如频率不支持)
                                     // I2C configuration error
    M5IOE1_ERR_RULE_VIOLATION = -3,  // 条件规则错误 (如引脚冲突，互斥功能)
                                     // Condition rule violation
    M5IOE1_ERR_INVALID_ARG = -4,     // 无效参数
                                     // Invalid argument
    M5IOE1_ERR_TIMEOUT = -5,         // 超时
                                     // Timeout
    M5IOE1_ERR_NOT_SUPPORTED = -6,   // 不支持的功能
                                     // Function not supported
    M5IOE1_ERR_I2C_COMM = -7,        // I2C 通信错误
                                     // I2C communication error
    M5IOE1_ERR_NOT_INIT = -8,        // 设备未初始化
                                     // Device not initialized
    M5IOE1_ERR_INTERNAL = -9,        // 内部错误
                                     // Internal error
} m5ioe1_err_t;

// ============================
// 设备常量
// Device Constants
// ============================
#define M5IOE1_DEFAULT_ADDR     0x6F
#define M5IOE1_MAX_GPIO_PINS    14
#define M5IOE1_MAX_ADC_CHANNELS 4
#define M5IOE1_MAX_PWM_CHANNELS 4
#define M5IOE1_MAX_LED_COUNT    32
#define M5IOE1_RTC_RAM_SIZE     32

// I2C retry settings
#define M5IOE1_I2C_RETRY_COUNT    2
#define M5IOE1_I2C_RETRY_DELAY_MS 50

// ============================
// I2C 频率常量
// I2C Frequency Constants
// ============================
#define M5IOE1_I2C_FREQ_100K    100000
#define M5IOE1_I2C_FREQ_400K    400000
#define M5IOE1_I2C_FREQ_DEFAULT M5IOE1_I2C_FREQ_100K

// ============================
// 寄存器地址
// Register Addresses
// ============================
// System
#define M5IOE1_REG_UID_L 0x00  // R     [7:0] UID Low Byte
#define M5IOE1_REG_UID_H 0x01  // R     [15:8] UID High Byte
#define M5IOE1_REG_REV   0x02  // R     [7:0] SW7-SW0 Version
// GPIO
#define M5IOE1_REG_GPIO_MODE_L 0x03  // R/W   [7:0] Mode P8-P1
#define M5IOE1_REG_GPIO_MODE_H 0x04  // R/W   [7:6] Res | [5:0] Mode P14-P9
#define M5IOE1_REG_GPIO_OUT_L  0x05  // R/W   [7:0] Out P8-P1
#define M5IOE1_REG_GPIO_OUT_H  0x06  // R/W   [7:6] Res | [5:0] Out P14-P9
#define M5IOE1_REG_GPIO_IN_L   0x07  // R     [7:0] In P8-P1
#define M5IOE1_REG_GPIO_IN_H   0x08  // R     [7:6] Res | [5:0] In P14-P9
#define M5IOE1_REG_GPIO_PU_L   0x09  // R/W   [7:0] PU P8-P1
#define M5IOE1_REG_GPIO_PU_H   0x0A  // R/W   [7:6] Res | [5:0] PU P14-P9
#define M5IOE1_REG_GPIO_PD_L   0x0B  // R/W   [7:0] PD P8-P1
#define M5IOE1_REG_GPIO_PD_H   0x0C  // R/W   [7:6] Res | [5:0] PD P14-P9
#define M5IOE1_REG_GPIO_IE_L   0x0D  // R/W   [7:0] IE P8-P1
#define M5IOE1_REG_GPIO_IE_H   0x0E  // R/W   [7:6] Res | [5:0] IE P14-P9
#define M5IOE1_REG_GPIO_IP_L   0x0F  // R/W   [7:0] IP P8-P1
#define M5IOE1_REG_GPIO_IP_H   0x10  // R/W   [7:6] Res | [5:0] IP P14-P9
#define M5IOE1_REG_GPIO_IS_L   0x11  // R     [7:0] IS P8-P1
#define M5IOE1_REG_GPIO_IS_H   0x12  // R     [7:6] Res | [5:0] IS P14-P9
#define M5IOE1_REG_GPIO_DRV_L  0x13  // R/W   [7:0] Drive P8-P1
#define M5IOE1_REG_GPIO_DRV_H  0x14  // R/W   [7:6] Res | [5:0] Drive P14-P9
// ADC
#define M5IOE1_REG_ADC_CTRL   0x15  // R/W   [7] BUSY | [6] START | [2:0] Channel
#define M5IOE1_REG_ADC_DATA_L 0x16  // R     [7:0] ADC Data Low
#define M5IOE1_REG_ADC_DATA_H 0x17  // R     [3:0] ADC Data High
// Temperature
#define M5IOE1_REG_TEMP_CTRL   0x18  // R/W   [7] TBUSY | [6] TSTART
#define M5IOE1_REG_TEMP_DATA_L 0x19  // R     [7:0] Temp Data Low
#define M5IOE1_REG_TEMP_DATA_H 0x1A  // R     [3:0] Temp Data High
// PWM
#define M5IOE1_REG_PWM1_DUTY_L 0x1B  // R/W   [7:0] PWM1 Duty Low
#define M5IOE1_REG_PWM1_DUTY_H 0x1C  // R/W   [7] EN | [6] POL | [3:0] PWM1 Duty High
#define M5IOE1_REG_PWM2_DUTY_L 0x1D  // R/W   [7:0] PWM2 Duty Low
#define M5IOE1_REG_PWM2_DUTY_H 0x1E  // R/W   [7] EN | [6] POL | [3:0] PWM2 Duty High
#define M5IOE1_REG_PWM3_DUTY_L 0x1F  // R/W   [7:0] PWM3 Duty Low
#define M5IOE1_REG_PWM3_DUTY_H 0x20  // R/W   [7] EN | [6] POL | [3:0] PWM3 Duty High
#define M5IOE1_REG_PWM4_DUTY_L 0x21  // R/W   [7:0] PWM4 Duty Low
#define M5IOE1_REG_PWM4_DUTY_H 0x22  // R/W   [7] EN | [6] POL | [3:0] PWM4 Duty High
// System Config
#define M5IOE1_REG_I2C_CFG       0x23  // R/W   [6] INT_PU/PD | [5] WAKE | [4] SPD | [3:0] SLEEP
#define M5IOE1_REG_LED_CFG       0x24  // R/W   [6] REFRESH | [5:0] LED Num
#define M5IOE1_REG_PWM_FREQ_L    0x25  // R/W   [7:0] PWM Freq Low
#define M5IOE1_REG_PWM_FREQ_H    0x26  // R/W   [7:0] PWM Freq High
#define M5IOE1_REG_REF_VOLTAGE_L 0x27  // R     [7:0] Ref Voltage Low
#define M5IOE1_REG_REF_VOLTAGE_H 0x28  // R     [15:8] Ref Voltage High
#define M5IOE1_REG_FACTORY_RESET 0x29  // W     [7:0] Reset Key
// Data areas
#define M5IOE1_REG_LED_RAM_START 0x30  // R/W   NeoPixel RGB565 Data (32 LEDs x 2B)
#define M5IOE1_REG_LED_RAM_END   0x6F
#define M5IOE1_REG_RTC_RAM_START 0x70  // R/W   RTC Retention RAM (32B)
#define M5IOE1_REG_RTC_RAM_END   0x8F
// Extended
#define M5IOE1_REG_AW8737A_PULSE 0x90  // R/W   [7] REFRESH | [6:5] NUM | [4:0] GPIO

// ============================
// 位定义
// Bit Definitions
// ============================
// ADC Control
#define M5IOE1_ADC_CH_MASK 0x07
#define M5IOE1_ADC_START   (1 << 6)
#define M5IOE1_ADC_BUSY    (1 << 7)
// Temperature Control
#define M5IOE1_TEMP_START (1 << 6)
#define M5IOE1_TEMP_BUSY  (1 << 7)
// PWM Control
#define M5IOE1_PWM_POLARITY (1 << 6)
#define M5IOE1_PWM_ENABLE   (1 << 7)
// I2C Config
#define M5IOE1_I2C_SLEEP_MASK     0x0F
#define M5IOE1_I2C_SPEED_400K_BIT (1 << 4)
#define M5IOE1_I2C_WAKE_RISING    (1 << 5)
#define M5IOE1_I2C_PULL_OFF       (1 << 6)
// LED Config
#define M5IOE1_LED_NUM_MASK 0x3F
#define M5IOE1_LED_REFRESH  (1 << 6)
// Factory Reset
#define M5IOE1_FACTORY_RESET_KEY 0x3A
// AW8737A Pulse
#define M5IOE1_AW8737A_GPIO_MASK 0x1F
#define M5IOE1_AW8737A_NUM_SHIFT 5
#define M5IOE1_AW8737A_NUM_MASK  0x03
#define M5IOE1_AW8737A_REFRESH   (1 << 7)

// ============================
// ADC 通道定义（支持 ADC 的 IO 引脚）
// ADC Channel Definitions (IO pins that support ADC)
// ============================
#define M5IOE1_ADC_CH1 1  // IO2 (pin index 1)
#define M5IOE1_ADC_CH2 2  // IO4 (pin index 3)
#define M5IOE1_ADC_CH3 3  // IO5 (pin index 4)
#define M5IOE1_ADC_CH4 4  // IO7 (pin index 6)

// ============================
// PWM 通道定义（支持 PWM 的 IO 引脚）
// PWM Channel Definitions (IO pins that support PWM)
// ============================
#define M5IOE1_PWM_CH1 0  // IO9 (pin index 8)
#define M5IOE1_PWM_CH2 1  // IO8 (pin index 7)
#define M5IOE1_PWM_CH3 2  // IO11 (pin index 10)
#define M5IOE1_PWM_CH4 3  // IO10 (pin index 9)

// ============================
// GPIO 电平定义
// GPIO Level Definitions
// ============================
#ifndef LOW
#define LOW 0x0
#endif
#ifndef HIGH
#define HIGH 0x1
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

// ============================
// 中断模式定义
// Interrupt Mode Definitions
// ============================
#ifndef DISABLED
#define DISABLED 0x00
#endif
#ifndef RISING
#define RISING 0x01
#endif
#ifndef FALLING
#define FALLING 0x02
#endif
#ifndef CHANGE
#define CHANGE 0x03
#endif
#ifndef ONLOW
#define ONLOW 0x04
#endif
#ifndef ONHIGH
#define ONHIGH 0x05
#endif
#ifndef ONLOW_WE
#define ONLOW_WE 0x0C
#endif
#ifndef ONHIGH_WE
#define ONHIGH_WE 0x0D
#endif

// ============================
// 上拉/下拉模式定义
// Pull Mode Definitions
// ============================
#define M5IOE1_PULL_NONE 0x00
#define M5IOE1_PULL_UP   0x01
#define M5IOE1_PULL_DOWN 0x02

// ============================
// 驱动模式定义
// Drive Mode Definitions
// ============================
#define M5IOE1_DRIVE_PUSHPULL  0x00
#define M5IOE1_DRIVE_OPENDRAIN 0x01

// ============================
// AW8737A 脉冲刷新类型
// AW8737A PULSE Refresh Types
// ============================
typedef enum {
    M5IOE1_AW8737A_REFRESH_WAIT = 0,  // 不刷新，等待下一次触发
                                      // No refresh, wait for next trigger
    M5IOE1_AW8737A_REFRESH_NOW = 1    // 刷新并立即执行
                                      // Refresh and execute immediately
} m5ioe1_aw8737a_refresh_t;

// ============================
// AW8737A 脉冲数量类型
// AW8737A PULSE NUM Types
// ============================
typedef enum {
    M5IOE1_AW8737A_PULSE_0 = 0,  // 0 个脉冲
                                 // 0 pulse
    M5IOE1_AW8737A_PULSE_1 = 1,  // 1 个脉冲
                                 // 1 pulse
    M5IOE1_AW8737A_PULSE_2 = 2,  // 2 个脉冲
                                 // 2 pulses
    M5IOE1_AW8737A_PULSE_3 = 3   // 3 个脉冲
                                 // 3 pulses
} m5ioe1_aw8737a_pulse_t;

// ============================
// AW8737A 模式类型
// AW8737A Mode Types
// ============================
typedef enum {
    M5IOE1_AW8737A_MODE_1 = 0,  // 模式1: 0脉冲 (关闭/静音)
                                // Mode 1: 0 pulse (off/mute)
    M5IOE1_AW8737A_MODE_2 = 1,  // 模式2: 1脉冲 (低增益)
                                // Mode 2: 1 pulse (low gain)
    M5IOE1_AW8737A_MODE_3 = 2,  // 模式3: 2脉冲 (中增益)
                                // Mode 3: 2 pulses (medium gain)
    M5IOE1_AW8737A_MODE_4 = 3   // 模式4: 3脉冲 (高增益)
                                // Mode 4: 3 pulses (high gain)
} m5ioe1_aw8737a_mode_t;

// ============================
// 中断处理模式
// Interrupt Handling Mode
// ============================
typedef enum {
    M5IOE1_INT_MODE_DISABLED = 0,  // 中断处理已禁用
                                   // Interrupt handling disabled
    M5IOE1_INT_MODE_POLLING,       // 轮询模式（默认）
                                   // Polling mode (default)
    M5IOE1_INT_MODE_HARDWARE       // 硬件中断模式
                                   // Hardware interrupt mode
} m5ioe1_int_mode_t;

// ============================
// I2C 速度定义
// I2C Speed Definitions
// ============================
typedef enum {
    M5IOE1_I2C_SPEED_100K = 0,  // 100KHz 标准模式
                                // 100KHz standard mode
    M5IOE1_I2C_SPEED_400K = 1   // 400KHz 快速模式
                                // 400KHz fast mode
} m5ioe1_i2c_speed_t;

// ============================
// I2C 唤醒边沿定义
// I2C Wake Edge Definitions
// ============================
typedef enum {
    M5IOE1_WAKE_EDGE_FALLING = 0,  // 下降沿唤醒（默认）
                                   // Falling edge wake (default)
    M5IOE1_WAKE_EDGE_RISING = 1    // 上升沿唤醒
                                   // Rising edge wake
} m5ioe1_wake_edge_t;

// ============================
// I2C 内部上拉定义
// I2C Internal Pull-up Definitions
// ============================
typedef enum {
    M5IOE1_PULL_ENABLED = 0,  // 内部上拉启用（默认）
                              // Internal pull-up enabled (default)
    M5IOE1_PULL_DISABLED = 1  // 内部上拉禁用
                              // Internal pull-up disabled
} m5ioe1_pull_config_t;

// ============================
// 快照域定义（位掩码）
// Snapshot Domain Definitions (bitmask)
// ============================
typedef enum {
    M5IOE1_SNAPSHOT_DOMAIN_GPIO = 1 << 0,     // GPIO 引脚状态
                                              // GPIO pin states
    M5IOE1_SNAPSHOT_DOMAIN_PWM = 1 << 1,      // PWM 配置
                                              // PWM configuration
    M5IOE1_SNAPSHOT_DOMAIN_ADC = 1 << 2,      // ADC 状态
                                              // ADC state
    M5IOE1_SNAPSHOT_DOMAIN_AW8737A = 1 << 3,  // AW8737A 音频放大器
                                              // AW8737A audio amplifier
    M5IOE1_SNAPSHOT_DOMAIN_ALL = 0x0F         // 所有域
                                              // All domains
} m5ioe1_snapshot_domain_t;

// ============================
// 日志级别定义
// Log Level Definitions
// ============================
typedef enum {
    M5IOE1_LOG_LEVEL_NONE = 0,  // 无日志输出
                                // No log output
    M5IOE1_LOG_LEVEL_ERROR,     // 仅错误消息
                                // Error messages only
    M5IOE1_LOG_LEVEL_WARN,      // 警告和错误消息
                                // Warning and error messages
    M5IOE1_LOG_LEVEL_INFO,      // 信息、警告和错误消息（默认）
                                // Info, warning and error messages (default)
    M5IOE1_LOG_LEVEL_DEBUG,     // 调试、信息、警告和错误消息
                                // Debug, info, warning and error messages
    M5IOE1_LOG_LEVEL_VERBOSE    // 所有消息包括详细输出
                                // All messages including verbose
} m5ioe1_log_level_t;

// ============================
// 用于验证的配置类型
// Configuration Type for Validation
// ============================
typedef enum {
    M5IOE1_CONFIG_GPIO_INPUT = 0,  // GPIO 输入模式
                                   // GPIO input mode
    M5IOE1_CONFIG_GPIO_OUTPUT,     // GPIO 输出模式
                                   // GPIO output mode
    M5IOE1_CONFIG_GPIO_INTERRUPT,  // GPIO 中断模式
                                   // GPIO interrupt mode
    M5IOE1_CONFIG_ADC,             // ADC 功能
                                   // ADC function
    M5IOE1_CONFIG_PWM,             // PWM 功能
                                   // PWM function
    M5IOE1_CONFIG_NEOPIXEL,        // NeoPixel LED 功能（仅 IO14）
                                   // NeoPixel LED function (IO14 only)
    M5IOE1_CONFIG_I2C_SLEEP        // I2C 睡眠模式配置
                                   // I2C sleep mode configuration
} m5ioe1_config_type_t;

// ============================
// RGB 颜色结构
// RGB Color Structure
// ============================
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} m5ioe1_rgb_t;

// ============================
// 配置验证结果
// Configuration Validation Result
// ============================
typedef struct {
    bool valid;
    char error_msg[64];
    uint8_t conflicting_pin;
} m5ioe1_validation_t;

// ============================
// 快照验证结果
// Snapshot Verification Result
// ============================
typedef struct {
    bool consistent;           // 如果所有缓存值与硬件寄存器匹配则为 true
                               // true if all cached values match hardware registers
    bool gpio_mismatch;        // 如果 GPIO 寄存器与缓存不匹配则为 true
                               // true if GPIO registers don't match cache
    bool pwm_mismatch;         // 如果 PWM 寄存器与缓存不匹配则为 true
                               // true if PWM registers don't match cache
    bool adc_mismatch;         // 如果 ADC 寄存器与缓存不匹配则为 true
                               // true if ADC registers don't match cache
    bool aw8737a_mismatch;     // 如果 AW8737A 寄存器与缓存不匹配则为 true
                               // true if AW8737A registers don't match cache
    uint16_t expected_mode;    // 缓存的 GPIO 模式寄存器值
                               // cached GPIO mode register value
    uint16_t actual_mode;      // 实际的 GPIO 模式寄存器值
                               // actual GPIO mode register value
    uint16_t expected_output;  // 缓存的 GPIO 输出寄存器值
                               // cached GPIO output register value
    uint16_t actual_output;    // 实际的 GPIO 输出寄存器值
                               // actual GPIO output register value
    uint8_t expected_aw8737a;  // 缓存的 AW8737A 寄存器值
                               // cached AW8737A register value
    uint8_t actual_aw8737a;    // 实际的 AW8737A 寄存器值
                               // actual AW8737A register value
} m5ioe1_snapshot_verify_t;

// ============================
// 回调类型
// Callback Types
// ============================
typedef void (*m5ioe1_callback_t)(void);
typedef void (*m5ioe1_callback_arg_t)(void*);

// ============================
// M5IOE1 类
// M5IOE1 Class
// ============================
class M5IOE1 {
public:
    /**
     * @brief 构造 M5IOE1 对象
     *        Construct M5IOE1 object
     */
    M5IOE1();
    /**
     * @brief 析构 M5IOE1 对象并释放资源
     *        Destroy M5IOE1 object and release resources
     */
    ~M5IOE1();

    // ========================
    // 初始化
    // Initialization
    // ========================
#ifdef ARDUINO
    /**
     * @brief Initialize the M5IOE1 device (Arduino)
     * @note Without intPin (or intPin=-1): Only POLLING and DISABLED modes are supported
     * @note With intPin >= 0: Supports HARDWARE, POLLING, and DISABLED modes
     * @note HARDWARE mode: Configures intPin as input with internal pull-up. Uses FALLING edge, then reads GPIO_IS
     * registers (0x11-0x12)
     * @note POLLING mode: Periodically reads GPIO_IS registers (0x11-0x12). Non-zero indicates one or more pins
     * triggered
     * @note DISABLED mode: No interrupt handling
     * @param wire Pointer to TwoWire instance
     * @param addr I2C address (default 0x6F)
     * @param sda SDA pin (default -1, uses default I2C pins)
     * @param scl SCL pin (default -1, uses default I2C pins)
     * @param speed I2C speed in Hz (default 100000)
     * @param intPin Hardware interrupt pin (-1 = no hardware interrupt, default -1)
     * @param intMode Interrupt mode (default M5IOE1_INT_MODE_POLLING)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(TwoWire* wire, uint8_t addr = M5IOE1_DEFAULT_ADDR, uint8_t sda = -1, uint8_t scl = -1,
                       uint32_t speed = 100000, int8_t intPin = -1,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING);
#if M5IOE1_HAS_M5UNIFIED_I2C
    /**
     * @brief Initialize with M5Unified I2C_Class instance, no hardware interrupt (Arduino)
     * @note  The I2C bus must already be initialized (i2c->begin() called).
     *        M5IOE1 borrows the I2C_Class; caller retains ownership and lifecycle.
     *        不负责驱动安装或释放，I2C 由调用方全程管理。
     * @param i2c     已 begin() 的 m5::I2C_Class 指针 / Already begin()-ed m5::I2C_Class
     * @param addr    I2C 地址 / I2C address (default 0x6F)
     * @param speed   I2C 速率（Hz）/ I2C speed in Hz (default 100000)
     * @param intMode 中断模式 / Interrupt mode (default POLLING)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(m5::I2C_Class* i2c, uint8_t addr = M5IOE1_DEFAULT_ADDR, uint32_t speed = M5IOE1_I2C_FREQ_100K,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING);

    /**
     * @brief Initialize with M5Unified I2C_Class instance, with hardware interrupt (Arduino)
     * @param i2c     已 begin() 的 m5::I2C_Class 指针 / Already begin()-ed m5::I2C_Class
     * @param addr    I2C 地址 / I2C address
     * @param speed   I2C 速率（Hz）/ I2C speed in Hz
     * @param intPin  硬件中断引脚 / Hardware interrupt pin
     * @param intMode 中断模式 / Interrupt mode (default HARDWARE)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed, int intPin,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_HARDWARE);
#endif  // M5IOE1_HAS_M5UNIFIED_I2C
#else   // ESP-IDF
    // =====================================================
    // Type 1A: Self-created I2C bus, no hardware interrupt
    // =====================================================
    /**
     * @brief Initialize with self-created I2C bus, no hardware interrupt (ESP-IDF)
     * @param port I2C port number (I2C_NUM_0 or I2C_NUM_1)
     * @param addr I2C address (default 0x6F)
     * @param sda SDA pin (default 21)
     * @param scl SCL pin (default 22)
     * @param speed I2C speed in Hz (only 100000 or 400000 supported)
     * @param intMode Interrupt mode: M5IOE1_INT_MODE_POLLING or M5IOE1_INT_MODE_DISABLED
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(i2c_port_t port = I2C_NUM_0, uint8_t addr = M5IOE1_DEFAULT_ADDR, int sda = 21, int scl = 22,
                       uint32_t speed = M5IOE1_I2C_FREQ_100K, m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING);

    // =====================================================
    // Type 1B: Self-created I2C bus, with hardware interrupt
    // =====================================================
    /**
     * @brief Initialize with self-created I2C bus, with hardware interrupt (ESP-IDF)
     * @param port I2C port number
     * @param addr I2C address
     * @param sda SDA pin
     * @param scl SCL pin
     * @param speed I2C speed in Hz
     * @param intPin Hardware interrupt GPIO pin
     * @param intMode Interrupt mode: M5IOE1_INT_MODE_HARDWARE, POLLING, or DISABLED
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(i2c_port_t port, uint8_t addr, int sda, int scl, uint32_t speed, int intPin,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_HARDWARE);

    // =====================================================
    // Type 2A/2B: Existing i2c_master_bus_handle_t (ESP-IDF >= 5.3.0 only)
    // =====================================================
#if M5IOE1_HAS_I2C_MASTER
    /**
     * @brief Initialize with existing i2c_master_bus handle (ESP-IDF native driver, IDF >= 5.3.0)
     * @param bus Existing i2c_master_bus_handle_t
     * @param addr I2C address (default 0x6F)
     * @param speed I2C speed in Hz (for device handle creation)
     * @param intMode Interrupt mode: M5IOE1_INT_MODE_POLLING or M5IOE1_INT_MODE_DISABLED
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(i2c_master_bus_handle_t bus, uint8_t addr = M5IOE1_DEFAULT_ADDR,
                       uint32_t speed = M5IOE1_I2C_FREQ_100K, m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING);

    /**
     * @brief Initialize with existing i2c_master_bus handle, with hardware interrupt (IDF >= 5.3.0)
     * @param bus Existing i2c_master_bus_handle_t
     * @param addr I2C address
     * @param speed I2C speed in Hz
     * @param intPin Hardware interrupt GPIO pin
     * @param intMode Interrupt mode
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t speed, int intPin,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_HARDWARE);
#endif  // M5IOE1_HAS_I2C_MASTER

    // =====================================================
    // Type 3A: Existing i2c_bus_handle_t, no hardware interrupt
    // =====================================================
#if M5IOE1_HAS_I2C_BUS
    /**
     * @brief Initialize with existing i2c_bus handle (esp-idf-lib component)
     * @param bus Existing i2c_bus_handle_t
     * @param addr I2C address (default 0x6F)
     * @param speed I2C speed in Hz (for device handle creation)
     * @param intMode Interrupt mode: M5IOE1_INT_MODE_POLLING or M5IOE1_INT_MODE_DISABLED
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(i2c_bus_handle_t bus, uint8_t addr = M5IOE1_DEFAULT_ADDR, uint32_t speed = M5IOE1_I2C_FREQ_100K,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING);

    // =====================================================
    // Type 3B: Existing i2c_bus_handle_t, with hardware interrupt
    // =====================================================
    /**
     * @brief Initialize with existing i2c_bus handle, with hardware interrupt
     * @param bus Existing i2c_bus_handle_t
     * @param addr I2C address
     * @param speed I2C speed in Hz
     * @param intPin Hardware interrupt GPIO pin
     * @param intMode Interrupt mode
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(i2c_bus_handle_t bus, uint8_t addr, uint32_t speed, int intPin,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_HARDWARE);
#else
    /**
     * @brief i2c_bus overload is intentionally kept for diagnostics when unavailable
     * @note This overload exists only to provide a clear compile-time message when called.
     */
    inline m5ioe1_err_t begin(i2c_bus_handle_t bus, uint8_t addr = M5IOE1_DEFAULT_ADDR,
                              uint32_t speed            = M5IOE1_I2C_FREQ_100K,
                              m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING)
    {
        (void)bus;
        (void)addr;
        (void)speed;
        (void)intMode;
        return M5IOE1_ERR_NOT_SUPPORTED;
    }
    inline m5ioe1_err_t begin(i2c_bus_handle_t bus, uint8_t addr, uint32_t speed, int intPin,
                              m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_HARDWARE)
    {
        (void)bus;
        (void)addr;
        (void)speed;
        (void)intPin;
        (void)intMode;
        return M5IOE1_ERR_NOT_SUPPORTED;
    }
#endif  // M5IOE1_HAS_I2C_BUS

#if M5IOE1_HAS_M5UNIFIED_I2C
    // =====================================================
    // Type 4A: M5Unified I2C_Class, no hardware interrupt
    // =====================================================
    /**
     * @brief Initialize with M5Unified I2C_Class instance, no hardware interrupt (ESP-IDF)
     * @note  The I2C bus must already be initialized (i2c->begin() called).
     *        M5IOE1 borrows the I2C_Class; caller retains ownership and lifecycle.
     *        不负责驱动安装或释放，I2C 由调用方全程管理。
     * @param i2c     已 begin() 的 m5::I2C_Class 指针 / Already begin()-ed m5::I2C_Class
     * @param addr    I2C 地址 / I2C address (default 0x6F)
     * @param speed   I2C 速率（Hz）/ I2C speed in Hz (default 100000)
     * @param intMode 中断模式 / Interrupt mode (POLLING or DISABLED)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(m5::I2C_Class* i2c, uint8_t addr = M5IOE1_DEFAULT_ADDR, uint32_t speed = M5IOE1_I2C_FREQ_100K,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_POLLING);

    // =====================================================
    // Type 4B: M5Unified I2C_Class, with hardware interrupt
    // =====================================================
    /**
     * @brief Initialize with M5Unified I2C_Class instance, with hardware interrupt (ESP-IDF)
     * @param i2c     已 begin() 的 m5::I2C_Class 指针 / Already begin()-ed m5::I2C_Class
     * @param addr    I2C 地址 / I2C address
     * @param speed   I2C 速率（Hz）/ I2C speed in Hz
     * @param intPin  硬件中断 GPIO引脚 / Hardware interrupt GPIO pin
     * @param intMode 中断模式 / Interrupt mode
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed, int intPin,
                       m5ioe1_int_mode_t intMode = M5IOE1_INT_MODE_HARDWARE);
#endif  // M5IOE1_HAS_M5UNIFIED_I2C

#endif  // !ARDUINO

    /**
     * @brief Set interrupt handling mode
     * @param intMode Interrupt mode (DISABLED, POLLING, HARDWARE)
     * @param pollingIntervalMs Polling interval in ms (for polling mode, default 5000ms)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setInterruptMode(m5ioe1_int_mode_t intMode, uint32_t pollingIntervalMs = 5000);

    /**
     * @brief Set polling interval for POLLING mode
     * @param seconds Polling interval in seconds (range: 0.001 to 3600, supports float)
     * @return M5IOE1_OK if successful, M5IOE1_ERR_INVALID_ARG if out of range
     * @note If already in POLLING mode, the task will be restarted with new interval
     */
    m5ioe1_err_t setPollingInterval(float seconds);

    /**
     * @brief Set global log level for M5IOE1 library
     * @param level Log level (ERROR, WARN, INFO, DEBUG, VERBOSE)
     * @note For ESP-IDF: Uses esp_log_level_set() to control ESP_LOGx macros
     * @note For Arduino: Controls Serial.printf output filtering
     * @note Default level is M5IOE1_LOG_LEVEL_INFO
     */
    static void setLogLevel(m5ioe1_log_level_t level);

    /**
     * @brief Get current global log level
     * @return Current log level
     */
    static m5ioe1_log_level_t getLogLevel();

    // ========================
    // 设备信息
    // Device Information
    // ========================
    /**
     * @brief 读取设备 UID
     *        Read device UID
     * @param uid 输出：UID（16-bit）
     *            Output: 16-bit UID
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getUID(uint16_t* uid);
    /**
     * @brief 读取设备版本号
     *        Read device version
     * @param version 输出：版本号
     *                Output: version value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getVersion(uint8_t* version);
    /**
     * @brief 读取参考电压（mV）
     *        Read reference voltage (mV)
     * @param voltage_mv 输出：参考电压（mV）
     *                   Output: reference voltage (mV)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getRefVoltage(uint16_t* voltage_mv);

    // ========================
    // GPIO 功能（Arduino 风格）
    // GPIO Functions (Arduino-style)
    // ========================
    /**
     * @brief 设置 GPIO 模式（无返回值）
     *        Set GPIO mode (no return value)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param mode 模式：INPUT/OUTPUT/INPUT_PULLUP/INPUT_PULLDOWN/OUTPUT_OPEN_DRAIN 等
     *             Mode: INPUT/OUTPUT/INPUT_PULLUP/INPUT_PULLDOWN/OUTPUT_OPEN_DRAIN, etc.
     * @note 如需错误码请使用 pinModeWithRes
     *       Use pinModeWithRes for error codes
     */
    void pinMode(uint8_t pin, uint8_t mode);
    /**
     * @brief 写入数字电平（无返回值）
     *        Write digital level (no return value)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param value 电平值：LOW 或 HIGH
     *              Level: LOW or HIGH
     * @note 如需错误码请使用 digitalWriteWithRes
     *       Use digitalWriteWithRes for error codes
     */
    void digitalWrite(uint8_t pin, uint8_t value);
    /**
     * @brief 读取数字电平（错误时返回 -1）
     *        Read digital level (returns -1 on error)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @return 成功返回 0/1，失败返回 -1
     *         Returns 0/1 on success, -1 on failure
     */
    int digitalRead(uint8_t pin);

    // ========================
    // GPIO 功能（Arduino 风格 - 带错误码）
    // GPIO Functions (Arduino-style - with error code)
    // ========================
    /**
     * @brief 设置 GPIO 模式并返回错误码
     *        Set GPIO mode and return error code
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param mode 模式：INPUT/OUTPUT/INPUT_PULLUP/INPUT_PULLDOWN/OUTPUT_OPEN_DRAIN 等
     *             Mode: INPUT/OUTPUT/INPUT_PULLUP/INPUT_PULLDOWN/OUTPUT_OPEN_DRAIN, etc.
     * @param err 输出：错误码指针（不能为空）
     *            Output: error code pointer (must not be NULL)
     * @note err 为空时仅记录日志并返回
     *       If err is NULL, the function only logs and returns
     */
    void pinModeWithRes(uint8_t pin, uint8_t mode, m5ioe1_err_t* err);
    /**
     * @brief 写入数字电平并返回错误码
     *        Write digital level and return error code
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param value 电平值：LOW 或 HIGH
     *              Level: LOW or HIGH
     * @param err 输出：错误码指针（不能为空）
     *            Output: error code pointer (must not be NULL)
     * @note err 为空时仅记录日志并返回
     *       If err is NULL, the function only logs and returns
     */
    void digitalWriteWithRes(uint8_t pin, uint8_t value, m5ioe1_err_t* err);
    /**
     * @brief 读取数字电平并返回错误码
     *        Read digital level and return error code
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param err 输出：错误码指针（不能为空）
     *            Output: error code pointer (must not be NULL)
     * @return 成功返回 0/1，失败返回 -1
     *         Returns 0/1 on success, -1 on failure
     * @note err 为空时仅记录日志并返回 -1
     *       If err is NULL, the function only logs and returns -1
     */
    int digitalReadWithRes(uint8_t pin, m5ioe1_err_t* err);

    // ========================
    // 中断功能
    // Interrupt Functions
    // ========================
    /**
     * @brief 绑定 GPIO 中断回调（无参数）
     *        Attach GPIO interrupt callback (no argument)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param callback 回调函数
     *                 Callback function
     * @param mode 中断模式：RISING 或 FALLING（非 RISING 值均视为 FALLING）
     *             Interrupt mode: RISING or FALLING (non-RISING values treated as FALLING)
     * @note 需要先调用 begin 完成初始化
     *       begin must be called before use
     */
    void attachInterrupt(uint8_t pin, m5ioe1_callback_t callback, uint8_t mode);
    /**
     * @brief 绑定 GPIO 中断回调（带参数）
     *        Attach GPIO interrupt callback (with argument)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param callback 回调函数（带参数）
     *                 Callback function with argument
     * @param arg 回调参数
     *            Callback argument
     * @param mode 中断模式：RISING 或 FALLING（非 RISING 值均视为 FALLING）
     *             Interrupt mode: RISING or FALLING (non-RISING values treated as FALLING)
     * @note 需要先调用 begin 完成初始化
     *       begin must be called before use
     */
    void attachInterruptArg(uint8_t pin, m5ioe1_callback_arg_t callback, void* arg, uint8_t mode);
    /**
     * @brief 解绑并禁用 GPIO 中断
     *        Detach and disable GPIO interrupt
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     */
    void detachInterrupt(uint8_t pin);
    /**
     * @brief 启用回调触发（软件开关）
     *        Enable callback trigger (software switch)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @note 不修改硬件中断寄存器
     *       Does not change hardware interrupt registers
     */
    void enableInterrupt(uint8_t pin);
    /**
     * @brief 禁用回调触发（软件开关）
     *        Disable callback trigger (software switch)
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @note 不修改硬件中断寄存器
     *       Does not change hardware interrupt registers
     */
    void disableInterrupt(uint8_t pin);
    /**
     * @brief 获取中断状态位（GPIO_IS）
     *        Get interrupt status bits (GPIO_IS)
     * @param status 输出：中断状态位掩码
     *               Output: interrupt status bitmask
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getInterruptStatus(uint16_t* status);
    /**
     * @brief 清除所有中断状态
     *        Clear all interrupt status
     * @return M5IOE1_OK if successful, error code otherwise
     * @note GPIO_IS 寄存器为"写 0 清除"语义，会清除所有引脚的中断状态
     *       GPIO_IS register uses "write 0 to clear" semantics, clears all pins' interrupt status
     */
    m5ioe1_err_t clearInterrupt();

    /**
     * @brief 清除指定引脚的中断状态
     *        Clear interrupt status for a specific pin
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @return M5IOE1_OK if successful, error code otherwise
     * @note GPIO_IS 寄存器为"写 0 清除"语义，仅清除指定引脚的中断位
     *       GPIO_IS register uses "write 0 to clear" semantics, clears only the specified pin's interrupt bit
     */
    m5ioe1_err_t clearInterrupt(uint8_t pin);

    // ========================
    // 高级 GPIO 功能
    // Advanced GPIO Functions
    // ========================
    /**
     * @brief 设置上拉/下拉模式
     *        Set pull-up/pull-down mode
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param pullMode 上拉/下拉模式：M5IOE1_PULL_NONE/UP/DOWN
     *                 Pull mode: M5IOE1_PULL_NONE/UP/DOWN
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setPullMode(uint8_t pin, uint8_t pullMode);
    /**
     * @brief 设置输出驱动模式
     *        Set output drive mode
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param driveMode 驱动模式：M5IOE1_DRIVE_PUSHPULL 或 M5IOE1_DRIVE_OPENDRAIN
     *                  Drive mode: M5IOE1_DRIVE_PUSHPULL or M5IOE1_DRIVE_OPENDRAIN
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setDriveMode(uint8_t pin, uint8_t driveMode);
    /**
     * @brief 读取 GPIO 输入电平
     *        Read GPIO input level
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param state 输出：电平状态（0/1）
     *              Output: level state (0/1)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getInputState(uint8_t pin, uint8_t* state);

    // ========================
    // ADC 功能
    // ADC Functions
    // ========================
    /**
     * @brief Read ADC value
     * @param channel ADC channel (1-4)
     * @param result Pointer to store 12-bit result
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t analogRead(uint8_t channel, uint16_t* result);
    /**
     * @brief 查询 ADC 是否忙
     *        Check if ADC is busy
     * @param busy 输出：busy 状态
     *             Output: busy status
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t isAdcBusy(bool* busy);
    /**
     * @brief 禁用 ADC 并清零控制寄存器
     *        Disable ADC and clear control register
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t disableAdc();

    // ========================
    // 温度传感器
    // Temperature Sensor
    // ========================
    /**
     * @brief 读取温度传感器原始值
     *        Read temperature sensor raw value
     * @param temperature 输出：温度原始值（12-bit）
     *                    Output: raw temperature value (12-bit)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t readTemperature(uint16_t* temperature);
    /**
     * @brief 查询温度转换是否忙
     *        Check if temperature conversion is busy
     * @param busy 输出：busy 状态
     *             Output: busy status
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t isTemperatureBusy(bool* busy);

    // ========================
    // PWM 功能
    // PWM Functions
    // ========================
    /**
     * @brief Set PWM frequency (shared by all channels)
     * @param frequency Frequency in Hz
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setPwmFrequency(uint16_t frequency);
    /**
     * @brief 读取 PWM 频率
     *        Read PWM frequency
     * @param frequency 输出：频率（Hz）
     *                  Output: frequency in Hz
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getPwmFrequency(uint16_t* frequency);

    /**
     * @brief Set PWM duty cycle (percentage)
     * @param channel PWM channel (0-3)
     * @param duty Duty cycle percentage (0-100)
     * @param polarity PWM polarity (false=normal, true=inverted)
     * @param enable Enable PWM output
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setPwmDuty(uint8_t channel, uint8_t duty, bool polarity = false, bool enable = true);
    /**
     * @brief 读取 PWM 占空比（百分比）
     *        Read PWM duty cycle (percentage)
     * @param channel PWM 通道（0-3）
     *                PWM channel (0-3)
     * @param duty 输出：占空比百分比（0-100）
     *             Output: duty percentage (0-100)
     * @param polarity 输出：极性状态
     *                 Output: polarity state
     * @param enable 输出：使能状态
     *               Output: enable state
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getPwmDuty(uint8_t channel, uint8_t* duty, bool* polarity, bool* enable);

    /**
     * @brief Set PWM duty cycle (12-bit value)
     * @param channel PWM channel (0-3)
     * @param duty12 12-bit duty value (0-4095)
     * @param polarity PWM polarity
     * @param enable Enable PWM output
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setPwmDuty12bit(uint8_t channel, uint16_t duty12, bool polarity = false, bool enable = true);
    /**
     * @brief 读取 PWM 占空比（12-bit）
     *        Read PWM duty cycle (12-bit)
     * @param channel PWM 通道（0-3）
     *                PWM channel (0-3)
     * @param duty12 输出：12-bit 占空比（0-4095）
     *               Output: 12-bit duty (0-4095)
     * @param polarity 输出：极性状态
     *                 Output: polarity state
     * @param enable 输出：使能状态
     *               Output: enable state
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getPwmDuty12bit(uint8_t channel, uint16_t* duty12, bool* polarity, bool* enable);

    /**
     * @brief 一次性配置 PWM 参数
     *        Configure PWM in one call
     * @param channel PWM 通道（0-3）
     *               PWM channel (0-3)
     * @param enable 输出使能（true=启用，false=禁用）
     *              Enable output (true=enable, false=disable)
     * @param polarity 极性（false=正常，true=反相）
     *                Polarity (false=normal, true=inverted)
     * @param frequency PWM 频率（0-65535，全通道共享）
     *                  PWM frequency in Hz (0-65535, shared by all channels)
     * @param duty12 12 位占空比（0-4095）
     *               12-bit duty (0-4095)
     * @note 此 API 对冲突仅告警，仍会继续配置
     *       This API warns on conflicts but still applies settings
     * @note 变更频率会影响所有通道
     *       Changing frequency affects all channels
     */
    m5ioe1_err_t setPwmConfig(uint8_t channel, bool enable, bool polarity, uint16_t frequency, uint16_t duty12);

    /**
     * @brief Arduino 兼容的 analogWrite 函数（PWM 输出）
     *        Arduino-compatible analogWrite function (PWM output)
     * @param channel PWM 通道（0-3）：M5IOE1_PWM_CH1/CH2/CH3/CH4
     *               PWM channel (0-3): M5IOE1_PWM_CH1/CH2/CH3/CH4
     *               CH1=IO9, CH2=IO8, CH3=IO11, CH4=IO10
     * @param value PWM 占空比（0-255，8-bit Arduino 标准）
     *              PWM duty cycle (0-255, 8-bit Arduino standard)
     *              0 = 0% duty, 127 = 50% duty, 255 = 100% duty
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 此函数内部将 8-bit 值缩放到 12-bit
     *       This function scales 8-bit value to 12-bit internally
     * @note 值为 0 时关闭 PWM 输出
     *       Value 0 turns off PWM output
     */
    m5ioe1_err_t analogWrite(uint8_t channel, uint8_t value);

    // ========================
    // NeoPixel LED 功能
    // NeoPixel LED Functions
    // ========================
    /**
     * @brief 一次性配置并设置所有 NeoPixel LED / Configure and set all NeoPixel LEDs at once
     * @param colors RGB 颜色数组 / Array of RGB colors
     * @param arraySize 颜色数组的大小（用于边界检查） / Size of the colors array (for bounds checking)
     * @param count 要设置的 LED 数量（1-32） / Number of LEDs to set (1-32)
     * @param autoRefresh 如果为 true，设置后自动刷新 LED（默认：true）
     *                    If true, automatically refresh LEDs after setting (default: true)
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 此函数将：
     *       1. 验证参数（count <= arraySize，count <= 32）
     *       2. 设置 LED 数量寄存器
     *       3. 将所有 RGB 数据写入 LED RAM
     *       4. 可选触发刷新
     * @note This function will:
     *       1. Validate parameters (count <= arraySize, count <= 32)
     *       2. Set LED count register
     *       3. Write all RGB data to LED RAM
     *       4. Optionally trigger refresh
     * @example
     *   m5ioe1_rgb_t leds[8] = {
     *       {255, 0, 0},    // Red
     *       {0, 255, 0},    // Green
     *       {0, 0, 255},    // Blue
     *       {255, 255, 0},  // Yellow
     *       {255, 0, 255},  // Magenta
     *       {0, 255, 255},  // Cyan
     *       {255, 255, 255},// White
     *       {128, 128, 128} // Gray
     *   };
     *   ioe1.setLeds(leds, sizeof(leds)/sizeof(leds[0]), 8, true);
     */
    m5ioe1_err_t setLeds(const m5ioe1_rgb_t* colors, uint8_t arraySize, uint8_t count, bool autoRefresh = true);
    /**
     * @brief 设置 NeoPixel LED 数量
     *        Set NeoPixel LED count
     * @param count LED 数量（0-32）
     *              LED count (0-32)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setLedCount(uint8_t count);
    /**
     * @brief 设置单个 LED 颜色（RGB888）
     *        Set a single LED color (RGB888)
     * @param index LED 索引（0-31）
     *              LED index (0-31)
     * @param r 红色分量（0-255）
     *          Red component (0-255)
     * @param g 绿色分量（0-255）
     *          Green component (0-255)
     * @param b 蓝色分量（0-255）
     *          Blue component (0-255)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    /**
     * @brief 设置单个 LED 颜色（结构体）
     *        Set a single LED color (struct)
     * @param index LED 索引（0-31）
     *              LED index (0-31)
     * @param color RGB 颜色结构体
     *              RGB color struct
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setLedColor(uint8_t index, m5ioe1_rgb_t color);
    /**
     * @brief 刷新 LED 显示
     *        Refresh LED display
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t refreshLeds();
    /**
     * @brief 禁用所有 LED 并清零配置
     *        Disable all LEDs and clear configuration
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t disableLeds();

    /**
     * @brief 清除 LED RAM 寄存器（将所有 LED 数据设置为 0）
     *        Clear LED RAM registers (set all LED data to 0)
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 此函数会清除所有 32 个 LED 的颜色数据，建议在 begin() 后调用
     *       This function clears color data for all 32 LEDs, recommended to call after begin()
     */
    m5ioe1_err_t clearLedRam();

    // ========================
    // AW8737A 脉冲功能
    // AW8737A Pulse Functions
    // ========================
    /**
     * @brief 设置 AW8737A 脉冲输出配置
     *        Set AW8737A pulse output configuration
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param pulseNum 脉冲数量（0-3）
     *                 Number of pulses (0-3)
     * @param refresh 刷新控制：WAIT 或 NOW
     *                Refresh control: WAIT or NOW
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 如果引脚不是输出模式，会自动配置为推挽输出；如果已是输出模式则保持原有配置
     *       If pin is not output mode, it will be auto-configured as push-pull output; if already output, keeps current
     * config
     * @note 如果使用开漏输出，需要外部上拉电阻
     *       If using open-drain output, external pull-up is required
     * @note 当 refresh=NOW 时，执行后会有 20ms 延迟
     *       When refresh=NOW, there will be a 20ms delay after execution
     */
    m5ioe1_err_t setAw8737aPulse(uint8_t pin, m5ioe1_aw8737a_pulse_t pulseNum,
                                 m5ioe1_aw8737a_refresh_t refresh = M5IOE1_AW8737A_REFRESH_NOW);

    /**
     * @brief 触发 AW8737A 脉冲刷新
     *        Trigger AW8737A pulse refresh
     * @return M5IOE1_OK if successful, error code otherwise
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
    m5ioe1_err_t refreshAw8737aPulse();

    /**
     * @brief 设置 AW8737A 增益模式并可选刷新
     *        Set AW8737A gain mode with optional refresh
     * @param pin GPIO 引脚号（0-13）
     *            GPIO pin number (0-13)
     * @param mode 增益模式 (MODE_1 到 MODE_4)
     *             Gain mode (MODE_1 to MODE_4)
     * @param refresh 刷新控制：WAIT 或 NOW
     *                Refresh control: WAIT or NOW
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 如果引脚不是输出模式，会自动配置为推挽输出；如果已是输出模式则保持原有配置
     *       If pin is not output mode, it will be auto-configured as push-pull output; if already output, keeps current
     * config
     * @note 如果使用开漏输出，需要外部上拉电阻
     *       If using open-drain output, external pull-up is required
     * @note 当 refresh=NOW 时，执行后会有 20ms 延迟
     *       When refresh=NOW, there will be a 20ms delay after execution
     */
    m5ioe1_err_t setAw8737aMode(uint8_t pin, m5ioe1_aw8737a_mode_t mode,
                                m5ioe1_aw8737a_refresh_t refresh = M5IOE1_AW8737A_REFRESH_NOW);

    /**
     * @brief 刷新 AW8737A 模式配置
     *        Refresh AW8737A mode configuration
     * @return M5IOE1_OK if successful, error code otherwise
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
    m5ioe1_err_t refreshAw8737aMode();

    // ========================
    // RTC RAM 功能
    // RTC RAM Functions
    // ========================
    /**
     * @brief 写入 RTC RAM 并回读验证
     *        Write RTC RAM with read-back verification
     * @param offset 起始偏移（0-31）
     *               Start offset (0-31)
     * @param data 输入数据指针
     *             Input data pointer
     * @param length 数据长度（1-32，且 offset+length <= 32）
     *               Data length (1-32, offset+length <= 32)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t writeRtcRAM(uint8_t offset, const uint8_t* data, uint8_t length);
    /**
     * @brief 读取 RTC RAM
     *        Read RTC RAM
     * @param offset 起始偏移（0-31）
     *               Start offset (0-31)
     * @param data 输出数据缓冲区
     *             Output data buffer
     * @param length 数据长度（1-32，且 offset+length <= 32）
     *               Data length (1-32, offset+length <= 32)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t readRtcRAM(uint8_t offset, uint8_t* data, uint8_t length);

    // ========================
    // 系统配置
    // System Configuration
    // ========================

    /**
     * @brief 一次性设置所有 I2C 配置 / Set all I2C configuration at once
     * @param sleepTime 休眠超时（0=禁用，1-15=超时值）
     * @param sleepTime Sleep timeout (0=disabled, 1-15=timeout value)
     * @param speed I2C 速度（M5IOE1_I2C_SPEED_100K 或 M5IOE1_I2C_SPEED_400K）
     * @param speed I2C speed (M5IOE1_I2C_SPEED_100K or M5IOE1_I2C_SPEED_400K)
     * @param wakeEdge 唤醒边沿（M5IOE1_WAKE_EDGE_FALLING 或 M5IOE1_WAKE_EDGE_RISING）
     * @param wakeEdge Wake edge (M5IOE1_WAKE_EDGE_FALLING or M5IOE1_WAKE_EDGE_RISING)
     * @param pullConfig 上拉配置（M5IOE1_PULL_ENABLED 或 M5IOE1_PULL_DISABLED）
     * @param pullConfig Pull-up config (M5IOE1_PULL_ENABLED or M5IOE1_PULL_DISABLED)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setI2cConfig(uint8_t sleepTime, m5ioe1_i2c_speed_t speed = M5IOE1_I2C_SPEED_100K,
                              m5ioe1_wake_edge_t wakeEdge     = M5IOE1_WAKE_EDGE_FALLING,
                              m5ioe1_pull_config_t pullConfig = M5IOE1_PULL_ENABLED);

    /**
     * @brief 切换 I2C 通讯速度 / Switch I2C communication speed
     * @param speed 目标速度 (M5IOE1_I2C_SPEED_100K 或 M5IOE1_I2C_SPEED_400K)
     * @param speed Target speed (M5IOE1_I2C_SPEED_100K or M5IOE1_I2C_SPEED_400K)
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 此函数会同时配置设备和主机 I2C 总线
     * @note This function will configure both the device and host I2C bus
     */
    m5ioe1_err_t switchI2cSpeed(m5ioe1_i2c_speed_t speed);

    /**
     * @brief 获取当前 I2C 速度设置 / Get current I2C speed setting
     * @param speed 存储速度值的指针 / Pointer to store the speed value
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 从设备寄存器读取并更新内部缓存
     * @note Reads from device register and updates internal cache
     */
    m5ioe1_err_t getI2cSpeed(m5ioe1_i2c_speed_t* speed);

    /**
     * @brief 设置 I2C 休眠超时 / Set I2C sleep timeout
     * @param sleepTime 休眠超时值（0=禁用，1-15=超时）
     * @param sleepTime Sleep timeout value (0=disabled, 1-15=timeout)
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 休眠时间公式：T = sleepTime * 基础时间
     * @note Sleep time formula: T = sleepTime * base_time
     */
    m5ioe1_err_t setI2cSleepTime(uint8_t sleepTime);

    /**
     * @brief 获取当前 I2C 休眠超时 / Get current I2C sleep timeout
     * @param sleepTime 存储休眠超时值的指针 / Pointer to store the sleep timeout value
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 从设备寄存器读取并更新内部缓存
     * @note Reads from device register and updates internal cache
     */
    m5ioe1_err_t getI2cSleepTime(uint8_t* sleepTime);

    /**
     * @brief 设置 I2C 唤醒边沿 / Set I2C wake edge
     * @param edge 唤醒边沿（M5IOE1_WAKE_EDGE_FALLING 或 M5IOE1_WAKE_EDGE_RISING）
     * @param edge Wake edge (M5IOE1_WAKE_EDGE_FALLING or M5IOE1_WAKE_EDGE_RISING)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setI2cWakeEdge(m5ioe1_wake_edge_t edge);

    /**
     * @brief 获取当前 I2C 唤醒边沿 / Get current I2C wake edge
     * @param edge 存储唤醒边沿设置的指针 / Pointer to store the wake edge setting
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 从设备寄存器读取并更新内部缓存
     * @note Reads from device register and updates internal cache
     */
    m5ioe1_err_t getI2cWakeEdge(m5ioe1_wake_edge_t* edge);

    /**
     * @brief 设置 I2C 内部上拉配置 / Set I2C internal pull-up configuration
     * @param config 上拉配置（M5IOE1_PULL_ENABLED 或 M5IOE1_PULL_DISABLED）
     * @param config Pull-up config (M5IOE1_PULL_ENABLED or M5IOE1_PULL_DISABLED)
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t setI2cPullConfig(m5ioe1_pull_config_t config);

    /**
     * @brief 获取当前 I2C 内部上拉配置 / Get current I2C internal pull-up configuration
     * @param config 存储上拉配置的指针 / Pointer to store the pull-up configuration
     * @return M5IOE1_OK if successful, error code otherwise
     * @note 从设备寄存器读取并更新内部缓存
     * @note Reads from device register and updates internal cache
     */
    m5ioe1_err_t getI2cPullConfig(m5ioe1_pull_config_t* config);

    /**
     * @brief 恢复出厂设置 / Factory reset the device
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t factoryReset();

    // ========================
    // 自动唤醒功能
    // Auto Wake Feature
    // ========================
    /**
     * @brief 启用/禁用 I2C 操作前的自动唤醒信号
     *        Enable/disable automatic wake signal before I2C operations
     * @param enable true to enable auto-wake, false to disable
     * @note 当 IOE1 进入睡眠模式（I2C 睡眠超时）后，需要在 SDA 上发送
     *       START 信号来唤醒。此功能会在需要时自动发送唤醒信号。
     *       When IOE1 enters sleep mode (I2C sleep timeout), it needs a
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
     * @brief 手动发送唤醒信号到 IOE1 / Manually send wake signal to IOE1
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t sendWakeSignal();

    // ========================
    // 状态快照功能
    // State Snapshot Functions
    // ========================
    /**
     * @brief 启用/禁用自动快照更新
     *        Enable/disable automatic snapshot updates
     * @param enable true 启用，false 禁用
     *               true to enable, false to disable
     */
    void setAutoSnapshot(bool enable);
    /**
     * @brief 查询自动快照是否启用
     *        Check if auto snapshot is enabled
     * @return true if enabled
     */
    bool isAutoSnapshotEnabled() const;
    /**
     * @brief 立即更新快照缓存
     *        Update snapshot cache immediately
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t updateSnapshot();

    // ========================
    // 调试功能
    // Debug Functions
    // ========================
    /**
     * @brief 读取 GPIO 模式寄存器
     *        Read GPIO mode register
     * @param reg 输出：寄存器值
     *            Output: register value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getModeReg(uint16_t* reg);
    /**
     * @brief 读取 GPIO 输出寄存器
     *        Read GPIO output register
     * @param reg 输出：寄存器值
     *            Output: register value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getOutputReg(uint16_t* reg);
    /**
     * @brief 读取 GPIO 输入寄存器
     *        Read GPIO input register
     * @param reg 输出：寄存器值
     *            Output: register value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getInputReg(uint16_t* reg);
    /**
     * @brief 读取 GPIO 上拉寄存器
     *        Read GPIO pull-up register
     * @param reg 输出：寄存器值
     *            Output: register value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getPullUpReg(uint16_t* reg);
    /**
     * @brief 读取 GPIO 下拉寄存器
     *        Read GPIO pull-down register
     * @param reg 输出：寄存器值
     *            Output: register value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getPullDownReg(uint16_t* reg);
    /**
     * @brief 读取 GPIO 驱动寄存器
     *        Read GPIO drive register
     * @param reg 输出：寄存器值
     *            Output: register value
     * @return M5IOE1_OK if successful, error code otherwise
     */
    m5ioe1_err_t getDriveReg(uint16_t* reg);

    // ========================
    // 配置验证
    // Configuration Validation
    // ========================
    /**
     * @brief Validate pin configuration before applying
     * @param pin Pin number (0-13)
     * @param configType Configuration type (m5ioe1_config_type_t)
     * @param enable true to enable config, false to disable
     * @return Validation result with error details if invalid
     */
    m5ioe1_validation_t validateConfig(uint8_t pin, m5ioe1_config_type_t configType, bool enable = true);

    // ========================
    // 快照验证
    // Snapshot Verification
    // ========================
    /**
     * @brief Verify that cached state matches actual hardware registers
     * @return Verification result with mismatch details
     */
    m5ioe1_snapshot_verify_t verifySnapshot();

    // ========================
    // 缓存状态查询函数
    // Cached State Query Functions
    // ========================
    /**
     * @brief Get cached PWM frequency
     * @param frequency Output: PWM frequency in Hz
     * @return M5IOE1_OK if cache is valid, M5IOE1_FAIL if cache invalid
     */
    m5ioe1_err_t getCachedPwmFrequency(uint16_t* frequency);

    /**
     * @brief Get cached PWM channel state
     * @param channel PWM channel (0-3)
     * @param duty12 Output: 12-bit duty value
     * @param polarity Output: polarity setting
     * @param enabled Output: enable state
     * @return M5IOE1_OK if cache is valid, M5IOE1_FAIL if cache invalid
     */
    m5ioe1_err_t getCachedPwmState(uint8_t channel, uint16_t* duty12, bool* polarity, bool* enabled);

    /**
     * @brief Get cached ADC state
     * @param activeChannel Output: current ADC channel (0=disabled, 1-4=channel)
     * @param busy Output: conversion status
     * @param lastValue Output: last conversion result
     * @return M5IOE1_OK if cache is valid, M5IOE1_FAIL if cache invalid
     */
    m5ioe1_err_t getCachedAdcState(uint8_t* activeChannel, bool* busy, uint16_t* lastValue);

    /**
     * @brief Get cached GPIO pin state
     * @param pin Pin number (0-13)
     * @param isOutput Output: true if pin is output
     * @param level Output: current level (output or input)
     * @param pull Output: pull mode (0=none, 1=up, 2=down)
     * @return M5IOE1_OK if cache is valid, M5IOE1_FAIL if cache invalid
     */
    m5ioe1_err_t getCachedPinState(uint8_t pin, bool* isOutput, uint8_t* level, uint8_t* pull);

    /**
     * @brief Enable or disable default interrupt logging when no callback is registered
     * @param enable true to enable logging, false to disable (default)
     */
    void enableDefaultInterruptLog(bool enable);

private:
    // 设备状态
    // Device state
    uint8_t _addr;
    bool _initialized;
    bool _autoSnapshot;
    bool _enableDefaultIsrLog;
    uint32_t _requestedSpeed;  // 用户请求的 I2C 速度（用于 400K 切换）
                               // User requested I2C speed (for 400K switch)

    // 自动唤醒状态
    // Auto wake state
    bool _autoWakeEnabled;   // 自动唤醒是否启用
                             // Whether auto wake is enabled
    uint32_t _lastCommTime;  // 上次通信时间（毫秒）
                             // Last communication time (milliseconds)

    // 中断模式
    // Interrupt mode
    m5ioe1_int_mode_t _intMode;
    int8_t _intPin;
    uint32_t _pollingInterval;

#ifdef ARDUINO
    TwoWire* _wire;
    uint8_t _sda;  // SDA 引脚编号，用于 I2C 重新初始化
                   // SDA pin number for I2C re-initialization
    uint8_t _scl;  // SCL 引脚编号，用于 I2C 重新初始化
                   // SCL pin number for I2C re-initialization
#if M5IOE1_HAS_M5UNIFIED_I2C
    m5::I2C_Class* _m5_i2c;
    uint32_t _commFreq;  // M5UNIFIED 路径专用，init 时从 100K 起，完成后切换至 _requestedSpeed
                         // M5UNIFIED path: starts at 100K during init, switches to _requestedSpeed after
#endif
#else
    // I2C 驱动类型选择
    // I2C driver type selection
    m5ioe1_i2c_driver_t _i2cDriverType;

    // I2C 句柄（根据驱动类型仅使用一对）
    // I2C handles (only one pair is used based on driver type)
    // M5IOE1_I2C_DRIVER_SELF_CREATED (IDF >= 5.3.0): 使用 _i2c_master_bus + _i2c_master_dev
    // uses _i2c_master_bus + _i2c_master_dev
    // M5IOE1_I2C_DRIVER_MASTER: 使用 _i2c_master_bus + _i2c_master_dev
    // uses _i2c_master_bus + _i2c_master_dev
    // M5IOE1_I2C_DRIVER_BUS: 使用 _i2c_bus + _i2c_device
    // uses _i2c_bus + _i2c_device
    // M5IOE1_I2C_DRIVER_LEGACY (IDF < 5.3.0): 使用 _port + _addr，无额外句柄
    // uses _port + _addr, no additional handle
#if M5IOE1_HAS_I2C_MASTER
    i2c_master_bus_handle_t _i2c_master_bus;  // ESP-IDF 原生驱动/自创建 (IDF >= 5.3.0)
                                              // ESP-IDF native driver / self-created (IDF >= 5.3.0)
    i2c_master_dev_handle_t _i2c_master_dev;  // ESP-IDF 原生驱动/自创建 (IDF >= 5.3.0)
                                              // ESP-IDF native driver / self-created (IDF >= 5.3.0)
#endif  // M5IOE1_HAS_I2C_MASTER
#if M5IOE1_HAS_I2C_BUS
    i2c_bus_handle_t _i2c_bus;            // esp-idf-lib 组件
                                          // esp-idf-lib component
    i2c_bus_device_handle_t _i2c_device;  // esp-idf-lib 组件
                                          // esp-idf-lib component
#endif

    // I2C 管理标志
    // I2C management flags
    bool _busExternal;  // 如果总线句柄由外部提供则为 true
                        // true if bus handle is provided externally

    // 自创建总线的 I2C 引脚（用于频率切换）
    // I2C pins for self-created bus (for frequency switching)
    int _sda;
    int _scl;
    i2c_port_t _port;

    // 中断处理
    // Interrupt handling
    QueueHandle_t _intrQueue;

    // M5Unified I2C_Class 借用句柄及当前通信频率
    // Borrowed M5Unified I2C_Class handle and current communication frequency
#if M5IOE1_HAS_M5UNIFIED_I2C
    m5::I2C_Class* _m5_i2c;
    uint32_t _commFreq;  // M5UNIFIED 路径专用，init 时从 100K 起，完成后切换至 _requestedSpeed
                         // M5UNIFIED path: starts at 100K during init, switches to _requestedSpeed after
#endif
#endif

    // 中断轮询任务句柄（Arduino / ESP-IDF 共用）
    // Interrupt poll task handle (shared by Arduino and ESP-IDF)
    TaskHandle_t _pollTask;

    // 中断回调
    // Interrupt callbacks
    struct {
        m5ioe1_callback_t callback;
        m5ioe1_callback_arg_t callbackArg;
        void* arg;
        bool enabled;
        bool rising;
    } _callbacks[M5IOE1_MAX_GPIO_PINS];

    // 缓存的引脚状态
    // Cached pin states
    struct {
        bool isOutput;
        uint8_t outputLevel;
        uint8_t inputLevel;
        uint8_t pull;   // 0:无, 1:上拉, 2:下拉
                        // 0:none, 1:up, 2:down
        uint8_t drive;  // 0:推挽, 1:开漏
                        // 0:push-pull, 1:open-drain
        bool intrEnabled;
        bool intrRising;
    } _pinStates[M5IOE1_MAX_GPIO_PINS];
    bool _pinStatesValid;

    // 缓存的 PWM 状态
    // Cached PWM states
    struct {
        uint16_t duty12;
        bool enabled;
        bool polarity;
    } _pwmStates[M5IOE1_MAX_PWM_CHANNELS];
    uint16_t _pwmFrequency;
    bool _pwmStatesValid;

    // 缓存的 ADC 状态
    // Cached ADC state
    struct {
        uint8_t activeChannel;
        bool busy;
        uint16_t lastValue;
    } _adcState;
    bool _adcStateValid;

    // 缓存的 I2C 配置状态（用于睡眠模式检测）
    // Cached I2C config state (for sleep mode detection)
    struct {
        uint8_t sleepTime;  // 0=禁用, 1-15=睡眠时间
                            // 0=disabled, 1-15=sleep time
        bool speed400k;     // I2C 速度模式
                            // I2C speed mode
        bool wakeRising;    // 唤醒边沿模式
                            // Wake edge mode
        bool pullOff;       // 内部上拉关闭
                            // Internal pull-up off
    } _i2cConfig;
    bool _i2cConfigValid;

    // NeoPixel 状态
    // NeoPixel state

    // AW8737A 配置状态缓存
    // AW8737A configuration state cache
    bool _aw8737aConfigured;                  // 是否已调用 setAw8737aPulse 配置
                                              // Whether setAw8737aPulse has been called
    uint8_t _aw8737aPin;                      // 配置的引脚号
                                              // Configured pin number
    m5ioe1_aw8737a_pulse_t _aw8737aPulseNum;  // 配置的脉冲数
                                              // Configured pulse count
    uint8_t _aw8737aRegValue;                 // 缓存的寄存器值（不含 REFRESH 位）
                                              // Cached register value (without REFRESH bit)
    bool _aw8737aStateValid;                  // 缓存有效性标志
                                              // Cache validity flag

    // ========================
    // 内部辅助函数
    // Internal Helper Functions
    // ========================
    m5ioe1_err_t _pinModeWithErr(uint8_t pin, uint8_t mode);
    m5ioe1_err_t _digitalWriteWithErr(uint8_t pin, uint8_t value);
    m5ioe1_err_t _digitalReadWithErr(uint8_t pin, int* value);

    bool _writeReg(uint8_t reg, uint8_t value);
    bool _writeReg16(uint8_t reg, uint16_t value);
    bool _readReg(uint8_t reg, uint8_t* value);
    bool _readReg16(uint8_t reg, uint16_t* value);
    bool _writeBytes(uint8_t reg, const uint8_t* data, uint8_t len);
    bool _readBytes(uint8_t reg, uint8_t* data, uint8_t len);

    bool _isValidPin(uint8_t pin);
    bool _isAdcPin(uint8_t pin);
    bool _isPwmPin(uint8_t pin);
    uint8_t _getAdcChannel(uint8_t pin);
    uint8_t _getPwmChannel(uint8_t pin);

    void _clearPinStates();
    void _clearPwmStates();
    void _clearAdcState();
    bool _snapshotPinStates();
    bool _snapshotPwmStates();
    bool _snapshotAdcState();
    bool _snapshotAw8737a();
    void _clearAw8737a();
    void _autoSnapshotUpdate(uint8_t domains = M5IOE1_SNAPSHOT_DOMAIN_ALL);

    bool _initDevice();
    void _handleInterrupt();

    // 自动唤醒检查
    // Auto wake check
    void _checkAutoWake();

    // I2C 休眠与轮询任务联动
    // I2C sleep and polling task linkage
    void _updatePollingForI2cSleep(uint8_t sleepTime);

    // I2C 频率验证
    // I2C frequency validation
    bool _isValidI2cFrequency(uint32_t speed);

    // 中断互斥对检查
    // Interrupt mutex pairs check
    static bool _pinsConflict(uint8_t a, uint8_t b);
    bool _hasConflictingInterrupt(uint8_t pin);

    // 配置验证辅助函数
    // Configuration validation helpers
    bool _getInterruptMutexPin(uint8_t pin, uint8_t* mutexPin);
    bool _isNeopixelPin(uint8_t pin);
    bool _hasActiveInterrupt(uint8_t pin);
    bool _hasActiveAdc(uint8_t pin);
    bool _hasActivePwm(uint8_t pin);
    bool _hasI2cSleepEnabled();
    bool _isLedEnabled();

    // I2C 配置快照
    // I2C config snapshot
    void _clearI2cConfig();
    bool _snapshotI2cConfig();

#ifdef ARDUINO
    // Arduino 专用
    // Arduino specific
    bool _setupPollingArduino();
    void _cleanupPollingArduino();
    static void _pollTaskArduino(void* arg);
    bool _setupHardwareInterruptArduino();
    void _cleanupHardwareInterruptArduino();
    static void _intrTaskArduino(void* arg);
    static void IRAM_ATTR _arduinoIsrHandler(void* arg);
#else
    // ESP-IDF 专用
    // ESP-IDF specific
    static void _pollTaskFunc(void* arg);
    static void IRAM_ATTR _isrHandler(void* arg);
    bool _setupHardwareInterrupt();
    bool _setupPolling();
    void _cleanupPolling();
    void _cleanupHardwareInterrupt();
#endif
};

#endif  // _M5IOE1_H_
