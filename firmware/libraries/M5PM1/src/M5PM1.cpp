/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "M5PM1.h"
#include <string.h>
#include <stdio.h>

static const char* TAG      = "M5PM1";       // 保留用于 setLogLevel 等极少数场景
static const char* TAG_I2C  = "M5PM1_I2C";   // I2C 总线初始化、底层读写、I2C 配置
static const char* TAG_GPIO = "M5PM1_GPIO";  // GPIO 引脚操作
static const char* TAG_ADC  = "M5PM1_ADC";   // ADC 采样、电压读取
static const char* TAG_PWM  = "M5PM1_PWM";   // PWM 频率/占空比控制
static const char* TAG_LED  = "M5PM1_LED";   // WS2812 LED + LED 使能
static const char* TAG_AMP  = "M5PM1_AMP";   // AW8737A 功放控制
static const char* TAG_PWR  = "M5PM1_PWR";   // 电源管理 (DCDC/LDO/Boost/充电/电池/关机/重启)
static const char* TAG_BTN  = "M5PM1_BTN";   // 按钮配置与状态
static const char* TAG_IRQ  = "M5PM1_IRQ";   // 中断掩码/状态/清除
static const char* TAG_SYS  = "M5PM1_SYS";   // 系统级 (设备信息/定时器/WDT/RTC/系统配置)

#ifdef ARDUINO
#include <Arduino.h>
#define M5PM1_DELAY_MS(ms)  delay(ms)
#define M5PM1_GET_TIME_MS() millis()

// Arduino 日志级别控制
// Arduino log level control
static m5pm1_log_level_t _m5pm1_log_level = M5PM1_LOG_LEVEL_INFO;

#define M5PM1_LOG_I(tag, fmt, ...)                                    \
    do {                                                              \
        if (_m5pm1_log_level >= M5PM1_LOG_LEVEL_INFO) {               \
            Serial.printf("[I][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5PM1_LOG_W(tag, fmt, ...)                                    \
    do {                                                              \
        if (_m5pm1_log_level >= M5PM1_LOG_LEVEL_WARN) {               \
            Serial.printf("[W][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5PM1_LOG_E(tag, fmt, ...)                                    \
    do {                                                              \
        if (_m5pm1_log_level >= M5PM1_LOG_LEVEL_ERROR) {              \
            Serial.printf("[E][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5PM1_LOG_D(tag, fmt, ...)                                    \
    do {                                                              \
        if (_m5pm1_log_level >= M5PM1_LOG_LEVEL_DEBUG) {              \
            Serial.printf("[D][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5PM1_LOG_V(tag, fmt, ...)                                    \
    do {                                                              \
        if (_m5pm1_log_level >= M5PM1_LOG_LEVEL_VERBOSE) {            \
            Serial.printf("[V][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)
#else
// 强制本组件编译所有级别的日志，实际输出由运行时的 esp_log_level_set 控制
// Force compile all log levels for this component; actual output controlled by esp_log_level_set at runtime
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define M5PM1_DELAY_MS(ms)         vTaskDelay(pdMS_TO_TICKS(ms))
#define M5PM1_GET_TIME_MS()        (xTaskGetTickCount() * portTICK_PERIOD_MS)
#define M5PM1_LOG_I(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define M5PM1_LOG_W(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define M5PM1_LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define M5PM1_LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define M5PM1_LOG_V(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)

// ESP-IDF 平台日志级别控制
// ESP-IDF platform log level control
static m5pm1_log_level_t _m5pm1_current_log_level = M5PM1_LOG_LEVEL_INFO;
#endif

// ============================
// 全局日志级别控制
// Global Log Level Control
// ============================

void M5PM1::setLogLevel(m5pm1_log_level_t level)
{
#ifdef ARDUINO
    _m5pm1_log_level = level;
#else
    _m5pm1_current_log_level = level;

    // 将 M5PM1 日志级别映射到 ESP-IDF 日志级别
    // Map M5PM1 log level to ESP-IDF log level
    esp_log_level_t esp_level;
    switch (level) {
        case M5PM1_LOG_LEVEL_NONE:
            esp_level = ESP_LOG_NONE;
            break;
        case M5PM1_LOG_LEVEL_ERROR:
            esp_level = ESP_LOG_ERROR;
            break;
        case M5PM1_LOG_LEVEL_WARN:
            esp_level = ESP_LOG_WARN;
            break;
        case M5PM1_LOG_LEVEL_INFO:
            esp_level = ESP_LOG_INFO;
            break;
        case M5PM1_LOG_LEVEL_DEBUG:
            esp_level = ESP_LOG_DEBUG;
            break;
        case M5PM1_LOG_LEVEL_VERBOSE:
            esp_level = ESP_LOG_VERBOSE;
            break;
        default:
            esp_level = ESP_LOG_INFO;
            break;
    }

    esp_log_level_set(TAG, esp_level);
#endif
}

m5pm1_log_level_t M5PM1::getLogLevel()
{
#ifdef ARDUINO
    return _m5pm1_log_level;
#else
    return _m5pm1_current_log_level;
#endif
}

// ============================
// 构造函数 / 析构函数
// Constructor / Destructor
// ============================

M5PM1::M5PM1()
{
    _addr                = M5PM1_DEFAULT_ADDR;
    _initialized         = false;
    _autoWakeEnabled     = false;
    _autoSnapshot        = true;
    _i2cSleepTime        = 0;
    _requestedSpeed      = M5PM1_I2C_FREQ_100K;
    _i2cConfig.sleepTime = 0;
    _i2cConfig.speed400k = false;
    _i2cConfigValid      = false;
    _lastCommTime        = 0;
    memset(_pwmStates, 0, sizeof(_pwmStates));
    _pwmFrequency       = 0;
    _pwmStatesValid     = false;
    _adcState.channel   = 0;
    _adcState.busy      = false;
    _adcState.lastValue = 0;
    _adcStateValid      = false;
    _powerCfg           = 0;
    _holdCfg            = 0;
    _powerConfigValid   = false;
    _btnCfg1            = 0;
    _btnCfg2            = 0;
    _btnConfigValid     = false;
    _btnFlagCache       = false;
    _irqMask1           = 0;
    _irqMask2           = 0;
    _irqMask3           = 0;
    _irqMaskValid       = false;
    _irqStatus1         = 0;
    _irqStatus2         = 0;
    _irqStatus3         = 0;
    _irqStatusValid     = false;
    _neoCfg             = 0;
    _neoConfigValid     = false;
    _aw8737aConfigured  = false;
    _aw8737aPin         = M5PM1_GPIO_NUM_0;
    _aw8737aPulseNum    = M5PM1_AW8737A_PULSE_0;
    _aw8737aRegValue    = 0;
    _aw8737aStateValid  = false;
    _cacheValid         = false;

#ifdef ARDUINO
    _wire = nullptr;
    _sda  = -1;
    _scl  = -1;
#if M5PM1_HAS_M5UNIFIED_I2C
    _m5_i2c   = nullptr;
    _commFreq = 0;
#endif
#else
    _i2cDriverType = M5PM1_I2C_DRIVER_NONE;
#if M5PM1_HAS_I2C_MASTER
    _i2c_master_bus = nullptr;
    _i2c_master_dev = nullptr;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
    _i2c_bus    = nullptr;
    _i2c_device = nullptr;
#endif
    _busExternal = false;
    _sda         = -1;
    _scl         = -1;
    _port        = I2C_NUM_0;
#if M5PM1_HAS_M5UNIFIED_I2C
    _m5_i2c   = nullptr;
    _commFreq = M5PM1_I2C_FREQ_100K;
#endif
#endif
}

M5PM1::~M5PM1()
{
#ifndef ARDUINO
    // Cleanup based on driver type
    switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
        case M5PM1_I2C_DRIVER_SELF_CREATED:
            if (_i2c_master_dev) {
                i2c_master_bus_rm_device(_i2c_master_dev);
                _i2c_master_dev = nullptr;
            }
            if (_i2c_master_bus) {
                i2c_del_master_bus(_i2c_master_bus);
                _i2c_master_bus = nullptr;
            }
            break;

        case M5PM1_I2C_DRIVER_MASTER:
            if (_i2c_master_dev) {
                i2c_master_bus_rm_device(_i2c_master_dev);
                _i2c_master_dev = nullptr;
            }
            break;
#endif  // M5PM1_HAS_I2C_MASTER

#if M5PM1_HAS_I2C_BUS
        case M5PM1_I2C_DRIVER_BUS:
            if (_i2c_device) {
                i2c_bus_device_delete(&_i2c_device);
                _i2c_device = nullptr;
            }
            break;
#endif

#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
        case M5PM1_I2C_DRIVER_LEGACY:
            // 传统驱动由 i2c_driver_delete 管理，仅在 SELF_CREATED 时才卸载
            // Legacy driver is managed by i2c_driver_delete, uninstall only when self-created
            if (!_busExternal) {
                i2c_driver_delete(_port);
            }
            break;
#endif

        default:
            break;
    }
#endif
}

// ============================
// 初始化函数
// Initialization Functions
// ============================

#ifdef ARDUINO

m5pm1_err_t M5PM1::begin(TwoWire* wire, uint8_t addr, int8_t sda, int8_t scl, uint32_t speed)
{
    _wire = wire;
    _addr = addr;
    _sda  = sda;
    _scl  = scl;

    // 步骤1：校验用户频率并记录
    // Step 1: Validate requested speed and store it
    if (!_isValidI2cFrequency(speed)) {
        M5PM1_LOG_W(TAG_I2C,
                    "Invalid I2C frequency: %lu Hz. PM1 only supports 100KHz or 400KHz. Falling back to 100KHz.",
                    (unsigned long)speed);
        _requestedSpeed = M5PM1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    // 步骤2：以100KHz初始化主机I2C
    // Step 2: Initialize host I2C at 100KHz
    _wire->end();
    M5PM1_DELAY_MS(50);
    if (!_wire->begin(_sda, _scl, M5PM1_I2C_FREQ_100K)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to initialize I2C bus (SDA=%d, SCL=%d)", _sda, _scl);
        return M5PM1_ERR_I2C_CONFIG;
    }
    M5PM1_DELAY_MS(100);

    // 尝试唤醒设备 - 发送 I2C START 信号
    // Try to wake up the device - send I2C START signal
    M5PM1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待800ms重试一次）
    // Step 3: Verify device communication (retry once after 800ms if failed)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        // 重试前再次发送唤醒信号
        // Send wake signal again before retry
        M5PM1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");

            _wire->end();
            M5PM1_DELAY_MS(50);
            if (!_wire->begin(_sda, _scl, M5PM1_I2C_FREQ_400K)) {
                M5PM1_LOG_E(TAG_I2C, "Failed to initialize I2C bus at 400KHz");
                return M5PM1_ERR_I2C_CONFIG;
            }
            M5PM1_DELAY_MS(50);

            // 尝试唤醒设备 - 发送 I2C START 信号
            // Try to wake up the device - send I2C START signal
            M5PM1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
            M5PM1_DELAY_MS(10);

            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }

    _initialized = true;

    // 步骤4：配置设备I2C参数（关闭睡眠 + 目标频率）
    // Step 4: Configure device I2C (sleep off + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤5：切换主机到目标频率
    // Step 5: Switch host to target speed
    _wire->end();
    M5PM1_DELAY_MS(10);
    if (!_wire->begin(_sda, _scl, _requestedSpeed)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to switch host to %lu Hz", (unsigned long)_requestedSpeed);
        _initialized = false;
        return M5PM1_ERR_I2C_CONFIG;
    }
    M5PM1_DELAY_MS(50);

    // 步骤6：刷新快照并完成初始化
    // Step 6: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, (unsigned long)_requestedSpeed);
    return M5PM1_OK;
}

#if M5PM1_HAS_M5UNIFIED_I2C
m5pm1_err_t M5PM1::begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed)
{
    if (i2c == nullptr || !i2c->isEnabled()) {
        M5PM1_LOG_E(TAG_I2C, "M5Unified I2C_Class is null or not initialized");
        return M5PM1_ERR_I2C_CONFIG;
    }

    _wire   = nullptr;
    _m5_i2c = i2c;
    _addr   = addr;
    _sda    = -1;
    _scl    = -1;

    // 步骤1：校验用户频率并记录
    // Step 1: Validate requested speed
    if (!_isValidI2cFrequency(speed)) {
        M5PM1_LOG_W(TAG_I2C, "Invalid I2C frequency: %lu Hz. Falling back to 100KHz.", (unsigned long)speed);
        _requestedSpeed = M5PM1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    // 步骤2：以 100KHz 发送唤醒信号
    // Step 2: Send wake signal at 100KHz
    _commFreq = M5PM1_I2C_FREQ_100K;
    M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待 800ms 重试一次）
    // Step 3: Verify device communication (retry once after 800ms)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");
            _commFreq = M5PM1_I2C_FREQ_400K;
            M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
            M5PM1_DELAY_MS(10);
            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                _m5_i2c = nullptr;
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }
    _initialized = true;

    // 步骤4：配置设备 I2C 参数（关闭睡眠 + 目标频率）
    // Step 4: Configure device I2C (disable sleep + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤5：切换通信频率到目标值（M5Unified 无需重装驱动，直接更新 _commFreq）
    // Step 5: Switch to target frequency (no driver reinstall needed; update _commFreq only)
    _commFreq = _requestedSpeed;

    // 步骤6：刷新快照并完成初始化
    // Step 6: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    _initialized = true;
    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, (unsigned long)_requestedSpeed);
    return M5PM1_OK;
}
#endif  // M5PM1_HAS_M5UNIFIED_I2C

#else  // ESP-IDF

m5pm1_err_t M5PM1::begin(i2c_port_t port, uint8_t addr, int sda, int scl, uint32_t speed)
{
    _addr        = addr;
    _busExternal = false;
    _port        = port;
    _sda         = sda;
    _scl         = scl;

    // 步骤1：校验用户频率并记录
    // Step 1: Validate requested speed and store it
    if (!_isValidI2cFrequency(speed)) {
        M5PM1_LOG_W(TAG_I2C,
                    "Invalid I2C frequency: %lu Hz. PM1 only supports 100KHz or 400KHz. Falling back to 100KHz.",
                    (unsigned long)speed);
        _requestedSpeed = M5PM1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

#if M5PM1_HAS_I2C_MASTER
    _i2cDriverType = M5PM1_I2C_DRIVER_SELF_CREATED;

    // 步骤2：创建I2C主总线
    // Step 2: Create I2C master bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port          = port,
        .sda_io_num        = (gpio_num_t)sda,
        .scl_io_num        = (gpio_num_t)scl,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority     = 0,
        .trans_queue_depth = 0,
        .flags =
            {
                .enable_internal_pullup = true,
                .allow_pd               = false,
            },
    };

    esp_err_t ret = i2c_new_master_bus(&bus_config, &_i2c_master_bus);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 步骤2：以100KHz创建设备句柄
    // Step 2: Create device handle at 100KHz
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = _addr,
        .scl_speed_hz    = M5PM1_I2C_FREQ_100K,
        .scl_wait_us     = 0,
        .flags =
            {
                .disable_ack_check = false,
            },
    };

    ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(_i2c_master_bus);
        _i2c_master_bus = nullptr;
        _initialized    = false;
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备
    // Try to wake up the device
    M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待800ms重试一次）
    // Step 3: Verify device communication (retry once after 800ms if failed)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        // 重试前再次发送唤醒信号
        // Send wake signal again before retry
        M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");

            // 删除当前100K设备句柄
            // Remove current 100K device handle
            i2c_master_bus_rm_device(_i2c_master_dev);
            _i2c_master_dev = nullptr;

            // 以400K重新创建设备句柄
            // Recreate device handle at 400K
            dev_config.scl_speed_hz = M5PM1_I2C_FREQ_400K;
            ret                     = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
            if (ret != ESP_OK) {
                M5PM1_LOG_E(TAG_I2C, "Failed to add I2C device at 400KHz: %s", esp_err_to_name(ret));
                i2c_del_master_bus(_i2c_master_bus);
                _i2c_master_bus = nullptr;
                return M5PM1_ERR_I2C_CONFIG;
            }

            // 尝试唤醒设备
            // Try to wake up the device
            M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
            M5PM1_DELAY_MS(10);

            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_master_bus_rm_device(_i2c_master_dev);
                i2c_del_master_bus(_i2c_master_bus);
                _i2c_master_dev = nullptr;
                _i2c_master_bus = nullptr;
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }
    _initialized = true;

    // 步骤5：配置设备I2C参数（关闭睡眠 + 目标频率）
    // Step 5: Configure device I2C (sleep off + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤6：重建设备句柄到目标频率
    // Step 6: Recreate device handle at target speed
    i2c_master_bus_rm_device(_i2c_master_dev);
    _i2c_master_dev = nullptr;

    dev_config.scl_speed_hz = _requestedSpeed;
    ret                     = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "Failed to switch device to %lu Hz: %s", (unsigned long)_requestedSpeed,
                    esp_err_to_name(ret));
        i2c_del_master_bus(_i2c_master_bus);
        _i2c_master_bus = nullptr;
        _initialized    = false;
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 步骤7：刷新快照并完成初始化
    // Step 7: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    _initialized = true;
    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, (unsigned long)_requestedSpeed);
    return M5PM1_OK;

#else   // !M5PM1_HAS_I2C_MASTER — 传统 driver/i2c.h 路径 / Legacy driver/i2c.h path
    _i2cDriverType = M5PM1_I2C_DRIVER_LEGACY;

    // 步骤2：安装传统 I2C 驱动（100KHz 起始）
    // Step 2: Install legacy I2C driver starting at 100KHz
    i2c_config_t conf = {
        .mode          = I2C_MODE_MASTER,
        .sda_io_num    = sda,
        .scl_io_num    = scl,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master        = {.clk_speed = M5PM1_I2C_FREQ_100K},
        .clk_flags     = 0,
    };

    esp_err_t ret = i2c_param_config(port, &conf);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return M5PM1_ERR_I2C_CONFIG;
    }

    ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备
    // Try to wake up the device
    M5PM1_I2C_LEGACY_SEND_WAKE(port, _addr);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待800ms重试一次）
    // Step 3: Verify device communication (retry once after 800ms if failed)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        M5PM1_I2C_LEGACY_SEND_WAKE(port, _addr);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");

            // 以400K重新安装驱动
            // Reinstall driver at 400K
            i2c_driver_delete(port);
            conf.master.clk_speed = M5PM1_I2C_FREQ_400K;
            ret                   = i2c_param_config(port, &conf);
            if (ret != ESP_OK) {
                M5PM1_LOG_E(TAG_I2C, "i2c_param_config 400K failed: %s", esp_err_to_name(ret));
                return M5PM1_ERR_I2C_CONFIG;
            }

            ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
            if (ret != ESP_OK) {
                M5PM1_LOG_E(TAG_I2C, "i2c_driver_install 400K failed: %s", esp_err_to_name(ret));
                return M5PM1_ERR_I2C_CONFIG;
            }

            // 尝试唤醒设备
            // Try to wake up the device
            M5PM1_I2C_LEGACY_SEND_WAKE(port, _addr);
            M5PM1_DELAY_MS(10);

            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_driver_delete(port);
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }
    _initialized = true;

    // 步骤5：配置设备I2C参数（关闭睡眠 + 目标频率）
    // Step 5: Configure device I2C (sleep off + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤6：若目标频率与100K不同，重新安装驱动至目标频率
    // Step 6: Reinstall driver at target speed if different from 100K
    if (_requestedSpeed != M5PM1_I2C_FREQ_100K) {
        i2c_driver_delete(port);
        conf.master.clk_speed = _requestedSpeed;
        ret                   = i2c_param_config(port, &conf);
        if (ret != ESP_OK) {
            M5PM1_LOG_E(TAG_I2C, "i2c_param_config target speed failed: %s", esp_err_to_name(ret));
            _initialized = false;
            return M5PM1_ERR_I2C_CONFIG;
        }

        ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
        if (ret != ESP_OK) {
            M5PM1_LOG_E(TAG_I2C, "i2c_driver_install target speed failed: %s", esp_err_to_name(ret));
            _initialized = false;
            return M5PM1_ERR_I2C_CONFIG;
        }
    }

    // 步骤7：刷新快照并完成初始化
    // Step 7: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    _initialized = true;
    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, (unsigned long)_requestedSpeed);
    return M5PM1_OK;
#endif  // M5PM1_HAS_I2C_MASTER
}

#if M5PM1_HAS_I2C_MASTER
m5pm1_err_t M5PM1::begin(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t speed)
{
    _addr           = addr;
    _busExternal    = true;
    _i2cDriverType  = M5PM1_I2C_DRIVER_MASTER;
    _i2c_master_bus = bus;

    // 步骤1：校验用户频率并记录
    // Step 1: Validate requested speed and store it
    if (!_isValidI2cFrequency(speed)) {
        M5PM1_LOG_W(TAG_I2C,
                    "Invalid I2C frequency: %lu Hz. PM1 only supports 100KHz or 400KHz. Falling back to 100KHz.",
                    (unsigned long)speed);
        _requestedSpeed = M5PM1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    // 步骤3：以100KHz创建设备句柄
    // Step 3: Create device handle at 100KHz
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = _addr,
        .scl_speed_hz    = M5PM1_I2C_FREQ_100K,
        .scl_wait_us     = 0,
        .flags =
            {
                .disable_ack_check = false,
            },
    };

    esp_err_t ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备
    // Try to wake up the device
    M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待800ms重试一次）
    // Step 3: Verify device communication (retry once after 800ms if failed)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        // 重试前再次发送唤醒信号
        // Send wake signal again before retry
        M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");

            // 删除当前100K设备句柄
            // Remove current 100K device handle
            i2c_master_bus_rm_device(_i2c_master_dev);
            _i2c_master_dev = nullptr;

            // 以400K重新创建设备句柄
            // Recreate device handle at 400K
            dev_config.scl_speed_hz = M5PM1_I2C_FREQ_400K;
            ret                     = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
            if (ret != ESP_OK) {
                M5PM1_LOG_E(TAG_I2C, "Failed to add I2C device at 400KHz: %s", esp_err_to_name(ret));
                return M5PM1_ERR_I2C_CONFIG;
            }

            // 尝试唤醒设备
            // Try to wake up the device
            M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
            M5PM1_DELAY_MS(10);

            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_master_bus_rm_device(_i2c_master_dev);
                _i2c_master_dev = nullptr;
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }
    _initialized = true;

    // 步骤4：配置设备I2C参数（关闭睡眠 + 目标频率）
    // Step 4: Configure device I2C (sleep off + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤5：重建设备句柄到目标频率
    // Step 5: Recreate device handle at target speed
    i2c_master_bus_rm_device(_i2c_master_dev);
    _i2c_master_dev = nullptr;

    dev_config.scl_speed_hz = _requestedSpeed;
    ret                     = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
    if (ret != ESP_OK) {
        M5PM1_LOG_E(TAG_I2C, "Failed to switch device to %lu Hz: %s", (unsigned long)_requestedSpeed,
                    esp_err_to_name(ret));
        _initialized = false;
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 步骤6：刷新快照并完成初始化
    // Step 6: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    _initialized = true;
    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, (unsigned long)_requestedSpeed);
    return M5PM1_OK;
}
#endif  // M5PM1_HAS_I2C_MASTER

#if M5PM1_HAS_I2C_BUS
m5pm1_err_t M5PM1::begin(i2c_bus_handle_t bus, uint8_t addr, uint32_t speed)
{
    _addr          = addr;
    _busExternal   = true;
    _i2cDriverType = M5PM1_I2C_DRIVER_BUS;
    _i2c_bus       = bus;

    // 步骤1：校验用户频率并记录
    // Step 1: Validate requested speed and store it
    if (!_isValidI2cFrequency(speed)) {
        M5PM1_LOG_W(TAG_I2C,
                    "Invalid I2C frequency: %lu Hz. PM1 only supports 100KHz or 400KHz. Falling back to 100KHz.",
                    (unsigned long)speed);
        _requestedSpeed = M5PM1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    // 步骤2：以100KHz创建设备句柄
    // Step 2: Create device handle at 100KHz
    _i2c_device = i2c_bus_device_create(bus, addr, M5PM1_I2C_FREQ_100K);
    if (_i2c_device == nullptr) {
        M5PM1_LOG_E(TAG_I2C, "Failed to create I2C device");
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备
    // Try to wake up the device
    M5PM1_I2C_BUS_SEND_WAKE(_i2c_device, M5PM1_REG_HW_REV);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待800ms重试一次）
    // Step 3: Verify device communication (retry once after 800ms if failed)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        // 重试前再次发送唤醒信号
        // Send wake signal again before retry
        M5PM1_I2C_BUS_SEND_WAKE(_i2c_device, M5PM1_REG_HW_REV);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");

            // 删除当前100K设备句柄
            // Remove current 100K device handle
            i2c_bus_device_delete(&_i2c_device);
            _i2c_device = nullptr;

            // 以400K重新创建设备句柄
            // Recreate device handle at 400K
            _i2c_device = i2c_bus_device_create(bus, addr, M5PM1_I2C_FREQ_400K);
            if (_i2c_device == nullptr) {
                M5PM1_LOG_E(TAG_I2C, "Failed to create I2C device at 400KHz");
                return M5PM1_ERR_I2C_CONFIG;
            }

            // 尝试唤醒设备
            // Try to wake up the device
            M5PM1_I2C_BUS_SEND_WAKE(_i2c_device, M5PM1_REG_HW_REV);
            M5PM1_DELAY_MS(10);

            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_bus_device_delete(&_i2c_device);
                _i2c_device = nullptr;
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }
    _initialized = true;

    // 步骤4：配置设备I2C参数（关闭睡眠 + 目标频率）
    // Step 4: Configure device I2C (sleep off + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤5：重建设备句柄到目标频率
    // Step 5: Recreate device handle at target speed
    i2c_bus_device_delete(&_i2c_device);
    _i2c_device = nullptr;

    _i2c_device = i2c_bus_device_create(bus, addr, _requestedSpeed);
    if (_i2c_device == nullptr) {
        M5PM1_LOG_E(TAG_I2C, "Failed to switch device to %lu Hz", (unsigned long)_requestedSpeed);
        _initialized = false;
        return M5PM1_ERR_I2C_CONFIG;
    }

    // 步骤6：刷新快照并完成初始化
    // Step 6: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    _initialized = true;
    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, (unsigned long)_requestedSpeed);
    return M5PM1_OK;
}
#endif  // M5PM1_HAS_I2C_BUS

#if M5PM1_HAS_M5UNIFIED_I2C
m5pm1_err_t M5PM1::begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed)
{
    if (i2c == nullptr || !i2c->isEnabled()) {
        M5PM1_LOG_E(TAG_I2C, "M5Unified I2C_Class is null or not initialized");
        return M5PM1_ERR_I2C_CONFIG;
    }

    _addr          = addr;
    _m5_i2c        = i2c;
    _busExternal   = true;
    _i2cDriverType = M5PM1_I2C_DRIVER_M5UNIFIED;

    // 步骤1：校验用户频率并记录
    // Step 1: Validate requested speed
    if (!_isValidI2cFrequency(speed)) {
        M5PM1_LOG_W(TAG_I2C, "Invalid I2C frequency: %lu Hz. Falling back to 100KHz.", (unsigned long)speed);
        _requestedSpeed = M5PM1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    // 步骤2：以 100KHz 发送唤醒信号
    // Step 2: Send wake signal at 100KHz
    _commFreq = M5PM1_I2C_FREQ_100K;
    M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
    M5PM1_DELAY_MS(10);

    // 步骤3：验证设备通信（失败则等待 800ms 重试一次）
    // Step 3: Verify device communication (retry once after 800ms)
    if (!_initDevice()) {
        M5PM1_LOG_W(TAG_I2C, "Device init failed, retrying after 800ms...");
        M5PM1_DELAY_MS(800);
        M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
        M5PM1_DELAY_MS(10);
        if (!_initDevice()) {
            // 100K 再次失败，尝试 400K
            // 100K failed again, try 400K
            M5PM1_LOG_W(TAG_I2C, "Device init failed at 100KHz (retry), trying 400KHz...");
            _commFreq = M5PM1_I2C_FREQ_400K;
            M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
            M5PM1_DELAY_MS(10);
            if (!_initDevice()) {
                M5PM1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                _i2cDriverType = M5PM1_I2C_DRIVER_NONE;
                _m5_i2c        = nullptr;
                return M5PM1_ERR_I2C_COMM;
            }
        }
    }
    _initialized = true;

    // 步骤4：配置设备 I2C 参数（关闭睡眠 + 目标频率）
    // Step 4: Configure device I2C (disable sleep + target speed)
    m5pm1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5PM1_I2C_FREQ_400K) ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    if (setI2cConfig(0, targetSpeed) != M5PM1_OK) {
        M5PM1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤5：切换通信频率到目标亟（M5Unified 无需重装驱动，直接更新 _commFreq）
    // Step 5: Switch to target frequency (no driver reinstall needed; update _commFreq only)
    _commFreq = _requestedSpeed;

    // 步骤6：刷新快照并完成初始化
    // Step 6: Refresh snapshot and finish initialization
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (!_snapshotAll()) {
        M5PM1_LOG_D(TAG_I2C, "Snapshot failed, clearing all cached state");
        _clearAll();
    } else {
        M5PM1_LOG_D(TAG_I2C, "Snapshot completed successfully");
    }

    _initialized = true;
    M5PM1_LOG_I(TAG_I2C, "M5PM1 initialized via M5Unified I2C at address 0x%02X (%lu Hz)", _addr,
                (unsigned long)_requestedSpeed);
    return M5PM1_OK;
}
#endif  // M5PM1_HAS_M5UNIFIED_I2C

#endif  // ARDUINO

// ============================
// 内部辅助函数
// Internal Helper Functions
// ============================

bool M5PM1::_initDevice()
{
    // Verify device ID
    uint8_t id    = 0;
    uint8_t model = 0;
    uint8_t hw    = 0;
    uint8_t sw    = 0;
    if (!_readReg(M5PM1_REG_DEVICE_ID, &id) || !_readReg(M5PM1_REG_DEVICE_MODEL, &model) ||
        !_readReg(M5PM1_REG_HW_REV, &hw) || !_readReg(M5PM1_REG_SW_REV, &sw)) {
        return false;
    }
    if (sw >= 'A' && sw <= 'Z') {
        M5PM1_LOG_I(TAG_SYS, "Device: ID=0x%02X MODEL=0x%02X HW=0x%02X SW=0x%02X(%c)", id, model, hw, sw, sw);
    } else {
        M5PM1_LOG_I(TAG_SYS, "Device: ID=0x%02X MODEL=0x%02X HW=0x%02X SW=0x%02X", id, model, hw, sw);
    }

    _clearAll();

    if (!_snapshotI2cConfig()) {
        _clearI2cConfig();
    }

    return true;
}

bool M5PM1::_isValidPin(uint8_t pin)
{
    return pin < M5PM1_MAX_GPIO_PINS;
}

bool M5PM1::_isAdcPin(uint8_t pin)
{
    return (pin == M5PM1_GPIO_NUM_1 || pin == M5PM1_GPIO_NUM_2);
}

bool M5PM1::_isPwmPin(uint8_t pin)
{
    return (pin == M5PM1_GPIO_NUM_3 || pin == M5PM1_GPIO_NUM_4);
}

bool M5PM1::_isNeoPin(uint8_t pin)
{
    return (pin == M5PM1_GPIO_NUM_0);
}

bool M5PM1::_hasActiveAdc(uint8_t pin)
{
    if (!_adcStateValid) return false;

    if (pin == M5PM1_GPIO_NUM_1) {
        return _adcState.channel == M5PM1_ADC_CH_1;
    }
    if (pin == M5PM1_GPIO_NUM_2) {
        return _adcState.channel == M5PM1_ADC_CH_2;
    }
    return false;
}

bool M5PM1::_hasActivePwm(uint8_t pin)
{
    if (!_pwmStatesValid) return false;

    if (pin == M5PM1_GPIO_NUM_3) {
        return _pwmStates[M5PM1_PWM_CH_0].enabled;
    }
    if (pin == M5PM1_GPIO_NUM_4) {
        return _pwmStates[M5PM1_PWM_CH_1].enabled;
    }
    return false;
}

bool M5PM1::_hasActiveIrq(uint8_t pin)
{
    if (!_cacheValid) return false;
    return _pinStatus[pin].func == M5PM1_GPIO_FUNC_IRQ;
}

bool M5PM1::_hasActiveWake(uint8_t pin)
{
    if (!_cacheValid) return false;
    return _pinStatus[pin].func == M5PM1_GPIO_FUNC_WAKE || _pinStatus[pin].wake_en;
}

bool M5PM1::_hasActiveNeo(uint8_t pin)
{
    if (!_cacheValid) return false;
    return _isNeoPin(pin) && _pinStatus[pin].func == M5PM1_GPIO_FUNC_OTHER;
}

bool M5PM1::_isValidI2cFrequency(uint32_t speed)
{
    return (speed == M5PM1_I2C_FREQ_100K || speed == M5PM1_I2C_FREQ_400K);
}

void M5PM1::_checkAutoWake()
{
    if (!_autoWakeEnabled || !_i2cConfigValid || _i2cConfig.sleepTime == 0) return;

    uint32_t now     = M5PM1_GET_TIME_MS();
    uint32_t elapsed = now - _lastCommTime;

    // If more than 1 second since last communication, send wake signal
    if (elapsed >= 1000) {
        sendWakeSignal();
        M5PM1_DELAY_MS(10);
    }
}

bool M5PM1::_snapshotI2cConfig()
{
    uint8_t cfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &cfg)) {
        return false;
    }

    _i2cConfig.sleepTime = cfg & M5PM1_I2C_CFG_SLEEP_MASK;
    _i2cConfig.speed400k = (cfg & M5PM1_I2C_CFG_SPEED_400K) != 0;
    _i2cConfigValid      = true;
    _i2cSleepTime        = _i2cConfig.sleepTime;
    return true;
}

void M5PM1::_clearI2cConfig()
{
    _i2cConfig.sleepTime = 0;
    _i2cConfig.speed400k = false;
    _i2cConfigValid      = false;
    _i2cSleepTime        = 0;
}

bool M5PM1::_snapshotNeoConfig()
{
    uint8_t cfg = 0;
    if (!_readReg(M5PM1_REG_NEO_CFG, &cfg)) {
        _neoConfigValid = false;
        return false;
    }

    _neoCfg         = cfg & M5PM1_NEO_CFG_COUNT_MASK;
    _neoConfigValid = true;
    return true;
}

void M5PM1::_clearNeoConfig()
{
    _neoCfg         = 0;
    _neoConfigValid = false;
}

bool M5PM1::_snapshotAw8737a()
{
    uint8_t regValue = 0;
    if (!_readReg(M5PM1_REG_AW8737A_PULSE, &regValue)) {
        M5PM1_LOG_E(TAG_AMP, "Failed to read AW8737A pulse register");
        _aw8737aStateValid = false;
        return false;
    }

    // 解析寄存器值（不含 REFRESH 位）
    // Parse register value (without REFRESH bit)
    _aw8737aRegValue   = regValue & 0x7F;
    _aw8737aPin        = (m5pm1_gpio_num_t)(regValue & 0x1F);
    _aw8737aPulseNum   = (m5pm1_aw8737a_pulse_t)((regValue >> 5) & 0x03);
    _aw8737aStateValid = true;

    return true;
}

void M5PM1::_clearAw8737a()
{
    _aw8737aConfigured = false;
    _aw8737aPin        = M5PM1_GPIO_NUM_0;
    _aw8737aPulseNum   = M5PM1_AW8737A_PULSE_0;
    _aw8737aRegValue   = 0;
    _aw8737aStateValid = false;
}

// ============================
// 缓存管理函数
// Cache Management Functions
// ============================

void M5PM1::_clearPinStates()
{
    for (int i = 0; i < M5PM1_MAX_GPIO_PINS; i++) {
        _pinStatus[i].func       = M5PM1_GPIO_FUNC_GPIO;
        _pinStatus[i].mode       = M5PM1_GPIO_MODE_INPUT;
        _pinStatus[i].output     = 0;
        _pinStatus[i].pull       = M5PM1_GPIO_PULL_NONE;
        _pinStatus[i].wake_en    = false;
        _pinStatus[i].wake_edge  = M5PM1_GPIO_WAKE_FALLING;
        _pinStatus[i].drive      = M5PM1_GPIO_DRIVE_PUSHPULL;
        _pinStatus[i].power_hold = false;
    }
    _cacheValid = false;
}

void M5PM1::_clearPwmStates()
{
    memset(_pwmStates, 0, sizeof(_pwmStates));
    _pwmFrequency   = 0;
    _pwmStatesValid = false;
}

void M5PM1::_clearAdcState()
{
    _adcState.channel   = 0;
    _adcState.busy      = false;
    _adcState.lastValue = 0;
    _adcStateValid      = false;
}

void M5PM1::_clearPowerConfig()
{
    _powerCfg         = 0;
    _holdCfg          = 0;
    _powerConfigValid = false;
}

void M5PM1::_clearButtonConfig()
{
    _btnCfg1        = 0;
    _btnCfg2        = 0;
    _btnConfigValid = false;
    _btnFlagCache   = false;
}

void M5PM1::_clearIrqMasks()
{
    _irqMask1     = 0;
    _irqMask2     = 0;
    _irqMask3     = 0;
    _irqMaskValid = false;
}

void M5PM1::_clearIrqStatus()
{
    _irqStatus1     = 0;
    _irqStatus2     = 0;
    _irqStatus3     = 0;
    _irqStatusValid = false;
}

void M5PM1::_clearAll()
{
    _clearPinStates();
    _clearPwmStates();
    _clearAdcState();
    _clearPowerConfig();
    _clearButtonConfig();
    _clearIrqMasks();
    _clearIrqStatus();
    _clearI2cConfig();
    _clearNeoConfig();
}

bool M5PM1::_snapshotPinStates()
{
    uint8_t mode    = 0;
    uint8_t out     = 0;
    uint8_t drv     = 0;
    uint8_t pupd0   = 0;
    uint8_t pupd1   = 0;
    uint8_t func0   = 0;
    uint8_t func1   = 0;
    uint8_t wakeEn  = 0;
    uint8_t wakeCfg = 0;
    uint8_t holdCfg = 0;

    if (!_readReg(M5PM1_REG_GPIO_MODE, &mode)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_OUT, &out)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_DRV, &drv)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_PUPD0, &pupd0)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_PUPD1, &pupd1)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_FUNC0, &func0)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_FUNC1, &func1)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_WAKE_EN, &wakeEn)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_GPIO_WAKE_CFG, &wakeCfg)) {
        _cacheValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_HOLD_CFG, &holdCfg)) {
        _cacheValid = false;
        return false;
    }

    for (int pin = 0; pin < M5PM1_MAX_GPIO_PINS; pin++) {
        _pinStatus[pin].mode       = (mode & (1 << pin)) ? M5PM1_GPIO_MODE_OUTPUT : M5PM1_GPIO_MODE_INPUT;
        _pinStatus[pin].output     = (out >> pin) & 0x01;
        _pinStatus[pin].drive      = (drv & (1 << pin)) ? M5PM1_GPIO_DRIVE_OPENDRAIN : M5PM1_GPIO_DRIVE_PUSHPULL;
        _pinStatus[pin].wake_en    = (wakeEn & (1 << pin)) != 0;
        _pinStatus[pin].wake_edge  = (wakeCfg & (1 << pin)) ? M5PM1_GPIO_WAKE_RISING : M5PM1_GPIO_WAKE_FALLING;
        _pinStatus[pin].power_hold = (holdCfg & (1 << pin)) != 0;

        if (pin < 4) {
            _pinStatus[pin].func = (m5pm1_gpio_func_t)((func0 >> (pin * 2)) & 0x03);
            _pinStatus[pin].pull = (m5pm1_gpio_pull_t)((pupd0 >> (pin * 2)) & 0x03);
        } else {
            _pinStatus[pin].func = (m5pm1_gpio_func_t)((func1 >> 0) & 0x03);
            _pinStatus[pin].pull = (m5pm1_gpio_pull_t)((pupd1 >> 0) & 0x03);
        }
    }

    _cacheValid = true;
    return true;
}

bool M5PM1::_snapshotPwmStates()
{
    if (!_readReg16(M5PM1_REG_PWM_FREQ_L, &_pwmFrequency)) {
        _pwmStatesValid = false;
        return false;
    }

    for (uint8_t ch = 0; ch < M5PM1_MAX_PWM_CHANNELS; ch++) {
        uint8_t regL = (ch == 0) ? M5PM1_REG_PWM0_L : M5PM1_REG_PWM1_L;
        uint8_t regH = (ch == 0) ? M5PM1_REG_PWM0_HC : M5PM1_REG_PWM1_HC;
        uint8_t low  = 0;
        uint8_t high = 0;

        if (!_readReg(regL, &low)) {
            _pwmStatesValid = false;
            return false;
        }
        if (!_readReg(regH, &high)) {
            _pwmStatesValid = false;
            return false;
        }

        _pwmStates[ch].duty12   = (uint16_t)(low | ((high & 0x0F) << 8));
        _pwmStates[ch].enabled  = (high & 0x10) != 0;
        _pwmStates[ch].polarity = (high & 0x20) != 0;
    }

    _pwmStatesValid = true;
    return true;
}

bool M5PM1::_snapshotAdcState()
{
    uint8_t ctrl = 0;
    if (!_readReg(M5PM1_REG_ADC_CTRL, &ctrl)) {
        _adcStateValid = false;
        return false;
    }

    _adcState.channel = (ctrl >> 1) & 0x07;
    _adcState.busy    = (ctrl & 0x01) != 0;
    if (!_readReg16(M5PM1_REG_ADC_RES_L, &_adcState.lastValue)) {
        _adcStateValid = false;
        return false;
    }

    _adcStateValid = true;
    return true;
}

bool M5PM1::_snapshotPowerConfig()
{
    if (!_readReg(M5PM1_REG_PWR_CFG, &_powerCfg)) {
        _powerConfigValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_HOLD_CFG, &_holdCfg)) {
        _powerConfigValid = false;
        return false;
    }

    _powerConfigValid = true;
    return true;
}

bool M5PM1::_snapshotButtonConfig()
{
    if (!_readReg(M5PM1_REG_BTN_CFG_1, &_btnCfg1)) {
        _btnConfigValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_BTN_CFG_2, &_btnCfg2)) {
        _btnConfigValid = false;
        return false;
    }

    _btnConfigValid = true;
    return true;
}

bool M5PM1::_snapshotIrqMasks()
{
    if (!_readReg(M5PM1_REG_IRQ_MASK1, &_irqMask1)) {
        _irqMaskValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_IRQ_MASK2, &_irqMask2)) {
        _irqMaskValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_IRQ_MASK3, &_irqMask3)) {
        _irqMaskValid = false;
        return false;
    }

    _irqMaskValid = true;
    return true;
}

bool M5PM1::_snapshotIrqStatus()
{
    if (!_readReg(M5PM1_REG_IRQ_STATUS1, &_irqStatus1)) {
        _irqStatusValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_IRQ_STATUS2, &_irqStatus2)) {
        _irqStatusValid = false;
        return false;
    }
    if (!_readReg(M5PM1_REG_IRQ_STATUS3, &_irqStatus3)) {
        _irqStatusValid = false;
        return false;
    }

    _irqStatusValid = true;
    return true;
}

bool M5PM1::_snapshotAll()
{
    bool gpio      = _snapshotPinStates();
    bool pwm       = _snapshotPwmStates();
    bool adc       = _snapshotAdcState();
    bool power     = _snapshotPowerConfig();
    bool button    = _snapshotButtonConfig();
    bool irqMask   = _snapshotIrqMasks();
    bool irqStatus = _snapshotIrqStatus();
    bool i2c       = _snapshotI2cConfig();
    bool neo       = _snapshotNeoConfig();

    if (!i2c) {
        _clearI2cConfig();
    }
    if (!neo) {
        _clearNeoConfig();
    }

    return gpio && pwm && adc && power && button && irqMask && irqStatus && i2c && neo;
}

void M5PM1::_autoSnapshotUpdate(uint16_t domains)
{
    if (!_autoSnapshot) return;

    if (domains & M5PM1_SNAPSHOT_DOMAIN_GPIO) {
        _snapshotPinStates();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_PWM) {
        _snapshotPwmStates();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_ADC) {
        _snapshotAdcState();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_POWER) {
        _snapshotPowerConfig();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_BUTTON) {
        _snapshotButtonConfig();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK) {
        _snapshotIrqMasks();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS) {
        _snapshotIrqStatus();
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_I2C) {
        if (!_snapshotI2cConfig()) {
            _clearI2cConfig();
        }
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_NEO) {
        if (!_snapshotNeoConfig()) {
            _clearNeoConfig();
        }
    }
    if (domains & M5PM1_SNAPSHOT_DOMAIN_AW8737A) {
        if (!_snapshotAw8737a()) {
            _clearAw8737a();
        }
    }
}

void M5PM1::_initPinCache()
{
    _clearPinStates();
}

bool M5PM1::_readBtnStatus(uint8_t* rawValue)
{
    // 读取 BTN_STATUS 寄存器并更新内部 BTN_FLAG 缓存
    // Read BTN_STATUS register and update internal BTN_FLAG cache
    uint8_t val;
    if (!_readReg(M5PM1_REG_BTN_STATUS, &val)) {
        return false;
    }
    // 如果 BTN_FLAG 位被置位，累积到缓存
    // If BTN_FLAG bit is set, accumulate to cache
    if (val & 0x80) {
        _btnFlagCache = true;
    }
    if (rawValue) {
        *rawValue = val;
    }
    return true;
}

bool M5PM1::_writeReg(uint8_t reg, uint8_t value)
{
    _checkAutoWake();
    bool success = false;
    for (int attempt = 0; attempt < M5PM1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            success = M5PM1_M5UNIFIED_WRITE_BYTE(_m5_i2c, _addr, reg, value, _commFreq);
        } else
#endif
        {
            success = M5PM1_I2C_ARDUINO_WRITE_BYTE(_wire, _addr, reg, value);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER:
                success = M5PM1_I2C_MASTER_WRITE_BYTE(_i2c_master_dev, reg, value) == ESP_OK;
                break;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                success = M5PM1_I2C_BUS_WRITE_BYTE(_i2c_device, reg, value) == ESP_OK;
                break;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY:
                success = M5PM1_I2C_LEGACY_WRITE_BYTE(_port, _addr, reg, value) == ESP_OK;
                break;
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                success = M5PM1_M5UNIFIED_WRITE_BYTE(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                success = false;
                break;
        }
#endif
        if (success) {
            break;
        }
        if (attempt + 1 < M5PM1_I2C_RETRY_COUNT) {
            M5PM1_DELAY_MS(M5PM1_I2C_RETRY_DELAY_MS);
        }
    }
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (success) {
        M5PM1_LOG_D(TAG_I2C, "Write Reg[0x%02X] <- 0x%02X", reg, value);
    } else {
        M5PM1_LOG_E(TAG_I2C, "Write Reg[0x%02X] failed", reg);
    }
    return success;
}

bool M5PM1::_writeReg16(uint8_t reg, uint16_t value)
{
    _checkAutoWake();
    bool success = false;
    for (int attempt = 0; attempt < M5PM1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            success = M5PM1_M5UNIFIED_WRITE_REG16(_m5_i2c, _addr, reg, value, _commFreq);
        } else
#endif
        {
            success = M5PM1_I2C_ARDUINO_WRITE_REG16(_wire, _addr, reg, value);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER:
                success = M5PM1_I2C_MASTER_WRITE_REG16(_i2c_master_dev, reg, value) == ESP_OK;
                break;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                success = M5PM1_I2C_BUS_WRITE_REG16(_i2c_device, reg, value) == ESP_OK;
                break;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY:
                success = M5PM1_I2C_LEGACY_WRITE_REG16(_port, _addr, reg, value) == ESP_OK;
                break;
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                success = M5PM1_M5UNIFIED_WRITE_REG16(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                success = false;
                break;
        }
#endif
        if (success) {
            break;
        }
        if (attempt + 1 < M5PM1_I2C_RETRY_COUNT) {
            M5PM1_DELAY_MS(M5PM1_I2C_RETRY_DELAY_MS);
        }
    }
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (success) {
        M5PM1_LOG_D(TAG_I2C, "Write16 Reg[0x%02X] <- 0x%04X", reg, value);
    } else {
        M5PM1_LOG_E(TAG_I2C, "Write16 Reg[0x%02X] failed", reg);
    }
    return success;
}

bool M5PM1::_readReg(uint8_t reg, uint8_t* value)
{
    _checkAutoWake();
    bool success = false;
    for (int attempt = 0; attempt < M5PM1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            success = M5PM1_M5UNIFIED_READ_BYTE(_m5_i2c, _addr, reg, value, _commFreq);
        } else
#endif
        {
            success = M5PM1_I2C_ARDUINO_READ_BYTE(_wire, _addr, reg, value);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER:
                success = M5PM1_I2C_MASTER_READ_BYTE(_i2c_master_dev, reg, value) == ESP_OK;
                break;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                success = M5PM1_I2C_BUS_READ_BYTE(_i2c_device, reg, value) == ESP_OK;
                break;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY:
                success = M5PM1_I2C_LEGACY_READ_BYTE(_port, _addr, reg, value) == ESP_OK;
                break;
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                success = M5PM1_M5UNIFIED_READ_BYTE(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                success = false;
                break;
        }
#endif
        if (success) {
            break;
        }
        if (attempt + 1 < M5PM1_I2C_RETRY_COUNT) {
            M5PM1_DELAY_MS(M5PM1_I2C_RETRY_DELAY_MS);
        }
    }
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (success) {
        M5PM1_LOG_D(TAG_I2C, "Read  Reg[0x%02X] -> 0x%02X", reg, *value);
    } else {
        M5PM1_LOG_E(TAG_I2C, "Read  Reg[0x%02X] failed", reg);
    }
    return success;
}

bool M5PM1::_readReg16(uint8_t reg, uint16_t* value)
{
    _checkAutoWake();
    bool success = false;
    for (int attempt = 0; attempt < M5PM1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            success = M5PM1_M5UNIFIED_READ_REG16(_m5_i2c, _addr, reg, value, _commFreq);
        } else
#endif
        {
            success = M5PM1_I2C_ARDUINO_READ_REG16(_wire, _addr, reg, value);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER:
                success = M5PM1_I2C_MASTER_READ_REG16(_i2c_master_dev, reg, value) == ESP_OK;
                break;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                success = M5PM1_I2C_BUS_READ_REG16(_i2c_device, reg, value) == ESP_OK;
                break;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY:
                success = M5PM1_I2C_LEGACY_READ_REG16(_port, _addr, reg, value) == ESP_OK;
                break;
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                success = M5PM1_M5UNIFIED_READ_REG16(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                success = false;
                break;
        }
#endif
        if (success) {
            break;
        }
        if (attempt + 1 < M5PM1_I2C_RETRY_COUNT) {
            M5PM1_DELAY_MS(M5PM1_I2C_RETRY_DELAY_MS);
        }
    }
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (success) {
        M5PM1_LOG_D(TAG_I2C, "Read16 Reg[0x%02X] -> 0x%04X", reg, *value);
    } else {
        M5PM1_LOG_E(TAG_I2C, "Read16 Reg[0x%02X] failed", reg);
    }
    return success;
}

bool M5PM1::_writeBytes(uint8_t reg, const uint8_t* data, uint8_t len)
{
    _checkAutoWake();
    bool success = false;
    for (int attempt = 0; attempt < M5PM1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            success = M5PM1_M5UNIFIED_WRITE_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq);
        } else
#endif
        {
            success = M5PM1_I2C_ARDUINO_WRITE_BYTES(_wire, _addr, reg, len, data);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER:
                success = M5PM1_I2C_MASTER_WRITE_BYTES(_i2c_master_dev, reg, len, data) == ESP_OK;
                break;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                success = M5PM1_I2C_BUS_WRITE_BYTES(_i2c_device, reg, len, data) == ESP_OK;
                break;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY:
                success = M5PM1_I2C_LEGACY_WRITE_BYTES(_port, _addr, reg, len, data) == ESP_OK;
                break;
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                success = M5PM1_M5UNIFIED_WRITE_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq);
                break;
#endif
            default:
                success = false;
                break;
        }
#endif
        if (success) {
            break;
        }
        if (attempt + 1 < M5PM1_I2C_RETRY_COUNT) {
            M5PM1_DELAY_MS(M5PM1_I2C_RETRY_DELAY_MS);
        }
    }
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (success) {
        M5PM1_LOG_V(TAG_I2C, "WriteBytes Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len, len > 0 ? data[0] : 0,
                    len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
    } else {
        M5PM1_LOG_E(TAG_I2C, "WriteBytes Reg[0x%02X] len=%d failed", reg, len);
    }
    return success;
}

bool M5PM1::_readBytes(uint8_t reg, uint8_t* data, uint8_t len)
{
    _checkAutoWake();
    bool success = false;
    for (int attempt = 0; attempt < M5PM1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            success = M5PM1_M5UNIFIED_READ_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq);
        } else
#endif
        {
            success = M5PM1_I2C_ARDUINO_READ_BYTES(_wire, _addr, reg, len, data);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER:
                success = M5PM1_I2C_MASTER_READ_BYTES(_i2c_master_dev, reg, len, data) == ESP_OK;
                break;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                success = M5PM1_I2C_BUS_READ_BYTES(_i2c_device, reg, len, data) == ESP_OK;
                break;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY:
                success = M5PM1_I2C_LEGACY_READ_BYTES(_port, _addr, reg, len, data) == ESP_OK;
                break;
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                success = M5PM1_M5UNIFIED_READ_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq);
                break;
#endif
            default:
                success = false;
                break;
        }
#endif
        if (success) {
            break;
        }
        if (attempt + 1 < M5PM1_I2C_RETRY_COUNT) {
            M5PM1_DELAY_MS(M5PM1_I2C_RETRY_DELAY_MS);
        }
    }
    _lastCommTime = M5PM1_GET_TIME_MS();
    if (success) {
        M5PM1_LOG_V(TAG_I2C, "ReadBytes  Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len, len > 0 ? data[0] : 0,
                    len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
    } else {
        M5PM1_LOG_E(TAG_I2C, "ReadBytes  Reg[0x%02X] len=%d failed", reg, len);
    }
    return success;
}

// ============================
// 设备信息
// Device Information
// ============================

m5pm1_err_t M5PM1::getDeviceId(uint8_t* id)
{
    if (id == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getDeviceId id is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_DEVICE_ID, id)) {
        M5PM1_LOG_E(TAG_SYS, "Failed to read device ID");
        return M5PM1_ERR_I2C_COMM;
    }
    M5PM1_LOG_D(TAG_SYS, "getDeviceId: 0x%02X", *id);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getDeviceModel(uint8_t* model)
{
    if (model == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getDeviceModel model is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_DEVICE_MODEL, model)) {
        M5PM1_LOG_E(TAG_SYS, "Failed to read device model");
        return M5PM1_ERR_I2C_COMM;
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getHwVersion(uint8_t* version)
{
    if (version == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getHwVersion version is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_HW_REV, version)) {
        M5PM1_LOG_E(TAG_SYS, "Failed to read hardware version");
        return M5PM1_ERR_I2C_COMM;
    }
    M5PM1_LOG_D(TAG_SYS, "getHwVersion: %u", *version);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getSwVersion(uint8_t* version)
{
    if (version == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getSwVersion version is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_SW_REV, version)) {
        M5PM1_LOG_E(TAG_SYS, "Failed to read software version");
        return M5PM1_ERR_I2C_COMM;
    }
    M5PM1_LOG_D(TAG_SYS, "getSwVersion: %u", *version);
    return M5PM1_OK;
}

// ============================
// GPIO 功能 (Arduino风格 - 带返回值)
// GPIO Functions (Arduino-style - WithRes)
// ============================

void M5PM1::pinModeWithRes(uint8_t pin, uint8_t mode, m5pm1_err_t* err)
{
    m5pm1_err_t localErr = M5PM1_OK;

    if (!_isValidPin(pin)) {
        localErr = M5PM1_ERR_INVALID_ARG;
        if (err) *err = localErr;
        return;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        localErr = M5PM1_ERR_NOT_INIT;
        if (err) *err = localErr;
        return;
    }

    m5pm1_gpio_func_t func     = M5PM1_GPIO_FUNC_GPIO;
    m5pm1_gpio_mode_t gpioMode = M5PM1_GPIO_MODE_INPUT;
    m5pm1_gpio_pull_t pull     = M5PM1_GPIO_PULL_NONE;
    m5pm1_gpio_drive_t drive   = M5PM1_GPIO_DRIVE_PUSHPULL;
    bool setDrive              = false;

    switch (mode) {
        case INPUT:
            gpioMode = M5PM1_GPIO_MODE_INPUT;
            pull     = M5PM1_GPIO_PULL_NONE;
            break;
        case OUTPUT:
            gpioMode = M5PM1_GPIO_MODE_OUTPUT;
            pull     = M5PM1_GPIO_PULL_NONE;
            drive    = M5PM1_GPIO_DRIVE_PUSHPULL;
            setDrive = true;
            break;
        case PULLUP:
        case INPUT_PULLUP:
            gpioMode = M5PM1_GPIO_MODE_INPUT;
            pull     = M5PM1_GPIO_PULL_UP;
            break;
        case PULLDOWN:
        case INPUT_PULLDOWN:
            gpioMode = M5PM1_GPIO_MODE_INPUT;
            pull     = M5PM1_GPIO_PULL_DOWN;
            break;
        case OPEN_DRAIN:
        case OUTPUT_OPEN_DRAIN:
            gpioMode = M5PM1_GPIO_MODE_OUTPUT;
            pull     = M5PM1_GPIO_PULL_NONE;
            drive    = M5PM1_GPIO_DRIVE_OPENDRAIN;
            setDrive = true;
            break;
        case ANALOG:
            func     = M5PM1_GPIO_FUNC_OTHER;
            gpioMode = M5PM1_GPIO_MODE_INPUT;
            pull     = M5PM1_GPIO_PULL_NONE;
            break;
        case M5PM1_OTHER:
            func     = M5PM1_GPIO_FUNC_OTHER;
            gpioMode = M5PM1_GPIO_MODE_OUTPUT;
            pull     = M5PM1_GPIO_PULL_NONE;
            setDrive = true;
            break;
        default:
            M5PM1_LOG_E(TAG_GPIO, "Invalid mode: 0x%02X", mode);
            localErr = M5PM1_ERR_INVALID_ARG;
            if (err) *err = localErr;
            return;
    }

    localErr = gpioSetFunc((m5pm1_gpio_num_t)pin, func);
    if (localErr != M5PM1_OK) {
        if (err) *err = localErr;
        return;
    }

    localErr = gpioSetMode((m5pm1_gpio_num_t)pin, gpioMode);
    if (localErr != M5PM1_OK) {
        if (err) *err = localErr;
        return;
    }

    localErr = gpioSetPull((m5pm1_gpio_num_t)pin, pull);
    if (localErr != M5PM1_OK) {
        if (err) *err = localErr;
        return;
    }

    if (setDrive) {
        localErr = gpioSetDrive((m5pm1_gpio_num_t)pin, drive);
        if (localErr != M5PM1_OK) {
            if (err) *err = localErr;
            return;
        }
    }

    if (err) *err = localErr;
}

void M5PM1::digitalWriteWithRes(uint8_t pin, uint8_t value, m5pm1_err_t* err)
{
    m5pm1_err_t localErr = M5PM1_OK;

    if (!_isValidPin(pin)) {
        localErr = M5PM1_ERR_INVALID_ARG;
        if (err) *err = localErr;
        return;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        localErr = M5PM1_ERR_NOT_INIT;
        if (err) *err = localErr;
        return;
    }

    localErr = gpioSetOutput((m5pm1_gpio_num_t)pin, value);
    if (err) *err = localErr;
}

int M5PM1::digitalReadWithRes(uint8_t pin, m5pm1_err_t* err)
{
    m5pm1_err_t localErr = M5PM1_OK;

    if (!_isValidPin(pin)) {
        localErr = M5PM1_ERR_INVALID_ARG;
        if (err) *err = localErr;
        return -1;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        localErr = M5PM1_ERR_NOT_INIT;
        if (err) *err = localErr;
        return -1;
    }
    uint8_t value = 0;
    localErr      = gpioGetInput((m5pm1_gpio_num_t)pin, &value);
    if (localErr != M5PM1_OK) {
        if (err) *err = localErr;
        return -1;
    }
    if (err) *err = M5PM1_OK;
    return value;
}

// ============================
// GPIO 功能 (Arduino风格)
// GPIO Functions (Arduino-style)
// ============================

void M5PM1::pinMode(uint8_t pin, uint8_t mode)
{
    pinModeWithRes(pin, mode, nullptr);
}
void M5PM1::digitalWrite(uint8_t pin, uint8_t value)
{
    digitalWriteWithRes(pin, value, nullptr);
}
int M5PM1::digitalRead(uint8_t pin)
{
    return digitalReadWithRes(pin, nullptr);
}

// ============================
// 高级 GPIO 功能
// Advanced GPIO Functions
// ============================

m5pm1_err_t M5PM1::gpioSet(m5pm1_gpio_num_t pin, m5pm1_gpio_mode_t mode, uint8_t value, m5pm1_gpio_pull_t pull,
                           m5pm1_gpio_drive_t drive)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_GPIO, "gpioSet: pin=%d mode=%d val=%d pull=%d drv=%d", pin, mode, value, pull, drive);

    m5pm1_err_t err = gpioSetFunc(pin, M5PM1_GPIO_FUNC_GPIO);
    if (err != M5PM1_OK) return err;

    err = gpioSetMode(pin, mode);
    if (err != M5PM1_OK) return err;

    if (mode == M5PM1_GPIO_MODE_OUTPUT) {
        err = gpioSetOutput(pin, value);
        if (err != M5PM1_OK) return err;
    }

    err = gpioSetPull(pin, pull);
    if (err != M5PM1_OK) return err;

    err = gpioSetDrive(pin, drive);
    if (err != M5PM1_OK) return err;

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetFunc(m5pm1_gpio_num_t pin, m5pm1_gpio_func_t func)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regAddr = (pin < 4) ? M5PM1_REG_GPIO_FUNC0 : M5PM1_REG_GPIO_FUNC1;
    uint8_t shift   = (pin < 4) ? (pin * 2) : ((pin - 4) * 2);

    uint8_t regVal;
    if (!_readReg(regAddr, &regVal)) return M5PM1_ERR_I2C_COMM;

    regVal &= ~(0x03 << shift);
    regVal |= ((uint8_t)func << shift);

    if (!_writeReg(regAddr, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetMode(m5pm1_gpio_num_t pin, m5pm1_gpio_mode_t mode)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_GPIO, "gpioSetMode: pin=%d mode=%d", pin, mode);

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_MODE, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (mode == M5PM1_GPIO_MODE_OUTPUT) {
        regVal |= (1 << pin);
    } else {
        regVal &= ~(1 << pin);
    }

    if (!_writeReg(M5PM1_REG_GPIO_MODE, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetOutput(m5pm1_gpio_num_t pin, uint8_t value)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_GPIO, "gpioSetOutput: pin=%d val=%s", pin, value ? "High" : "Low");

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_OUT, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (value) {
        regVal |= (1 << pin);
    } else {
        regVal &= ~(1 << pin);
    }

    if (!_writeReg(M5PM1_REG_GPIO_OUT, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioGetInput(m5pm1_gpio_num_t pin, uint8_t* value)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (value == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_IN, &regVal)) return M5PM1_ERR_I2C_COMM;

    *value = (regVal >> pin) & 0x01;
    M5PM1_LOG_D(TAG_GPIO, "gpioGetInput: pin=%d -> %d", pin, *value);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetPull(m5pm1_gpio_num_t pin, m5pm1_gpio_pull_t pull)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_GPIO, "gpioSetPull: pin=%d pull=%d", pin, pull);

    uint8_t regAddr = (pin < 4) ? M5PM1_REG_GPIO_PUPD0 : M5PM1_REG_GPIO_PUPD1;
    uint8_t shift   = (pin < 4) ? (pin * 2) : ((pin - 4) * 2);

    uint8_t regVal;
    if (!_readReg(regAddr, &regVal)) return M5PM1_ERR_I2C_COMM;

    regVal &= ~(0x03 << shift);
    regVal |= ((uint8_t)pull << shift);

    if (!_writeReg(regAddr, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetDrive(m5pm1_gpio_num_t pin, m5pm1_gpio_drive_t drive)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_GPIO, "gpioSetDrive: pin=%d drive=%d", pin, drive);

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_DRV, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (drive == M5PM1_GPIO_DRIVE_OPENDRAIN) {
        regVal |= (1 << pin);
    } else {
        regVal &= ~(1 << pin);
    }

    if (!_writeReg(M5PM1_REG_GPIO_DRV, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetWakeEnable(m5pm1_gpio_num_t pin, bool enable)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_I(TAG_GPIO, "GPIO%d Wake %s", pin, enable ? "Enabled" : "Disabled");

    // WAKE 互斥校验（仅在启用时检查）
    // WAKE mutual exclusion check (only when enabling)
    if (enable) {
        // GPIO1 不支持 WAKE（与 SDA 共用中断线）
        // GPIO1 does not support WAKE (shares interrupt line with SDA)
        if (pin == M5PM1_GPIO_NUM_1) {
            M5PM1_LOG_E(TAG_GPIO, "GPIO1 does not support WAKE");
            return M5PM1_ERR_RULE_VIOLATION;
        }
        // GPIO0/GPIO2 WAKE 互斥（共用中断线）
        // GPIO0/GPIO2 WAKE mutual exclusion (share interrupt line)
        if (pin == M5PM1_GPIO_NUM_0 && _hasActiveWake(M5PM1_GPIO_NUM_2)) {
            M5PM1_LOG_E(TAG_GPIO, "GPIO0/GPIO2 WAKE conflict: GPIO2 already enabled");
            return M5PM1_ERR_RULE_VIOLATION;
        }
        if (pin == M5PM1_GPIO_NUM_2 && _hasActiveWake(M5PM1_GPIO_NUM_0)) {
            M5PM1_LOG_E(TAG_GPIO, "GPIO0/GPIO2 WAKE conflict: GPIO0 already enabled");
            return M5PM1_ERR_RULE_VIOLATION;
        }
        // GPIO3/GPIO4 WAKE 互斥（共用中断线）
        // GPIO3/GPIO4 WAKE mutual exclusion (share interrupt line)
        if (pin == M5PM1_GPIO_NUM_3 && _hasActiveWake(M5PM1_GPIO_NUM_4)) {
            M5PM1_LOG_E(TAG_GPIO, "GPIO3/GPIO4 WAKE conflict: GPIO4 already enabled");
            return M5PM1_ERR_RULE_VIOLATION;
        }
        if (pin == M5PM1_GPIO_NUM_4 && _hasActiveWake(M5PM1_GPIO_NUM_3)) {
            M5PM1_LOG_E(TAG_GPIO, "GPIO3/GPIO4 WAKE conflict: GPIO3 already enabled");
            return M5PM1_ERR_RULE_VIOLATION;
        }
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_WAKE_EN, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (enable) {
        regVal |= (1 << pin);
    } else {
        regVal &= ~(1 << pin);
    }

    if (!_writeReg(M5PM1_REG_GPIO_WAKE_EN, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioSetWakeEdge(m5pm1_gpio_num_t pin, m5pm1_gpio_wake_edge_t edge)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // GPIO1 不支持唤醒边沿配置
    // GPIO1 does not support wake edge configuration
    if (pin == M5PM1_GPIO_NUM_1) {
        M5PM1_LOG_E(TAG_GPIO, "GPIO1 does not support wake edge configuration");
        return M5PM1_ERR_INVALID_ARG;
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_WAKE_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (edge == M5PM1_GPIO_WAKE_RISING) {
        regVal |= (1 << pin);
    } else {
        regVal &= ~(1 << pin);
    }

    if (!_writeReg(M5PM1_REG_GPIO_WAKE_CFG, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::ledEnSetDrive(m5pm1_gpio_drive_t drive)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_LED, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_GPIO_DRV, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (drive == M5PM1_GPIO_DRIVE_OPENDRAIN) {
        regVal |= (1 << 5);  // LED_EN_DRV is bit 5
    } else {
        regVal &= ~(1 << 5);
    }

    if (!_writeReg(M5PM1_REG_GPIO_DRV, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::dumpPinStatus()
{
    uint8_t mode, out, in, drv, pupd0, pupd1, func0, func1, wakeEn, wakeCfg;

    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_GPIO_MODE, &mode)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_OUT, &out)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_IN, &in)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_DRV, &drv)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_PUPD0, &pupd0)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_PUPD1, &pupd1)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_FUNC0, &func0)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_FUNC1, &func1)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_WAKE_EN, &wakeEn)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_WAKE_CFG, &wakeCfg)) return M5PM1_ERR_I2C_COMM;

    M5PM1_LOG_I(TAG_GPIO, "=== PM1 Pin Status ===");
    M5PM1_LOG_I(TAG_GPIO, "MODE: 0x%02X  OUT: 0x%02X  IN: 0x%02X  DRV: 0x%02X", mode, out, in, drv);
    M5PM1_LOG_I(TAG_GPIO, "PUPD0: 0x%02X  PUPD1: 0x%02X", pupd0, pupd1);
    M5PM1_LOG_I(TAG_GPIO, "FUNC0: 0x%02X  FUNC1: 0x%02X", func0, func1);
    M5PM1_LOG_I(TAG_GPIO, "WAKE_EN: 0x%02X  WAKE_CFG: 0x%02X", wakeEn, wakeCfg);

    const char* funcNames[] = {"GPIO", "IRQ", "WAKE", "OTHER"};
    const char* pullNames[] = {"NONE", "UP", "DOWN", "?"};
    const char* drvNames[]  = {"PP", "OD"};

    for (int i = 0; i < 5; i++) {
        uint8_t funcVal = (i < 4) ? ((func0 >> (i * 2)) & 0x03) : ((func1 >> 0) & 0x03);
        uint8_t pullVal = (i < 4) ? ((pupd0 >> (i * 2)) & 0x03) : ((pupd1 >> 0) & 0x03);
        uint8_t modeVal = (mode >> i) & 0x01;
        uint8_t outVal  = (out >> i) & 0x01;
        uint8_t inVal   = (in >> i) & 0x01;
        M5PM1_LOG_I(TAG_GPIO, "GPIO%d: %s %s %s OUT=%d IN=%d", i, funcNames[funcVal], modeVal ? "OUT" : "IN",
                    pullNames[pullVal], outVal, inVal);
    }

    M5PM1_LOG_I(TAG_GPIO, "LED_EN_DRV: %s", drvNames[(drv >> 5) & 0x01]);
    M5PM1_LOG_I(TAG_GPIO, "======================");

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::verifyPinConfig(bool enableLog)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 读取所有GPIO相关寄存器
    // Read all GPIO-related registers
    uint8_t mode, out, drv, pupd0, pupd1, func0, func1, wakeEn, wakeCfg, holdCfg;

    if (!_readReg(M5PM1_REG_GPIO_MODE, &mode)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_OUT, &out)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_DRV, &drv)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_PUPD0, &pupd0)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_PUPD1, &pupd1)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_FUNC0, &func0)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_FUNC1, &func1)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_WAKE_EN, &wakeEn)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_GPIO_WAKE_CFG, &wakeCfg)) return M5PM1_ERR_I2C_COMM;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &holdCfg)) return M5PM1_ERR_I2C_COMM;

    bool hasError = false;

    // 逐个引脚比对
    // Compare pin by pin
    for (int pin = 0; pin < M5PM1_MAX_GPIO_PINS; pin++) {
        if (!_cacheValid) {
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d: Cache invalid, skipping verification", pin);
            }
            continue;
        }

        const m5pm1_pin_status_t& cached = _pinStatus[pin];

        // 检查 MODE
        m5pm1_gpio_mode_t actualMode = (mode & (1 << pin)) ? M5PM1_GPIO_MODE_OUTPUT : M5PM1_GPIO_MODE_INPUT;
        if (cached.mode != actualMode) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d MODE mismatch: cached=%d actual=%d", pin, cached.mode, actualMode);
            }
        }

        // 检查 OUTPUT (仅输出模式)
        if (actualMode == M5PM1_GPIO_MODE_OUTPUT) {
            uint8_t actualOut = (out >> pin) & 0x01;
            if (cached.output != actualOut) {
                hasError = true;
                if (enableLog) {
                    M5PM1_LOG_W(TAG_GPIO, "GPIO%d OUT mismatch: cached=%d actual=%d", pin, cached.output, actualOut);
                }
            }
        }

        // 检查 DRIVE
        m5pm1_gpio_drive_t actualDrive = (drv & (1 << pin)) ? M5PM1_GPIO_DRIVE_OPENDRAIN : M5PM1_GPIO_DRIVE_PUSHPULL;
        if (cached.drive != actualDrive) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d DRIVE mismatch: cached=%d actual=%d", pin, cached.drive, actualDrive);
            }
        }

        // 检查 PULL
        m5pm1_gpio_pull_t actualPull;
        if (pin < 4) {
            actualPull = (m5pm1_gpio_pull_t)((pupd0 >> (pin * 2)) & 0x03);
        } else {
            actualPull = (m5pm1_gpio_pull_t)((pupd1 >> 0) & 0x03);
        }
        if (cached.pull != actualPull) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d PULL mismatch: cached=%d actual=%d", pin, cached.pull, actualPull);
            }
        }

        // 检查 FUNC
        m5pm1_gpio_func_t actualFunc;
        if (pin < 4) {
            actualFunc = (m5pm1_gpio_func_t)((func0 >> (pin * 2)) & 0x03);
        } else {
            actualFunc = (m5pm1_gpio_func_t)((func1 >> 0) & 0x03);
        }
        if (cached.func != actualFunc) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d FUNC mismatch: cached=%d actual=%d", pin, cached.func, actualFunc);
            }
        }

        // 检查 WAKE_EN
        bool actualWakeEn = (wakeEn & (1 << pin)) != 0;
        if (cached.wake_en != actualWakeEn) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d WAKE_EN mismatch: cached=%d actual=%d", pin, cached.wake_en,
                            actualWakeEn);
            }
        }

        // 检查 WAKE_EDGE
        m5pm1_gpio_wake_edge_t actualWakeEdge =
            (wakeCfg & (1 << pin)) ? M5PM1_GPIO_WAKE_RISING : M5PM1_GPIO_WAKE_FALLING;
        if (cached.wake_edge != actualWakeEdge) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d WAKE_EDGE mismatch: cached=%d actual=%d", pin, cached.wake_edge,
                            actualWakeEdge);
            }
        }

        // 检查 POWER_HOLD
        bool actualPowerHold = (holdCfg & (1 << pin)) != 0;
        if (cached.power_hold != actualPowerHold) {
            hasError = true;
            if (enableLog) {
                M5PM1_LOG_W(TAG_GPIO, "GPIO%d POWER_HOLD mismatch: cached=%d actual=%d", pin, cached.power_hold,
                            actualPowerHold);
            }
        }
    }

    // 更新缓存为实际值
    // Update cache with actual values
    if (hasError || !_cacheValid) {
        _snapshotPinStates();
    }

    return hasError ? M5PM1_ERR_VERIFY_FAILED : M5PM1_OK;
}

m5pm1_err_t M5PM1::getPinStatus(m5pm1_gpio_num_t pin, m5pm1_pin_status_t* status)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (status == nullptr) return M5PM1_ERR_INVALID_ARG;

    if (!_cacheValid) {
        if (!_snapshotPinStates()) return M5PM1_ERR_I2C_COMM;
    }

    *status = _pinStatus[pin];
    return M5PM1_OK;
}

const m5pm1_pin_status_t* M5PM1::getPinStatusArray() const
{
    return _pinStatus;
}

// ============================
// 电源保持功能
// Power Hold Functions
// ============================

m5pm1_err_t M5PM1::gpioSetPowerHold(m5pm1_gpio_num_t pin, bool enable)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_I(TAG_GPIO, "GPIO%d Power Hold %s", pin, enable ? "Enabled" : "Disabled");

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (enable) {
        regVal |= (1 << pin);
    } else {
        regVal &= ~(1 << pin);
    }

    if (!_writeReg(M5PM1_REG_HOLD_CFG, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_GPIO | M5PM1_SNAPSHOT_DOMAIN_POWER);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::gpioGetPowerHold(m5pm1_gpio_num_t pin, bool* enable)
{
    if (!_isValidPin(pin)) return M5PM1_ERR_INVALID_ARG;
    if (enable == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    *enable = (regVal >> pin) & 0x01;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::ldoSetPowerHold(bool enable)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (enable) {
        regVal |= (1 << 5);  // LDO bit
    } else {
        regVal &= ~(1 << 5);
    }

    if (!_writeReg(M5PM1_REG_HOLD_CFG, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_POWER);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::ldoGetPowerHold(bool* enable)
{
    if (enable == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    *enable = (regVal >> 5) & 0x01;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::boostSetPowerHold(bool enable)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (enable) {
        regVal |= (1 << 6);  // BOOST bit
    } else {
        regVal &= ~(1 << 6);
    }

    if (!_writeReg(M5PM1_REG_HOLD_CFG, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_POWER);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::boostGetPowerHold(bool* enable)
{
    if (enable == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_HOLD_CFG, &regVal)) return M5PM1_ERR_I2C_COMM;

    *enable = (regVal >> 6) & 0x01;
    return M5PM1_OK;
}

// ============================
// ADC 功能
// ADC Functions
// ============================

m5pm1_err_t M5PM1::analogRead(m5pm1_adc_channel_t channel, uint16_t* value)
{
    if (value == nullptr) {
        M5PM1_LOG_E(TAG_ADC, "analogRead value is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (channel != M5PM1_ADC_CH_1 && channel != M5PM1_ADC_CH_2 && channel != M5PM1_ADC_CH_TEMP) {
        M5PM1_LOG_E(TAG_ADC, "Invalid ADC channel: %d", channel);
        return M5PM1_ERR_INVALID_ARG;
    }

    // 启动转换
    // Start conversion
    uint8_t ctrl = ((uint8_t)channel << 1) | 0x01;
    if (!_writeReg(M5PM1_REG_ADC_CTRL, ctrl)) {
        M5PM1_LOG_E(TAG_ADC, "Failed to write ADC_CTRL register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 等待转换完成
    // Wait for conversion completion
    uint8_t reg = 0;
    int tries   = 0;
    do {
        M5PM1_DELAY_MS(10);  // 增加间隔 Increase interval
        if (!_readReg(M5PM1_REG_ADC_CTRL, &reg)) {
            M5PM1_LOG_E(TAG_ADC, "Failed to read ADC_CTRL register");
            return M5PM1_ERR_I2C_COMM;
        }
        M5PM1_LOG_V(TAG_ADC, "ADC ch%d waiting... tries=%d status=0x%02X", channel, tries, reg);
        tries++;
    } while ((reg & 0x01) && tries < 50);  // 最多等待 500ms / Max wait 500ms

    if (reg & 0x01) {
        M5PM1_LOG_E(TAG_ADC, "ADC conversion timeout on channel %d", channel);
        return M5PM1_ERR_TIMEOUT;
    }

    if (!_readReg16(M5PM1_REG_ADC_RES_L, value)) {
        M5PM1_LOG_E(TAG_ADC, "Failed to read ADC result register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 屏蔽高4位，只保留12位ADC数据
    // Mask high 4 bits, keep only 12-bit ADC data
    *value &= 0x0FFF;
    M5PM1_LOG_D(TAG_ADC, "ADC ch%d: raw=%u (tries=%d)", channel, *value, tries);

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_ADC);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::isAdcBusy(bool* busy)
{
    if (busy == nullptr) {
        M5PM1_LOG_E(TAG_ADC, "isAdcBusy busy is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t ctrl = 0;
    if (!_readReg(M5PM1_REG_ADC_CTRL, &ctrl)) {
        M5PM1_LOG_E(TAG_ADC, "Failed to read ADC_CTRL register");
        return M5PM1_ERR_I2C_COMM;
    }

    *busy = (ctrl & 0x01) != 0;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::disableAdc()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 写入寄存器
    // Write register
    if (!_writeReg(M5PM1_REG_ADC_CTRL, 0)) {
        M5PM1_LOG_E(TAG_ADC, "Failed to write ADC_CTRL register for disable");
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint8_t actualCtrl = 0;
    if (!_readReg(M5PM1_REG_ADC_CTRL, &actualCtrl)) {
        M5PM1_LOG_E(TAG_ADC, "Failed to read back ADC_CTRL register for disable");
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    if (actualCtrl != 0) {
        M5PM1_LOG_E(TAG_ADC, "ADC disable verification failed: expected=0, actual=0x%02X", actualCtrl);
        return M5PM1_FAIL;
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_ADC);
    M5PM1_LOG_I(TAG_ADC, "ADC disabled and verified");
    return M5PM1_OK;
}

// ============================
// 温度传感器
// Temperature Sensor
// ============================

m5pm1_err_t M5PM1::readTemperature(uint16_t* temperature)
{
    return analogRead(M5PM1_ADC_CH_TEMP, temperature);
}

// ============================
// PWM 功能
// PWM Functions
// ============================

m5pm1_err_t M5PM1::setPwmFrequency(uint16_t frequency)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_PWM, "setPwmFrequency: %d Hz", frequency);

    // 写入寄存器
    // Write register
    if (!_writeReg16(M5PM1_REG_PWM_FREQ_L, frequency)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to write PWM frequency register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint16_t actualFreq = 0;
    if (!_readReg16(M5PM1_REG_PWM_FREQ_L, &actualFreq)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to read back PWM frequency register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    if (actualFreq != frequency) {
        M5PM1_LOG_E(TAG_PWM, "PWM frequency verification failed: expected=%d, actual=%d", frequency, actualFreq);
        return M5PM1_FAIL;
    }

    _pwmFrequency = frequency;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_PWM);
    M5PM1_LOG_I(TAG_PWM, "PWM frequency set and verified: %d Hz", frequency);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getPwmFrequency(uint16_t* frequency)
{
    if (frequency == nullptr) {
        M5PM1_LOG_E(TAG_PWM, "getPwmFrequency frequency is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    if (!_readReg16(M5PM1_REG_PWM_FREQ_L, frequency)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to read PWM frequency register");
        return M5PM1_ERR_I2C_COMM;
    }

    _pwmFrequency = *frequency;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setPwmDuty(m5pm1_pwm_channel_t channel, uint8_t duty, bool polarity, bool enable)
{
    if (channel > M5PM1_PWM_CH_1 || duty > 100) {
        M5PM1_LOG_E(TAG_PWM, "Invalid PWM channel or duty: ch=%d duty=%d", channel, duty);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 百分比转换为 12-bit
    // Convert percentage to 12-bit
    uint16_t duty12 = (uint16_t)((duty * 0x0FFF) / 100);
    return setPwmDuty12bit(channel, duty12, polarity, enable);
}

m5pm1_err_t M5PM1::getPwmDuty(m5pm1_pwm_channel_t channel, uint8_t* duty, bool* polarity, bool* enable)
{
    if (channel > M5PM1_PWM_CH_1) {
        M5PM1_LOG_E(TAG_PWM, "Invalid PWM channel: %d", channel);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (duty == nullptr || polarity == nullptr || enable == nullptr) {
        M5PM1_LOG_E(TAG_PWM, "getPwmDuty output pointer is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regL  = (channel == M5PM1_PWM_CH_0) ? M5PM1_REG_PWM0_L : M5PM1_REG_PWM1_L;
    uint16_t data = 0;
    if (!_readReg16(regL, &data)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to read PWM channel %d duty register", channel);
        return M5PM1_ERR_I2C_COMM;
    }

    uint16_t duty12 = data & 0x0FFF;
    *duty           = (uint8_t)((duty12 * 100) / 0x0FFF);
    *polarity       = (data & ((uint16_t)0x20 << 8)) != 0;
    *enable         = (data & ((uint16_t)0x10 << 8)) != 0;

    _pwmStates[channel].duty12   = duty12;
    _pwmStates[channel].polarity = *polarity;
    _pwmStates[channel].enabled  = *enable;
    _pwmStatesValid              = true;

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setPwmDuty12bit(m5pm1_pwm_channel_t channel, uint16_t duty12, bool polarity, bool enable)
{
    if (channel > M5PM1_PWM_CH_1 || duty12 > 0x0FFF) {
        M5PM1_LOG_E(TAG_PWM, "Invalid PWM channel or duty12: ch=%d duty12=%d", channel, duty12);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    M5PM1_LOG_D(TAG_PWM, "setPwmDuty12bit: ch=%d duty=%d pol=%s en=%s", channel, duty12, polarity ? "Inv" : "Norm",
                enable ? "On" : "Off");

    uint8_t regL  = (channel == M5PM1_PWM_CH_0) ? M5PM1_REG_PWM0_L : M5PM1_REG_PWM1_L;
    uint8_t dataL = (uint8_t)(duty12 & 0xFF);
    uint8_t dataH = (uint8_t)((duty12 >> 8) & 0x0F);
    if (polarity) dataH |= 0x20;
    if (enable) dataH |= 0x10;

    uint8_t buf[2] = {dataL, dataH};

    // 写入寄存器
    // Write register
    if (!_writeBytes(regL, buf, 2)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to write PWM channel %d duty register", channel);
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint16_t actualData = 0;
    if (!_readReg16(regL, &actualData)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to read back PWM channel %d duty register", channel);
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    uint16_t actualDuty12 = actualData & 0x0FFF;
    bool actualPolarity   = (actualData & ((uint16_t)0x20 << 8)) != 0;
    bool actualEnable     = (actualData & ((uint16_t)0x10 << 8)) != 0;

    if (actualDuty12 != duty12 || actualPolarity != polarity || actualEnable != enable) {
        M5PM1_LOG_E(TAG_PWM, "PWM channel %d verification failed: duty=%d/%d, pol=%d/%d, en=%d/%d", channel, duty12,
                    actualDuty12, polarity, actualPolarity, enable, actualEnable);
        return M5PM1_FAIL;
    }

    _pwmStates[channel].duty12   = duty12;
    _pwmStates[channel].polarity = polarity;
    _pwmStates[channel].enabled  = enable;
    _pwmStatesValid              = true;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_PWM);

    M5PM1_LOG_I(TAG_PWM, "PWM ch%d verified: duty=%d pol=%s en=%s", channel, duty12, polarity ? "Inv" : "Norm",
                enable ? "On" : "Off");
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getPwmDuty12bit(m5pm1_pwm_channel_t channel, uint16_t* duty12, bool* polarity, bool* enable)
{
    if (channel > M5PM1_PWM_CH_1 || duty12 == nullptr || polarity == nullptr || enable == nullptr) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regL  = (channel == M5PM1_PWM_CH_0) ? M5PM1_REG_PWM0_L : M5PM1_REG_PWM1_L;
    uint16_t data = 0;
    if (!_readReg16(regL, &data)) {
        M5PM1_LOG_E(TAG_PWM, "Failed to read PWM channel %d duty register", channel);
        return M5PM1_ERR_I2C_COMM;
    }

    *duty12   = data & 0x0FFF;
    *polarity = (data & ((uint16_t)0x20 << 8)) != 0;
    *enable   = (data & ((uint16_t)0x10 << 8)) != 0;

    _pwmStates[channel].duty12   = *duty12;
    _pwmStates[channel].polarity = *polarity;
    _pwmStates[channel].enabled  = *enable;
    _pwmStatesValid              = true;

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setPwmConfig(m5pm1_pwm_channel_t channel, bool enable, bool polarity, uint16_t frequency,
                                uint16_t duty12)
{
    if (channel > M5PM1_PWM_CH_1 || duty12 > 0x0FFF) {
        M5PM1_LOG_E(TAG_PWM, "Invalid PWM config: ch=%d duty12=%d", channel, duty12);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWM, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t pin = (channel == M5PM1_PWM_CH_0) ? M5PM1_GPIO_NUM_3 : M5PM1_GPIO_NUM_4;

    m5pm1_validation_t validation = validateConfig(pin, M5PM1_CONFIG_PWM, enable);
    if (!validation.valid) {
        M5PM1_LOG_W(TAG_PWM, "PWM config warning on GPIO%d: %s", pin, validation.error_msg);
    }

    if (_cacheValid && _pinStatus[pin].func != M5PM1_GPIO_FUNC_OTHER) {
        M5PM1_LOG_W(TAG_PWM, "GPIO%d not configured for PWM function (OTHER mode)", pin);
    }

    if (_pwmStatesValid && _pwmFrequency != frequency) {
        m5pm1_pwm_channel_t other = (channel == M5PM1_PWM_CH_0) ? M5PM1_PWM_CH_1 : M5PM1_PWM_CH_0;
        if (_pwmStates[other].enabled) {
            M5PM1_LOG_W(TAG_PWM, "PWM frequency change affects other channel: %d -> %d", _pwmFrequency, frequency);
        }
    }

    // 先设频率再设 duty，避免频率设置失败时 duty 已以旧频率写入
    // Set frequency first, then duty, to avoid duty being written with old frequency if frequency fails
    m5pm1_err_t err = setPwmFrequency(frequency);
    if (err != M5PM1_OK) return err;
    return setPwmDuty12bit(channel, duty12, polarity, enable);
}

m5pm1_err_t M5PM1::analogWrite(m5pm1_pwm_channel_t channel, uint8_t value)
{
    if (channel > M5PM1_PWM_CH_1) {
        M5PM1_LOG_E(TAG_GPIO, "Invalid channel: ch=%d", channel);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_GPIO, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 值为 0 时关闭 PWM 输出
    // Turn off PWM when value is 0
    if (value == 0) {
        return setPwmDuty12bit(channel, 0, false, false);
    }

    // 将 8-bit 值缩放到 12-bit
    // Scale 8-bit value to 12-bit
    uint16_t duty12 = (uint16_t)value * 16 + (uint16_t)value / 16;
    return setPwmDuty12bit(channel, duty12, false, true);
}

// ============================
// 电压读取功能
// Voltage Reading Functions
// ============================

m5pm1_err_t M5PM1::readVref(uint16_t* mv)
{
    if (mv == nullptr) {
        M5PM1_LOG_E(TAG_ADC, "readVref mv is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg16(M5PM1_REG_VREF_L, mv)) {
        M5PM1_LOG_E(TAG_ADC, "Failed to read reference voltage");
        return M5PM1_ERR_I2C_COMM;
    }
    M5PM1_LOG_D(TAG_ADC, "readVref: %u mV", *mv);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getRefVoltage(uint16_t* mv)
{
    return readVref(mv);
}

m5pm1_err_t M5PM1::readVbat(uint16_t* mv)
{
    if (mv == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg16(M5PM1_REG_VBAT_L, mv)) return M5PM1_ERR_I2C_COMM;
    M5PM1_LOG_D(TAG_ADC, "readVbat: %u mV", *mv);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::readVin(uint16_t* mv)
{
    if (mv == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg16(M5PM1_REG_VIN_L, mv)) return M5PM1_ERR_I2C_COMM;
    M5PM1_LOG_D(TAG_ADC, "readVin: %u mV", *mv);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::read5VInOut(uint16_t* mv)
{
    if (mv == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg16(M5PM1_REG_5VINOUT_L, mv)) return M5PM1_ERR_I2C_COMM;
    M5PM1_LOG_D(TAG_ADC, "read5VInOut: %u mV", *mv);
    return M5PM1_OK;
}

// ============================
// 电源管理
// Power Management
// ============================

m5pm1_err_t M5PM1::getPowerSource(m5pm1_pwr_src_t* src)
{
    if (src == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_ADC, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t val;
    if (!_readReg(M5PM1_REG_PWR_SRC, &val)) return M5PM1_ERR_I2C_COMM;
    *src = (m5pm1_pwr_src_t)(val & 0x07);
    M5PM1_LOG_D(TAG_ADC, "Power source: %s",
                (*src == 0)   ? "None"
                : (*src == 1) ? "USB"
                : (*src == 2) ? "Battery"
                              : "Unknown");
    return M5PM1_OK;
}

// ============================
// 唤醒源读取
// Wake Source Read
// ============================
m5pm1_err_t M5PM1::getWakeSource(uint8_t* src, m5pm1_clean_type_t cleanType)
{
    if (src == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_WAKE_SRC, src)) return M5PM1_ERR_I2C_COMM;

    if (cleanType == M5PM1_CLEAN_ONCE && *src != 0) {
        // 清除已触发的位（采用"写0清除"机制）
        // Clear triggered bits (using "write-0-to-clear" mechanism)
        // 反转位：触发位变为0，未触发位变为1
        // Bit invert: triggered bits become 0, untriggered bits become 1
        uint8_t new_val = ~(*src);
        if (!_writeReg(M5PM1_REG_WAKE_SRC, new_val)) return M5PM1_ERR_I2C_COMM;
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有位（采用"写0清除"机制：写入0x00）
        // Clear all bits (using "write-0-to-clear" mechanism: write 0x00)
        if (!_writeReg(M5PM1_REG_WAKE_SRC, 0x00)) return M5PM1_ERR_I2C_COMM;
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::clearWakeSource(uint8_t mask)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // PM1芯片采用"写0清除"机制：读取当前值，清除mask中为1的位，写回
    // PM1 chip uses "write-0-to-clear" mechanism: read, clear bits in mask, write back
    uint8_t wake_src;
    if (!_readReg(M5PM1_REG_WAKE_SRC, &wake_src)) {
        return M5PM1_ERR_I2C_COMM;
    }
    uint8_t new_val = wake_src & ~mask;
    if (!_writeReg(M5PM1_REG_WAKE_SRC, new_val)) {
        return M5PM1_ERR_I2C_COMM;
    }

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setPowerConfig(uint8_t mask, uint8_t value)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_D(TAG_PWR, "setPowerConfig: mask=0x%02X val=0x%02X", mask, value);
    uint8_t current;
    if (!_readReg(M5PM1_REG_PWR_CFG, &current)) return M5PM1_ERR_I2C_COMM;
    current = (current & ~mask) | (value & mask);
    if (!_writeReg(M5PM1_REG_PWR_CFG, current)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_POWER);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getPowerConfig(uint8_t* config)
{
    if (config == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_PWR_CFG, config)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::clearPowerConfig(uint8_t mask)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t current;
    if (!_readReg(M5PM1_REG_PWR_CFG, &current)) return M5PM1_ERR_I2C_COMM;
    current &= ~mask;
    if (!_writeReg(M5PM1_REG_PWR_CFG, current)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_POWER);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setChargeEnable(bool enable)
{
    M5PM1_LOG_I(TAG_PWR, "Charge %s", enable ? "Enabled" : "Disabled");
    return setPowerConfig(M5PM1_PWR_CFG_CHG_EN, enable ? M5PM1_PWR_CFG_CHG_EN : 0);
}

m5pm1_err_t M5PM1::setDcdcEnable(bool enable)
{
    M5PM1_LOG_I(TAG_PWR, "DCDC %s", enable ? "Enabled" : "Disabled");
    return setPowerConfig(M5PM1_PWR_CFG_DCDC_EN, enable ? M5PM1_PWR_CFG_DCDC_EN : 0);
}

m5pm1_err_t M5PM1::setLdoEnable(bool enable)
{
    M5PM1_LOG_I(TAG_PWR, "LDO %s", enable ? "Enabled" : "Disabled");
    return setPowerConfig(M5PM1_PWR_CFG_LDO_EN, enable ? M5PM1_PWR_CFG_LDO_EN : 0);
}

m5pm1_err_t M5PM1::setBoostEnable(bool enable)
{
    M5PM1_LOG_I(TAG_PWR, "Boost %s", enable ? "Enabled" : "Disabled");
    return setPowerConfig(M5PM1_PWR_CFG_BOOST_EN, enable ? M5PM1_PWR_CFG_BOOST_EN : 0);
}

m5pm1_err_t M5PM1::setLedEnLevel(bool level)
{
    M5PM1_LOG_I(TAG_LED, "LED Enable Level: %s", level ? "High" : "Low");
    return setPowerConfig(M5PM1_PWR_CFG_LED_CTRL, level ? M5PM1_PWR_CFG_LED_CTRL : 0);
}

// ============================
// 电池功能
// Battery Functions
// ============================

m5pm1_err_t M5PM1::setBatteryLvp(uint16_t mv)
{
    // 检查初始化状态 / Check if initialized
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 验证电压范围 (2000-4000 mV) / Validate voltage range
    if (mv < 2000 || mv > 4000) {
        M5PM1_LOG_E(TAG_PWR, "Invalid battery LVP value: %u mV (valid range: 2000-4000 mV)", mv);
        return M5PM1_ERR_INVALID_ARG;
    }

    // 计算寄存器值 / Calculate register value
    // 公式: (电压_mV - 2000) / 7.81 ≈ (电压_mV - 2000) * 100 / 781
    // Formula: (voltage_mv - 2000) / 7.81 ≈ (voltage_mv - 2000) * 100 / 781
    uint8_t lvp = (uint8_t)((mv - 2000) * 100 / 781);

    M5PM1_LOG_I(TAG_PWR, "Battery LVP set to %u mV", mv);
    if (!_writeReg(M5PM1_REG_BATT_LVP, lvp)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

// ============================
// 看门狗功能
// Watchdog Functions
// ============================

m5pm1_err_t M5PM1::wdtSet(uint8_t timeout_sec)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_SYS, "Watchdog set to %d sec", timeout_sec);
    if (!_writeReg(M5PM1_REG_WDT_CNT, timeout_sec)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::wdtFeed()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_D(TAG_SYS, "wdtFeed");
    if (!_writeReg(M5PM1_REG_WDT_KEY, M5PM1_WDT_FEED_KEY)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::wdtGetCount(uint8_t* count)
{
    if (count == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_WDT_CNT, count)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

// ============================
// 定时器功能
// Timer Functions
// ============================

m5pm1_err_t M5PM1::timerSet(uint32_t seconds, m5pm1_tim_action_t action)
{
    // 检查初始化状态 / Check if initialized
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 验证定时器计数值 (最大 214748364 秒) / Validate timer count (max 214748364 seconds)
    if (seconds > 214748364) {
        M5PM1_LOG_E(TAG_SYS, "Invalid timer count: %lu (max 214748364, ~6.8 years)", seconds);
        return M5PM1_ERR_INVALID_ARG;
    }

    // 写入31位定时器值 / Write 31-bit timer value
    uint8_t data[4];
    data[0] = (seconds >> 0) & 0xFF;
    data[1] = (seconds >> 8) & 0xFF;
    data[2] = (seconds >> 16) & 0xFF;
    data[3] = (seconds >> 24) & 0x7F;

    if (!_writeBytes(M5PM1_REG_TIM_CNT_0, data, 4)) return M5PM1_ERR_I2C_COMM;

    // 配置定时器 / Configure timer
    // bit[3] ARM=1: 启动定时器 / Start timer
    // bit[2:0] ACTION: 定时器动作 / Timer action
    uint8_t cfg = 0x08 | (uint8_t)action;  // ARM=1, set ACTION

    if (!_writeReg(M5PM1_REG_TIM_CFG, cfg)) return M5PM1_ERR_I2C_COMM;

    // 重载定时器 / Reload timer
    if (!_writeReg(M5PM1_REG_TIM_KEY, M5PM1_TIM_RELOAD_KEY)) return M5PM1_ERR_I2C_COMM;
    M5PM1_LOG_D(TAG_SYS, "timerSet: %lu sec action=%d", seconds, (int)action);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::timerClear()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_D(TAG_SYS, "timerClear");
    // 清除定时器配置（停止计数）
    // Clear timer configuration (stop counting)
    if (!_writeReg(M5PM1_REG_TIM_CFG, 0)) return M5PM1_ERR_I2C_COMM;

    // 写入密钥以清除并重载定时器（与旧库行为一致）
    // Write key to clear and reload timer (consistent with old library behavior)
    if (!_writeReg(M5PM1_REG_TIM_KEY, M5PM1_TIM_RELOAD_KEY)) return M5PM1_ERR_I2C_COMM;

    return M5PM1_OK;
}

// ============================
// 按钮功能
// Button Functions
// ============================

m5pm1_err_t M5PM1::btnSetConfig(m5pm1_btn_type_t type, m5pm1_btn_delay_t delay)
{
    // 检查初始化状态 / Check if initialized
    if (!_initialized) {
        M5PM1_LOG_E(TAG_BTN, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_D(TAG_BTN, "btnSetConfig: type=%d delay=%d", (int)type, (int)delay);

    // 验证按钮类型 / Validate button type
    uint8_t shift;
    switch (type) {
        case M5PM1_BTN_TYPE_CLICK:
            shift = 1;
            break;
        case M5PM1_BTN_TYPE_LONG:
            shift = 3;
            break;
        case M5PM1_BTN_TYPE_DOUBLE:
            shift = 5;
            break;
        default:
            M5PM1_LOG_E(TAG_BTN, "Invalid button type: %d", type);
            return M5PM1_ERR_INVALID_ARG;
    }

    // 验证延迟配置 (0-3) / Validate delay configuration
    if (delay > M5PM1_BTN_CLICK_DELAY_1000MS) {
        M5PM1_LOG_E(TAG_BTN, "Invalid delay value: %d (valid range: 0-3)", delay);
        return M5PM1_ERR_INVALID_ARG;
    }

    // 读取当前寄存器值 / Read current register value
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_1, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 更新延迟位 / Update delay bits
    regVal &= ~(0x03 << shift);
    regVal |= ((uint8_t)delay << shift);

    if (!_writeReg(M5PM1_REG_BTN_CFG_1, regVal)) return M5PM1_ERR_I2C_COMM;
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_BUTTON);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::btnGetState(bool* pressed)
{
    if (pressed == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_BTN, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t val;
    // 使用内部函数读取寄存器并维护 BTN_FLAG 缓存
    // Use internal function to read register and maintain BTN_FLAG cache
    if (!_readBtnStatus(&val)) return M5PM1_ERR_I2C_COMM;
    // bit0: 0=释放, 1=按下
    // bit0: 0=released, 1=pressed
    *pressed = (val & 0x01) != 0;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::btnGetFlag(bool* wasPressed)
{
    if (wasPressed == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_BTN, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t val;
    // 使用内部函数读取寄存器并更新 BTN_FLAG 缓存
    // Use internal function to read register and update BTN_FLAG cache
    if (!_readBtnStatus(&val)) return M5PM1_ERR_I2C_COMM;

    // 检查 BTN_FLAG 是否被置位（无论是本次读取还是之前缓存的）
    // Check if BTN_FLAG was set (either from this read or cached from previous reads)
    *wasPressed = _btnFlagCache;

    // 报告后清除缓存（消耗该标志）
    // Clear cache after reporting (consume the flag)
    _btnFlagCache = false;

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setSingleResetDisable(bool disable)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_SYS, "Single Reset: %s", disable ? "Disabled" : "Enabled");
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_1, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (disable) {
        regVal |= 0x01;
    } else {
        regVal &= ~0x01;
    }

    bool ok = _writeReg(M5PM1_REG_BTN_CFG_1, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_BUTTON);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getSingleResetDisable(bool* disabled)
{
    if (disabled == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getSingleResetDisable disabled is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_1, &regVal)) return M5PM1_ERR_I2C_COMM;
    *disabled = (regVal & 0x01) != 0;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setDoubleOffDisable(bool disable)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_SYS, "Double Off: %s", disable ? "Disabled" : "Enabled");
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_2, &regVal)) return M5PM1_ERR_I2C_COMM;

    if (disable) {
        regVal |= 0x01;
    } else {
        regVal &= ~0x01;
    }

    bool ok = _writeReg(M5PM1_REG_BTN_CFG_2, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_BUTTON);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getDoubleOffDisable(bool* disabled)
{
    if (disabled == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getDoubleOffDisable disabled is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_2, &regVal)) return M5PM1_ERR_I2C_COMM;
    *disabled = (regVal & 0x01) != 0;
    return M5PM1_OK;
}

// ============================
// 中断功能
// IRQ Functions
// ============================

m5pm1_err_t M5PM1::irqGetGpioStatus(uint8_t* status, m5pm1_clean_type_t cleanType)
{
    if (status == nullptr) {
        M5PM1_LOG_E(TAG_IRQ, "irqGetGpioStatus status is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_IRQ_STATUS1, status)) return M5PM1_ERR_I2C_COMM;
    M5PM1_LOG_D(TAG_IRQ, "IRQ GPIO status: 0x%02X clean=%s", *status,
                cleanType == M5PM1_CLEAN_NONE   ? "None"
                : cleanType == M5PM1_CLEAN_ONCE ? "Once"
                                                : "All");

    if (cleanType == M5PM1_CLEAN_ONCE && *status != 0) {
        // 清除已触发的位（采用"写0清除"机制）
        // Clear triggered bits (using "write-0-to-clear" mechanism)
        // 反转位：触发位变为0，未触发位变为1
        // Bit invert: triggered bits become 0, untriggered bits become 1
        uint8_t new_val = ~(*status);
        if (!_writeReg(M5PM1_REG_IRQ_STATUS1, new_val)) return M5PM1_ERR_I2C_COMM;
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有位（采用"写0清除"机制：写入0x00）
        // Clear all bits (using "write-0-to-clear" mechanism: write 0x00)
        if (!_writeReg(M5PM1_REG_IRQ_STATUS1, 0x00)) return M5PM1_ERR_I2C_COMM;
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetGpioStatusEnum(m5pm1_irq_gpio_t* gpio_num, m5pm1_clean_type_t cleanType)
{
    // 参数验证
    // Parameter validation
    if (gpio_num == nullptr) {
        M5PM1_LOG_E(TAG_IRQ, "irqGetGpioStatusEnum gpio_num is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 读取 GPIO 中断状态寄存器
    // Read GPIO interrupt status register
    uint8_t irq_status;
    if (!_readReg(M5PM1_REG_IRQ_STATUS1, &irq_status)) {
        return M5PM1_ERR_I2C_COMM;
    }

    // 从低位往高位查找第一个触发的 GPIO
    // Find first triggered GPIO from low to high
    *gpio_num = M5PM1_IRQ_GPIO_NONE;
    for (int i = 0; i < 5; i++) {
        if (irq_status & (1 << i)) {
            *gpio_num = (m5pm1_irq_gpio_t)(1 << i);
            break;
        }
    }

    // 根据 cleanType 清除中断
    // Clear interrupt based on cleanType
    if (cleanType == M5PM1_CLEAN_ONCE && *gpio_num != M5PM1_IRQ_GPIO_NONE) {
        // 清除当前返回的 GPIO 中断（写0清除机制）
        // Clear current GPIO interrupt (write-0-to-clear mechanism)
        uint8_t new_val = irq_status & ~(*gpio_num);
        if (!_writeReg(M5PM1_REG_IRQ_STATUS1, new_val)) {
            return M5PM1_ERR_I2C_COMM;
        }
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有 GPIO 中断
        // Clear all GPIO interrupts
        if (!_writeReg(M5PM1_REG_IRQ_STATUS1, 0x00)) {
            return M5PM1_ERR_I2C_COMM;
        }
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqClearGpioAll()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // PM1芯片采用"写0清除"机制：写入0x00清除所有位
    // PM1 chip uses "write-0-to-clear" mechanism: write 0x00 to clear all bits
    if (!_writeReg(M5PM1_REG_IRQ_STATUS1, 0x00)) {
        return M5PM1_ERR_I2C_COMM;
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetSysStatus(uint8_t* status, m5pm1_clean_type_t cleanType)
{
    if (status == nullptr) {
        M5PM1_LOG_E(TAG_IRQ, "irqGetSysStatus status is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_IRQ_STATUS2, status)) return M5PM1_ERR_I2C_COMM;

    if (cleanType == M5PM1_CLEAN_ONCE && *status != 0) {
        // 清除已触发的位（采用"写0清除"机制）
        // Clear triggered bits (using "write-0-to-clear" mechanism)
        // 反转位：触发位变为0，未触发位变为1
        // Bit invert: triggered bits become 0, untriggered bits become 1
        uint8_t new_val = ~(*status);
        if (!_writeReg(M5PM1_REG_IRQ_STATUS2, new_val)) return M5PM1_ERR_I2C_COMM;
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有位（采用"写0清除"机制：写入0x00）
        // Clear all bits (using "write-0-to-clear" mechanism: write 0x00)
        if (!_writeReg(M5PM1_REG_IRQ_STATUS2, 0x00)) return M5PM1_ERR_I2C_COMM;
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetSysStatusEnum(m5pm1_irq_sys_t* sys_irq, m5pm1_clean_type_t cleanType)
{
    // 参数验证
    // Parameter validation
    if (sys_irq == nullptr) {
        M5PM1_LOG_E(TAG_IRQ, "irqGetSysStatusEnum sys_irq is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 读取系统中断状态寄存器
    // Read system interrupt status register
    uint8_t irq_status;
    if (!_readReg(M5PM1_REG_IRQ_STATUS2, &irq_status)) {
        return M5PM1_ERR_I2C_COMM;
    }

    // 从低位往高位查找第一个触发的系统事件
    // Find first triggered system event from low to high
    *sys_irq = M5PM1_IRQ_SYS_NONE;
    for (int i = 0; i < 6; i++) {
        if (irq_status & (1 << i)) {
            *sys_irq = (m5pm1_irq_sys_t)(1 << i);
            break;
        }
    }

    // 根据 cleanType 清除中断
    // Clear interrupt based on cleanType
    if (cleanType == M5PM1_CLEAN_ONCE && *sys_irq != M5PM1_IRQ_SYS_NONE) {
        // 清除当前返回的系统中断（写0清除机制）
        // Clear current system interrupt (write-0-to-clear mechanism)
        uint8_t new_val = irq_status & ~(*sys_irq);
        if (!_writeReg(M5PM1_REG_IRQ_STATUS2, new_val)) {
            return M5PM1_ERR_I2C_COMM;
        }
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有系统中断
        // Clear all system interrupts
        if (!_writeReg(M5PM1_REG_IRQ_STATUS2, 0x00)) {
            return M5PM1_ERR_I2C_COMM;
        }
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqClearSysAll()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // PM1芯片采用"写0清除"机制：写入0x00清除所有位
    // PM1 chip uses "write-0-to-clear" mechanism: write 0x00 to clear all bits
    if (!_writeReg(M5PM1_REG_IRQ_STATUS2, 0x00)) {
        return M5PM1_ERR_I2C_COMM;
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetBtnStatus(uint8_t* status, m5pm1_clean_type_t cleanType)
{
    if (status == nullptr) {
        M5PM1_LOG_E(TAG_IRQ, "irqGetBtnStatus status is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_IRQ_STATUS3, status)) return M5PM1_ERR_I2C_COMM;
    M5PM1_LOG_D(TAG_IRQ, "IRQ BTN status: 0x%02X clean=%s", *status,
                cleanType == M5PM1_CLEAN_NONE   ? "None"
                : cleanType == M5PM1_CLEAN_ONCE ? "Once"
                                                : "All");

    if (cleanType == M5PM1_CLEAN_ONCE && *status != 0) {
        // 清除已触发的位（采用"写0清除"机制）
        // Clear triggered bits (using "write-0-to-clear" mechanism)
        // 反转位：触发位变为0，未触发位变为1
        // Bit invert: triggered bits become 0, untriggered bits become 1
        uint8_t new_val = ~(*status);
        if (!_writeReg(M5PM1_REG_IRQ_STATUS3, new_val)) return M5PM1_ERR_I2C_COMM;
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有位（采用"写0清除"机制：写入0x00）
        // Clear all bits (using "write-0-to-clear" mechanism: write 0x00)
        if (!_writeReg(M5PM1_REG_IRQ_STATUS3, 0x00)) return M5PM1_ERR_I2C_COMM;
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetBtnStatusEnum(m5pm1_irq_btn_t* btn_irq, m5pm1_clean_type_t cleanType)
{
    // 参数验证
    // Parameter validation
    if (btn_irq == nullptr) {
        M5PM1_LOG_E(TAG_IRQ, "irqGetBtnStatusEnum btn_irq is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 读取按钮中断状态寄存器
    // Read button interrupt status register
    uint8_t irq_status;
    if (!_readReg(M5PM1_REG_IRQ_STATUS3, &irq_status)) {
        return M5PM1_ERR_I2C_COMM;
    }

    // 从低位往高位查找第一个触发的按钮事件
    // Find first triggered button event from low to high
    *btn_irq = M5PM1_IRQ_BTN_NONE;
    for (int i = 0; i < 3; i++) {
        if (irq_status & (1 << i)) {
            *btn_irq = (m5pm1_irq_btn_t)(1 << i);
            break;
        }
    }

    // 根据 cleanType 清除中断
    // Clear interrupt based on cleanType
    if (cleanType == M5PM1_CLEAN_ONCE && *btn_irq != M5PM1_IRQ_BTN_NONE) {
        // 清除当前返回的按钮中断（写0清除机制）
        // Clear current button interrupt (write-0-to-clear mechanism)
        uint8_t new_val = irq_status & ~(*btn_irq);
        if (!_writeReg(M5PM1_REG_IRQ_STATUS3, new_val)) {
            return M5PM1_ERR_I2C_COMM;
        }
    } else if (cleanType == M5PM1_CLEAN_ALL) {
        // 清除所有按钮中断
        // Clear all button interrupts
        if (!_writeReg(M5PM1_REG_IRQ_STATUS3, 0x00)) {
            return M5PM1_ERR_I2C_COMM;
        }
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqClearBtnAll()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // PM1芯片采用"写0清除"机制：写入0x00清除所有位
    // PM1 chip uses "write-0-to-clear" mechanism: write 0x00 to clear all bits
    if (!_writeReg(M5PM1_REG_IRQ_STATUS3, 0x00)) {
        return M5PM1_ERR_I2C_COMM;
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_STATUS);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqSetGpioMask(m5pm1_irq_gpio_t gpio, m5pm1_irq_mask_ctrl_t mask)
{
    // 不支持 ALL / NONE
    // ALL / NONE not supported
    if (gpio == M5PM1_IRQ_GPIO_ALL || gpio == M5PM1_IRQ_GPIO_NONE) {
        M5PM1_LOG_E(TAG_IRQ, "ALL/NONE not supported");
        return M5PM1_ERR_NOT_SUPPORTED;
    }
    // 验证参数：gpio 必须是有效的单个 GPIO 位掩码 (bit[4:0])
    // Validate parameter: gpio must be a valid single GPIO bitmask (bit[4:0])
    if (gpio != M5PM1_IRQ_GPIO0 && gpio != M5PM1_IRQ_GPIO1 && gpio != M5PM1_IRQ_GPIO2 && gpio != M5PM1_IRQ_GPIO3 &&
        gpio != M5PM1_IRQ_GPIO4) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_IRQ, "IRQ GPIO mask 0x%02X %s", (uint8_t)gpio,
                mask == M5PM1_IRQ_MASK_ENABLE ? "Enabled" : "Disabled");

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_IRQ_MASK1, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 直接使用位掩码操作
    // Use bitmask operation directly
    if (mask == M5PM1_IRQ_MASK_ENABLE) {
        regVal |= (uint8_t)gpio;
    } else {
        regVal &= ~(uint8_t)gpio;
    }

    bool ok = _writeReg(M5PM1_REG_IRQ_MASK1, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetGpioMask(m5pm1_irq_gpio_t gpio, m5pm1_irq_mask_ctrl_t* mask)
{
    if (mask == nullptr) return M5PM1_ERR_INVALID_ARG;
    // 不支持 ALL / NONE
    // ALL / NONE not supported
    if (gpio == M5PM1_IRQ_GPIO_ALL || gpio == M5PM1_IRQ_GPIO_NONE) {
        M5PM1_LOG_E(TAG_IRQ, "ALL/NONE not supported");
        return M5PM1_ERR_NOT_SUPPORTED;
    }
    // 验证参数：gpio 必须是有效的单个 GPIO 位掩码 (bit[4:0])
    // Validate parameter: gpio must be a valid single GPIO bitmask (bit[4:0])
    if (gpio != M5PM1_IRQ_GPIO0 && gpio != M5PM1_IRQ_GPIO1 && gpio != M5PM1_IRQ_GPIO2 && gpio != M5PM1_IRQ_GPIO3 &&
        gpio != M5PM1_IRQ_GPIO4) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_IRQ_MASK1, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 直接使用位掩码检查
    // Use bitmask check directly
    *mask = (regVal & (uint8_t)gpio) ? M5PM1_IRQ_MASK_ENABLE : M5PM1_IRQ_MASK_DISABLE;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqSetGpioMaskAll(m5pm1_irq_mask_ctrl_t mask)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_IRQ, "IRQ GPIO mask all %s", mask == M5PM1_IRQ_MASK_ENABLE ? "Enabled" : "Disabled");
    // GPIO 中断屏蔽寄存器有效位为 bit[4:0]，共5个GPIO
    // GPIO interrupt mask register valid bits are bit[4:0], 5 GPIOs total
    uint8_t regVal = (mask == M5PM1_IRQ_MASK_ENABLE) ? 0x1F : 0x00;
    bool ok        = _writeReg(M5PM1_REG_IRQ_MASK1, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetGpioMaskBits(uint8_t* mask)
{
    if (mask == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_IRQ_MASK1, mask)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqSetSysMask(m5pm1_irq_sys_t event, m5pm1_irq_mask_ctrl_t mask)
{
    // 不支持 ALL / NONE
    // ALL / NONE not supported
    if (event == M5PM1_IRQ_SYS_ALL || event == M5PM1_IRQ_SYS_NONE) {
        M5PM1_LOG_E(TAG_IRQ, "ALL/NONE not supported");
        return M5PM1_ERR_NOT_SUPPORTED;
    }
    // 验证参数：event 必须是有效的单个系统事件位掩码 (bit[5:0])
    // Validate parameter: event must be a valid single system event bitmask (bit[5:0])
    if (event != M5PM1_IRQ_SYS_5VIN_INSERT && event != M5PM1_IRQ_SYS_5VIN_REMOVE &&
        event != M5PM1_IRQ_SYS_5VINOUT_INSERT && event != M5PM1_IRQ_SYS_5VINOUT_REMOVE &&
        event != M5PM1_IRQ_SYS_BAT_INSERT && event != M5PM1_IRQ_SYS_BAT_REMOVE) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_IRQ, "IRQ SYS mask 0x%02X %s", (uint8_t)event,
                mask == M5PM1_IRQ_MASK_ENABLE ? "Enabled" : "Disabled");

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_IRQ_MASK2, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 直接使用位掩码操作
    // Use bitmask operation directly
    if (mask == M5PM1_IRQ_MASK_ENABLE) {
        regVal |= (uint8_t)event;
    } else {
        regVal &= ~(uint8_t)event;
    }

    bool ok = _writeReg(M5PM1_REG_IRQ_MASK2, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetSysMask(m5pm1_irq_sys_t event, m5pm1_irq_mask_ctrl_t* mask)
{
    if (mask == nullptr) return M5PM1_ERR_INVALID_ARG;
    // 不支持 ALL / NONE
    // ALL / NONE not supported
    if (event == M5PM1_IRQ_SYS_ALL || event == M5PM1_IRQ_SYS_NONE) {
        M5PM1_LOG_E(TAG_IRQ, "ALL/NONE not supported");
        return M5PM1_ERR_NOT_SUPPORTED;
    }
    // 验证参数：event 必须是有效的单个系统事件位掩码 (bit[5:0])
    // Validate parameter: event must be a valid single system event bitmask (bit[5:0])
    if (event != M5PM1_IRQ_SYS_5VIN_INSERT && event != M5PM1_IRQ_SYS_5VIN_REMOVE &&
        event != M5PM1_IRQ_SYS_5VINOUT_INSERT && event != M5PM1_IRQ_SYS_5VINOUT_REMOVE &&
        event != M5PM1_IRQ_SYS_BAT_INSERT && event != M5PM1_IRQ_SYS_BAT_REMOVE) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_IRQ_MASK2, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 直接使用位掩码检查
    // Use bitmask check directly
    *mask = (regVal & (uint8_t)event) ? M5PM1_IRQ_MASK_ENABLE : M5PM1_IRQ_MASK_DISABLE;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqSetSysMaskAll(m5pm1_irq_mask_ctrl_t mask)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_IRQ, "IRQ SYS mask all %s", mask == M5PM1_IRQ_MASK_ENABLE ? "Enabled" : "Disabled");
    // 系统中断屏蔽寄存器有效位为 bit[5:0]，共6个系统事件
    // System interrupt mask register valid bits are bit[5:0], 6 system events total
    uint8_t regVal = (mask == M5PM1_IRQ_MASK_ENABLE) ? 0x3F : 0x00;
    bool ok        = _writeReg(M5PM1_REG_IRQ_MASK2, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetSysMaskBits(uint8_t* mask)
{
    if (mask == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_IRQ_MASK2, mask)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqSetBtnMask(m5pm1_irq_btn_t type, m5pm1_irq_mask_ctrl_t mask)
{
    // 不支持 ALL / NONE
    // ALL / NONE not supported
    if (type == M5PM1_IRQ_BTN_ALL || type == M5PM1_IRQ_BTN_NONE) {
        M5PM1_LOG_E(TAG_IRQ, "ALL/NONE not supported");
        return M5PM1_ERR_NOT_SUPPORTED;
    }
    // 验证参数：type 必须是有效的单个按钮事件位掩码 (bit[2:0])
    // Validate parameter: type must be a valid single button event bitmask (bit[2:0])
    if (type != M5PM1_IRQ_BTN_CLICK && type != M5PM1_IRQ_BTN_WAKE && type != M5PM1_IRQ_BTN_DOUBLE) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_IRQ, "IRQ BTN mask 0x%02X %s", (uint8_t)type,
                mask == M5PM1_IRQ_MASK_ENABLE ? "Enabled" : "Disabled");

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_IRQ_MASK3, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 直接使用位掩码操作
    // Use bitmask operation directly
    if (mask == M5PM1_IRQ_MASK_ENABLE) {
        regVal |= (uint8_t)type;
    } else {
        regVal &= ~(uint8_t)type;
    }

    bool ok = _writeReg(M5PM1_REG_IRQ_MASK3, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetBtnMask(m5pm1_irq_btn_t type, m5pm1_irq_mask_ctrl_t* mask)
{
    if (mask == nullptr) return M5PM1_ERR_INVALID_ARG;
    // 不支持 ALL / NONE
    // ALL / NONE not supported
    if (type == M5PM1_IRQ_BTN_ALL || type == M5PM1_IRQ_BTN_NONE) {
        M5PM1_LOG_E(TAG_IRQ, "ALL/NONE not supported");
        return M5PM1_ERR_NOT_SUPPORTED;
    }
    // 验证参数：type 必须是有效的单个按钮事件位掩码 (bit[2:0])
    // Validate parameter: type must be a valid single button event bitmask (bit[2:0])
    if (type != M5PM1_IRQ_BTN_CLICK && type != M5PM1_IRQ_BTN_WAKE && type != M5PM1_IRQ_BTN_DOUBLE) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regVal;
    if (!_readReg(M5PM1_REG_IRQ_MASK3, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 直接使用位掩码检查
    // Use bitmask check directly
    *mask = (regVal & (uint8_t)type) ? M5PM1_IRQ_MASK_ENABLE : M5PM1_IRQ_MASK_DISABLE;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqSetBtnMaskAll(m5pm1_irq_mask_ctrl_t mask)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_I(TAG_IRQ, "IRQ BTN mask all %s", mask == M5PM1_IRQ_MASK_ENABLE ? "Enabled" : "Disabled");
    // 按钮中断屏蔽寄存器有效位为 bit[2:0]，共3个按钮事件
    // Button interrupt mask register valid bits are bit[2:0], 3 button events total
    uint8_t regVal = (mask == M5PM1_IRQ_MASK_ENABLE) ? 0x07 : 0x00;
    bool ok        = _writeReg(M5PM1_REG_IRQ_MASK3, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_IRQ_MASK);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::irqGetBtnMaskBits(uint8_t* mask)
{
    if (mask == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_initialized) {
        M5PM1_LOG_E(TAG_IRQ, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_readReg(M5PM1_REG_IRQ_MASK3, mask)) return M5PM1_ERR_I2C_COMM;
    return M5PM1_OK;
}

// ============================
// 系统命令
// System Commands
// ============================

m5pm1_err_t M5PM1::sysCmd(m5pm1_sys_cmd_t cmd)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_PWR, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    M5PM1_LOG_D(TAG_PWR, "sysCmd: cmd=%d", (int)cmd);
    M5PM1_DELAY_MS(120);
    uint8_t val = M5PM1_SYS_CMD_KEY | (uint8_t)cmd;
    if (!_writeReg(M5PM1_REG_SYS_CMD, val)) {
        return M5PM1_ERR_I2C_COMM;
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::shutdown()
{
    M5PM1_LOG_I(TAG_PWR, "Shutdown requested");
    return sysCmd(M5PM1_SYS_CMD_OFF);
}

m5pm1_err_t M5PM1::reboot()
{
    M5PM1_LOG_I(TAG_PWR, "Reboot requested");
    return sysCmd(M5PM1_SYS_CMD_RESET);
}

m5pm1_err_t M5PM1::enterDownloadMode()
{
    return sysCmd(M5PM1_SYS_CMD_DL);
}

m5pm1_err_t M5PM1::setDownloadLock(bool lock)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_1, &regVal)) return M5PM1_ERR_I2C_COMM;

    // 检查锁定逻辑: 一旦锁定(变为1)，除断电外无法解锁(变为0)
    // Check lock logic: Once locked (set to 1), it cannot be unlocked (set to 0) until power cycle
    if ((regVal & 0x80) && !lock) {
        M5PM1_LOG_E(TAG_SYS, "Download lock is permanent until power cycle");
        return M5PM1_ERR_RULE_VIOLATION;
    }

    if (lock) {
        regVal |= 0x80;  // DL_LOCK is bit 7
    } else {
        regVal &= ~0x80;
    }

    bool ok = _writeReg(M5PM1_REG_BTN_CFG_1, regVal);
    if (!ok) {
        return M5PM1_ERR_I2C_COMM;
    }
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_BUTTON);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getDownloadLock(bool* lock)
{
    if (lock == nullptr) {
        M5PM1_LOG_E(TAG_SYS, "getDownloadLock lock is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_SYS, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    uint8_t regVal;
    if (!_readReg(M5PM1_REG_BTN_CFG_1, &regVal)) return M5PM1_ERR_I2C_COMM;
    *lock = (regVal & 0x80) != 0;
    return M5PM1_OK;
}

// ============================
// NeoPixel 功能
// NeoPixel Functions
// ============================

m5pm1_err_t M5PM1::setLeds(const m5pm1_rgb_t* colors, uint8_t arraySize, uint8_t count, bool autoRefresh)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_LED, "Device not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (colors == nullptr) {
        M5PM1_LOG_E(TAG_LED, "Colors array is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (count == 0) {
        M5PM1_LOG_E(TAG_LED, "LED count cannot be 0");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (count > M5PM1_MAX_LED_COUNT) {
        M5PM1_LOG_E(TAG_LED, "LED count %d exceeds maximum %d", count, M5PM1_MAX_LED_COUNT);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (count > arraySize) {
        M5PM1_LOG_E(TAG_LED, "LED count %d exceeds array size %d", count, arraySize);
        return M5PM1_ERR_INVALID_ARG;
    }

    m5pm1_err_t err = setLedCount(count);
    if (err != M5PM1_OK) {
        return err;
    }

    uint8_t data[M5PM1_MAX_LED_COUNT * 2];
    for (uint8_t i = 0; i < count; i++) {
        uint16_t rgb565 =
            ((uint16_t)(colors[i].r & 0xF8) << 8) | ((uint16_t)(colors[i].g & 0xFC) << 3) | (colors[i].b >> 3);
        data[i * 2]     = (uint8_t)(rgb565 & 0xFF);
        data[i * 2 + 1] = (uint8_t)((rgb565 >> 8) & 0xFF);
    }

    if (!_writeBytes(M5PM1_REG_NEO_DATA_START, data, (uint8_t)(count * 2))) {
        M5PM1_LOG_E(TAG_LED, "Failed to write NEO data buffer");
        return M5PM1_ERR_I2C_COMM;
    }

    if (autoRefresh) {
        err = refreshLeds();
        if (err != M5PM1_OK) {
            return err;
        }
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_NEO);
    M5PM1_LOG_I(TAG_LED, "Set %d LEDs successfully%s", count, autoRefresh ? " (refreshed)" : "");
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setLedCount(uint8_t count)
{
    // 验证参数：count 必须在 1-32 范围内（固件最大支持32）
    // Validate parameter: count must be in range 1-32 (firmware max 32)
    if (count == 0 || count > M5PM1_MAX_LED_COUNT) {
        M5PM1_LOG_E(TAG_LED, "LED count %d out of valid range (1-%d)", count, M5PM1_MAX_LED_COUNT);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_LED, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t cfg = count & M5PM1_NEO_CFG_COUNT_MASK;

    // 写入寄存器
    // Write register
    if (!_writeReg(M5PM1_REG_NEO_CFG, cfg)) {
        M5PM1_LOG_E(TAG_LED, "Failed to write NEO_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5PM1_REG_NEO_CFG, &actualCfg)) {
        M5PM1_LOG_E(TAG_LED, "Failed to read back NEO_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    bool countMatch = ((actualCfg & M5PM1_NEO_CFG_COUNT_MASK) == (cfg & M5PM1_NEO_CFG_COUNT_MASK));
    if (!countMatch) {
        M5PM1_LOG_E(TAG_LED, "LED count verification failed: expected=%d, actual=%d", count,
                    actualCfg & M5PM1_NEO_CFG_COUNT_MASK);
        return M5PM1_FAIL;
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_NEO);
    M5PM1_LOG_I(TAG_LED, "LED count set and verified: %d", count);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= M5PM1_MAX_LED_COUNT) {
        M5PM1_LOG_E(TAG_LED, "Invalid LED index: %d", index);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_LED, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 转换为 RGB565
    // Convert to RGB565
    uint16_t rgb565 = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
    uint8_t regAddr = M5PM1_REG_NEO_DATA_START + (index * 2);

    // 写入寄存器
    // Write register
    if (!_writeReg16(regAddr, rgb565)) {
        M5PM1_LOG_E(TAG_LED, "Failed to write NEO data for index %d", index);
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint16_t actualRgb565 = 0;
    if (!_readReg16(regAddr, &actualRgb565)) {
        M5PM1_LOG_E(TAG_LED, "Failed to read back NEO data for index %d", index);
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    if (actualRgb565 != rgb565) {
        M5PM1_LOG_E(TAG_LED, "LED color verification failed for index %d: expected=0x%04X, actual=0x%04X", index,
                    rgb565, actualRgb565);
        return M5PM1_FAIL;
    }

    M5PM1_LOG_I(TAG_LED, "LED color set and verified for index %d: RGB(%d,%d,%d)", index, r, g, b);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setLedColor(uint8_t index, m5pm1_rgb_t color)
{
    return setLedColor(index, color.r, color.g, color.b);
}

m5pm1_err_t M5PM1::refreshLeds()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_LED, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5PM1_REG_NEO_CFG, &cfg)) {
        M5PM1_LOG_E(TAG_LED, "Failed to read NEO_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    cfg |= M5PM1_NEO_CFG_REFRESH;
    if (!_writeReg(M5PM1_REG_NEO_CFG, cfg)) {
        M5PM1_LOG_E(TAG_LED, "Failed to write NEO_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    return M5PM1_OK;
}

m5pm1_err_t M5PM1::disableLeds()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_LED, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 写入寄存器
    // Write register
    if (!_writeReg(M5PM1_REG_NEO_CFG, 0)) {
        M5PM1_LOG_E(TAG_LED, "Failed to write NEO_CFG register for disable");
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5PM1_REG_NEO_CFG, &actualCfg)) {
        M5PM1_LOG_E(TAG_LED, "Failed to read back NEO_CFG register for disable");
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    if (actualCfg != 0) {
        M5PM1_LOG_E(TAG_LED, "LED disable verification failed: expected=0, actual=0x%02X", actualCfg);
        return M5PM1_FAIL;
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_NEO);
    M5PM1_LOG_I(TAG_LED, "LEDs disabled and verified");
    return M5PM1_OK;
}

// ============================
// AW8737A 脉冲功能
// AW8737A Pulse Functions
// ============================

m5pm1_err_t M5PM1::setAw8737aPulse(m5pm1_gpio_num_t pin, m5pm1_aw8737a_pulse_t num, m5pm1_aw8737a_refresh_t refresh)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_AMP, "Device not initialized");
        return M5PM1_ERR_NOT_INIT;
    }
    if (!_isValidPin(pin)) {
        M5PM1_LOG_E(TAG_AMP, "Invalid pin number: %d", pin);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (num > M5PM1_AW8737A_PULSE_3) {
        M5PM1_LOG_E(TAG_AMP, "Invalid pulse number: %d", num);
        return M5PM1_ERR_INVALID_ARG;
    }

    // 检查引脚是否为输出模式，如果不是则自动配置为推挽输出
    // Check if pin is output mode, if not auto-configure as push-pull output
    if (!_cacheValid || _pinStatus[pin].mode != M5PM1_GPIO_MODE_OUTPUT) {
        M5PM1_LOG_I(TAG_AMP, "AW8737A: Pin %d not output mode (or cache invalid), auto-configuring as push-pull output",
                    pin);
        pinMode(pin, OUTPUT);
    }

    // 保存配置到成员变量
    // Save configuration to member variables
    _aw8737aConfigured = true;
    _aw8737aPin        = pin;
    _aw8737aPulseNum   = num;

    // 构建寄存器值（不含 REFRESH 位）
    // Build register value (without REFRESH bit)
    uint8_t regValue = (uint8_t)pin & 0x1F;
    regValue |= ((uint8_t)num << 5);

    // 写入寄存器（不含 REFRESH 位）
    // Write register (without REFRESH bit)
    if (!_writeReg(M5PM1_REG_AW8737A_PULSE, regValue)) {
        M5PM1_LOG_E(TAG_AMP, "Failed to set AW8737A pulse config");
        return M5PM1_ERR_I2C_COMM;
    }

    // 更新缓存
    // Update cache
    _aw8737aRegValue   = regValue;
    _aw8737aStateValid = true;

    // 回读验证
    // Read-back verification
    uint8_t actualReg = 0;
    if (!_readReg(M5PM1_REG_AW8737A_PULSE, &actualReg)) {
        M5PM1_LOG_E(TAG_AMP, "Failed to read back AW8737A pulse register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    uint8_t actualValue = actualReg & 0x7F;
    if (actualValue != regValue) {
        M5PM1_LOG_E(TAG_AMP, "AW8737A pulse verification failed: expected=0x%02X, actual=0x%02X", regValue,
                    actualValue);
        return M5PM1_FAIL;
    }

    M5PM1_LOG_I(TAG_AMP, "AW8737A pulse configured: pin=%d, num=%d (reg=0x%02X)", pin, num, regValue);

    // 如果需要立即刷新，调用 refreshAw8737aPulse
    // If immediate refresh needed, call refreshAw8737aPulse
    if (refresh == M5PM1_AW8737A_REFRESH_NOW) {
        return refreshAw8737aPulse();
    }

    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_AW8737A);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::refreshAw8737aPulse()
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_AMP, "Device not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    // 检查是否已配置
    // Check if configured
    if (!_aw8737aConfigured) {
        M5PM1_LOG_W(TAG_AMP, "AW8737A pulse not configured, executing anyway");
    }

    // 检查引脚是否为输出模式，如果不是则自动配置为推挽输出
    // Check if pin is output mode, if not auto-configure as push-pull output
    if (!_cacheValid || _pinStatus[_aw8737aPin].mode != M5PM1_GPIO_MODE_OUTPUT) {
        M5PM1_LOG_I(TAG_AMP, "AW8737A: Pin %d not output mode (or cache invalid), auto-configuring as push-pull output",
                    _aw8737aPin);
        pinMode(_aw8737aPin, OUTPUT);
    }

    // 读取当前寄存器值
    // Read current register value
    uint8_t regValue = 0;
    if (!_readReg(M5PM1_REG_AW8737A_PULSE, &regValue)) {
        M5PM1_LOG_E(TAG_AMP, "Failed to read AW8737A pulse register");
        return M5PM1_ERR_I2C_COMM;
    }

    // 设置 REFRESH 位
    // Set REFRESH bit
    regValue |= 0x80;
    if (!_writeReg(M5PM1_REG_AW8737A_PULSE, regValue)) {
        M5PM1_LOG_E(TAG_AMP, "Failed to refresh AW8737A pulse");
        return M5PM1_ERR_I2C_COMM;
    }

    // 输出详细日志
    // Output detailed log
    const char* driveStr = (_pinStatus[_aw8737aPin].drive == M5PM1_GPIO_DRIVE_PUSHPULL) ? "PUSH-PULL" : "OPEN-DRAIN";
    M5PM1_LOG_I(TAG_AMP, "AW8737A pulse refresh: pin=%d, pulseNum=%d, mode=OUTPUT, drive=%s", _aw8737aPin,
                _aw8737aPulseNum, driveStr);

    M5PM1_DELAY_MS(20);
    _autoSnapshotUpdate(M5PM1_SNAPSHOT_DOMAIN_AW8737A);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setAw8737aMode(m5pm1_gpio_num_t pin, m5pm1_aw8737a_mode_t mode, m5pm1_aw8737a_refresh_t refresh)
{
    // Mode 直接映射到 Pulse (MODE_1=0pulse, MODE_2=1pulse, MODE_3=2pulses, MODE_4=3pulses)
    // Mode directly maps to Pulse
    if (mode > M5PM1_AW8737A_MODE_4) {
        M5PM1_LOG_E(TAG_AMP, "Invalid AW8737A mode: %d (valid range: 0-3)", mode);
        return M5PM1_ERR_INVALID_ARG;
    }

    return setAw8737aPulse(pin, (m5pm1_aw8737a_pulse_t)mode, refresh);
}

m5pm1_err_t M5PM1::refreshAw8737aMode()
{
    return refreshAw8737aPulse();
}

// ============================
// RTC RAM 功能
// RTC RAM Functions
// ============================

m5pm1_err_t M5PM1::writeRtcRAM(uint8_t offset, const uint8_t* data, uint8_t len)
{
    if (data == nullptr || offset >= M5PM1_RTC_RAM_SIZE || len == 0 || (offset + len) > M5PM1_RTC_RAM_SIZE) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regAddr = M5PM1_REG_RTC_RAM_START + offset;

    // 写入寄存器
    // Write register
    if (!_writeBytes(regAddr, data, len)) {
        M5PM1_LOG_E(TAG_SYS, "Failed to write RTC_RAM register at offset %d", offset);
        return M5PM1_ERR_I2C_COMM;
    }

    // 回读验证
    // Read-back verification
    uint8_t actualData[M5PM1_RTC_RAM_SIZE];
    if (!_readBytes(regAddr, actualData, len)) {
        M5PM1_LOG_E(TAG_SYS, "Failed to read back RTC_RAM register at offset %d", offset);
        return M5PM1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    for (uint8_t i = 0; i < len; i++) {
        if (actualData[i] != data[i]) {
            M5PM1_LOG_E(TAG_SYS, "RTC_RAM verification failed at offset %d: expected=0x%02X, actual=0x%02X", offset + i,
                        data[i], actualData[i]);
            return M5PM1_FAIL;
        }
    }

    M5PM1_LOG_I(TAG_SYS, "RTC_RAM write and verified: offset=%d, length=%d", offset, len);
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::readRtcRAM(uint8_t offset, uint8_t* data, uint8_t len)
{
    if (data == nullptr || offset >= M5PM1_RTC_RAM_SIZE || len == 0 || (offset + len) > M5PM1_RTC_RAM_SIZE) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t regAddr = M5PM1_REG_RTC_RAM_START + offset;
    return _readBytes(regAddr, data, len) ? M5PM1_OK : M5PM1_ERR_I2C_COMM;
}

// ============================
// I2C 配置
// I2C Configuration
// ============================

m5pm1_err_t M5PM1::setI2cConfig(uint8_t sleepTime, m5pm1_i2c_speed_t speed)
{
    if (sleepTime > 15) {
        M5PM1_LOG_E(TAG_I2C, "Invalid I2C sleep time: %u", sleepTime);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_I2C, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint32_t targetFreq = (speed == M5PM1_I2C_SPEED_400K) ? M5PM1_I2C_FREQ_400K : M5PM1_I2C_FREQ_100K;
    bool speedChanged   = (targetFreq != _requestedSpeed);

    uint8_t cfg = (sleepTime & M5PM1_I2C_CFG_SLEEP_MASK);
    if (speed == M5PM1_I2C_SPEED_400K) {
        cfg |= M5PM1_I2C_CFG_SPEED_400K;
    }

    // Step 1: Write register
    if (!_writeReg(M5PM1_REG_I2C_CFG, cfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to write I2C_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &actualCfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to read back I2C_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    uint8_t expectedSleep = sleepTime & M5PM1_I2C_CFG_SLEEP_MASK;
    uint8_t actualSleep   = actualCfg & M5PM1_I2C_CFG_SLEEP_MASK;
    bool actualSpeed400k  = (actualCfg & M5PM1_I2C_CFG_SPEED_400K) != 0;

    if (actualSleep != expectedSleep || actualSpeed400k != (speed == M5PM1_I2C_SPEED_400K)) {
        M5PM1_LOG_E(TAG_I2C, "I2C_CFG verification failed: expected=0x%02X, actual=0x%02X", cfg, actualCfg);
        return M5PM1_FAIL;
    }

    if (speedChanged) {
        m5pm1_err_t ret = switchI2cSpeed(speed);
        if (ret != M5PM1_OK) {
            return ret;
        }
    }

    _i2cConfig.sleepTime = sleepTime;
    _i2cConfig.speed400k = (speed == M5PM1_I2C_SPEED_400K);
    _i2cConfigValid      = true;
    _i2cSleepTime        = sleepTime;

    if (sleepTime > 0 && !_autoWakeEnabled) {
        setAutoWakeEnable(true);
        M5PM1_LOG_W(TAG_I2C, "I2C sleep enabled, auto-wake automatically enabled");
    }

    M5PM1_LOG_I(TAG_I2C, "I2C config set and verified: sleep=%u, speed=%s", sleepTime,
                speed == M5PM1_I2C_SPEED_400K ? "400K" : "100K");
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getI2cSpeed(m5pm1_i2c_speed_t* speed)
{
    if (speed == nullptr) {
        M5PM1_LOG_E(TAG_I2C, "getI2cSpeed speed is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_I2C, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &cfg)) {
        return M5PM1_ERR_I2C_COMM;
    }

    _i2cConfig.speed400k = (cfg & M5PM1_I2C_CFG_SPEED_400K) != 0;
    _i2cConfigValid      = true;

    *speed = _i2cConfig.speed400k ? M5PM1_I2C_SPEED_400K : M5PM1_I2C_SPEED_100K;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::setI2cSleepTime(uint8_t seconds)
{
    if (seconds > 15) {
        M5PM1_LOG_E(TAG_I2C, "Invalid I2C sleep time: %u", seconds);
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_I2C, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &cfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    cfg = (cfg & ~M5PM1_I2C_CFG_SLEEP_MASK) | (seconds & M5PM1_I2C_CFG_SLEEP_MASK);

    if (!_writeReg(M5PM1_REG_I2C_CFG, cfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to write I2C_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    uint8_t actualCfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &actualCfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to read back I2C_CFG register");
        return M5PM1_ERR_I2C_COMM;
    }

    uint8_t actualSleep = actualCfg & M5PM1_I2C_CFG_SLEEP_MASK;
    if (actualSleep != seconds) {
        M5PM1_LOG_E(TAG_I2C, "I2C_CFG sleep time verification failed: expected=%u, actual=%u", seconds, actualSleep);
        return M5PM1_FAIL;
    }

    _i2cConfig.sleepTime = seconds;
    _i2cConfigValid      = true;
    _i2cSleepTime        = seconds;

    M5PM1_LOG_I(TAG_I2C, "I2C sleep time set and verified to %u", seconds);
    if (seconds > 0 && !_autoWakeEnabled) {
        setAutoWakeEnable(true);
        M5PM1_LOG_W(TAG_I2C, "I2C sleep enabled, auto-wake automatically enabled");
    }
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getI2cSleepTime(uint8_t* seconds)
{
    if (seconds == nullptr) {
        M5PM1_LOG_E(TAG_I2C, "getI2cSleepTime seconds is null");
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5PM1_LOG_E(TAG_I2C, "Not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &cfg)) {
        return M5PM1_ERR_I2C_COMM;
    }

    _i2cConfig.sleepTime = cfg & M5PM1_I2C_CFG_SLEEP_MASK;
    _i2cConfigValid      = true;
    _i2cSleepTime        = _i2cConfig.sleepTime;

    *seconds = _i2cConfig.sleepTime;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::switchI2cSpeed(m5pm1_i2c_speed_t speed)
{
    if (!_initialized) {
        M5PM1_LOG_E(TAG_I2C, "Cannot switch I2C speed: device not initialized");
        return M5PM1_ERR_NOT_INIT;
    }

    uint32_t targetFreq = (speed == M5PM1_I2C_SPEED_400K) ? M5PM1_I2C_FREQ_400K : M5PM1_I2C_FREQ_100K;
    if (targetFreq == _requestedSpeed) {
        M5PM1_LOG_I(TAG_I2C, "I2C speed already at %lu Hz, no change needed", (unsigned long)targetFreq);
        return M5PM1_OK;
    }

    uint8_t i2cCfg = 0;
    if (!_readReg(M5PM1_REG_I2C_CFG, &i2cCfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to read I2C config register");
        return M5PM1_ERR_I2C_COMM;
    }

    uint8_t originalCfg   = i2cCfg;
    uint32_t originalFreq = (i2cCfg & M5PM1_I2C_CFG_SPEED_400K) ? M5PM1_I2C_FREQ_400K : M5PM1_I2C_FREQ_100K;

    if (speed == M5PM1_I2C_SPEED_400K) {
        i2cCfg |= M5PM1_I2C_CFG_SPEED_400K;
    } else {
        i2cCfg &= ~M5PM1_I2C_CFG_SPEED_400K;
    }

    if (!_writeReg(M5PM1_REG_I2C_CFG, i2cCfg)) {
        M5PM1_LOG_E(TAG_I2C, "Failed to write I2C config register");
        return M5PM1_ERR_I2C_COMM;
    }

    M5PM1_DELAY_MS(5);

#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
    if (_m5_i2c) {
        _commFreq = targetFreq;
        M5PM1_LOG_D(TAG_I2C, "M5Unified I2C frequency updated to %lu Hz", (unsigned long)targetFreq);
    } else
#endif
    {
        _wire->end();
        M5PM1_DELAY_MS(10);
        if (!_wire->begin(_sda, _scl, targetFreq)) {
            M5PM1_LOG_E(TAG_I2C, "Failed to re-initialize I2C bus at %lu Hz", (unsigned long)targetFreq);
            _wire->begin(_sda, _scl, originalFreq);
            _writeReg(M5PM1_REG_I2C_CFG, originalCfg);
            return M5PM1_ERR_I2C_CONFIG;
        }
        M5PM1_DELAY_MS(10);
    }
#else
    esp_err_t ret;
    switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
        case M5PM1_I2C_DRIVER_SELF_CREATED:
        case M5PM1_I2C_DRIVER_MASTER: {
            if (_i2c_master_dev != nullptr) {
                ret = i2c_master_bus_rm_device(_i2c_master_dev);
                if (ret != ESP_OK) {
                    M5PM1_LOG_E(TAG_I2C, "Failed to remove I2C device: %s", esp_err_to_name(ret));
                    return M5PM1_ERR_I2C_CONFIG;
                }
                _i2c_master_dev = nullptr;

                i2c_device_config_t dev_config = {
                    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                    .device_address  = _addr,
                    .scl_speed_hz    = targetFreq,
                    .scl_wait_us     = 0,
                    .flags =
                        {
                            .disable_ack_check = false,
                        },
                };

                ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
                if (ret != ESP_OK) {
                    M5PM1_LOG_E(TAG_I2C, "Failed to add I2C device at %lu Hz: %s", (unsigned long)targetFreq,
                                esp_err_to_name(ret));
                    dev_config.scl_speed_hz = originalFreq;
                    i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
                    _writeReg(M5PM1_REG_I2C_CFG, originalCfg);
                    return M5PM1_ERR_I2C_CONFIG;
                }
            }
            break;
        }
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
        case M5PM1_I2C_DRIVER_BUS:
            if (_i2c_device != nullptr) {
                ret = i2c_bus_device_delete(&_i2c_device);
                if (ret != ESP_OK) {
                    M5PM1_LOG_E(TAG_I2C, "Failed to delete I2C device: %s", esp_err_to_name(ret));
                    return M5PM1_ERR_I2C_CONFIG;
                }
                _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, targetFreq);
                if (_i2c_device == nullptr) {
                    M5PM1_LOG_E(TAG_I2C, "Failed to create I2C device at %lu Hz", (unsigned long)targetFreq);
                    _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, originalFreq);
                    _writeReg(M5PM1_REG_I2C_CFG, originalCfg);
                    return M5PM1_ERR_I2C_CONFIG;
                }
            }
            break;
#endif  // M5PM1_HAS_I2C_BUS
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
        case M5PM1_I2C_DRIVER_LEGACY: {
            i2c_config_t conf     = {};
            conf.mode             = I2C_MODE_MASTER;
            conf.sda_io_num       = _sda;
            conf.scl_io_num       = _scl;
            conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
            conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
            conf.master.clk_speed = targetFreq;
            conf.clk_flags        = 0;
            ret                   = i2c_param_config(_port, &conf);
            if (ret != ESP_OK) {
                M5PM1_LOG_E(TAG_I2C, "i2c_param_config failed: %s", esp_err_to_name(ret));
                return M5PM1_ERR_I2C_CONFIG;
            }
            M5PM1_LOG_D(TAG_I2C, "Legacy I2C reconfigured to %lu Hz", (unsigned long)targetFreq);
            break;
        }
#endif  // !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
#if M5PM1_HAS_M5UNIFIED_I2C
        case M5PM1_I2C_DRIVER_M5UNIFIED:
            _commFreq = targetFreq;
            M5PM1_LOG_D(TAG_I2C, "M5Unified I2C frequency updated to %lu Hz", (unsigned long)targetFreq);
            break;
#endif
        default:
            M5PM1_LOG_E(TAG_I2C, "Unknown I2C driver type");
            return M5PM1_ERR_INTERNAL;
    }
#endif

    uint8_t id = 0;
    if (!_readReg(M5PM1_REG_DEVICE_ID, &id)) {
        M5PM1_LOG_E(TAG_I2C, "Communication failed after switching to %lu Hz, reverting", (unsigned long)targetFreq);

#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            _commFreq = originalFreq;
        } else
#endif
        {
            _wire->end();
            M5PM1_DELAY_MS(10);
            _wire->begin(_sda, _scl, originalFreq);
            M5PM1_DELAY_MS(10);
        }
#else
        switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
            case M5PM1_I2C_DRIVER_SELF_CREATED:
            case M5PM1_I2C_DRIVER_MASTER: {
                if (_i2c_master_dev != nullptr) {
                    i2c_master_bus_rm_device(_i2c_master_dev);
                    i2c_device_config_t dev_config = {
                        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                        .device_address  = _addr,
                        .scl_speed_hz    = originalFreq,
                        .scl_wait_us     = 0,
                        .flags           = {.disable_ack_check = false},
                    };
                    i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
                }
                break;
            }
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_BUS:
                if (_i2c_device != nullptr) {
                    i2c_bus_device_delete(&_i2c_device);
                    _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, originalFreq);
                }
                break;
#endif  // M5PM1_HAS_I2C_BUS
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
            case M5PM1_I2C_DRIVER_LEGACY: {
                i2c_config_t conf     = {};
                conf.mode             = I2C_MODE_MASTER;
                conf.sda_io_num       = _sda;
                conf.scl_io_num       = _scl;
                conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
                conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
                conf.master.clk_speed = originalFreq;
                conf.clk_flags        = 0;
                i2c_param_config(_port, &conf);  // best effort rollback
                break;
            }
#endif  // !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
#if M5PM1_HAS_M5UNIFIED_I2C
            case M5PM1_I2C_DRIVER_M5UNIFIED:
                _commFreq = originalFreq;
                break;
#endif
            default:
                break;
        }
#endif
        _writeReg(M5PM1_REG_I2C_CFG, originalCfg);
        return M5PM1_ERR_I2C_COMM;
    }

    _requestedSpeed = targetFreq;
    _lastCommTime   = M5PM1_GET_TIME_MS();

    _i2cConfig.speed400k = (speed == M5PM1_I2C_SPEED_400K);
    _i2cConfigValid      = true;

    M5PM1_LOG_I(TAG_I2C, "Successfully switched to %lu Hz I2C mode", (unsigned long)targetFreq);
    return M5PM1_OK;
}

// ============================
// 自动唤醒功能
// Auto Wake Feature
// ============================

void M5PM1::setAutoWakeEnable(bool enable)
{
    _autoWakeEnabled = enable;
    if (enable) {
        _lastCommTime = M5PM1_GET_TIME_MS();
    }
}

bool M5PM1::isAutoWakeEnabled() const
{
    return _autoWakeEnabled;
}

m5pm1_err_t M5PM1::sendWakeSignal()
{
#ifdef ARDUINO
#if M5PM1_HAS_M5UNIFIED_I2C
    if (_m5_i2c) {
        M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
        return M5PM1_OK;
    }
#endif
    M5PM1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
    return M5PM1_OK;
#else
    switch (_i2cDriverType) {
#if M5PM1_HAS_I2C_MASTER
        case M5PM1_I2C_DRIVER_SELF_CREATED:
        case M5PM1_I2C_DRIVER_MASTER:
            return M5PM1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr) == ESP_OK ? M5PM1_OK : M5PM1_ERR_I2C_COMM;
#endif  // M5PM1_HAS_I2C_MASTER
#if M5PM1_HAS_I2C_BUS
        case M5PM1_I2C_DRIVER_BUS:
            return M5PM1_I2C_BUS_SEND_WAKE(_i2c_device, M5PM1_REG_HW_REV) == ESP_OK ? M5PM1_OK : M5PM1_ERR_I2C_COMM;
#endif
#if !M5PM1_HAS_I2C_MASTER && !M5PM1_HAS_I2C_BUS
        case M5PM1_I2C_DRIVER_LEGACY:
            return M5PM1_I2C_LEGACY_SEND_WAKE(_port, _addr);
#endif
#if M5PM1_HAS_M5UNIFIED_I2C
        case M5PM1_I2C_DRIVER_M5UNIFIED:
            return M5PM1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq) ? M5PM1_OK : M5PM1_ERR_I2C_COMM;
#endif
        default:
            return M5PM1_ERR_INTERNAL;
    }
#endif
}

// ============================
// 状态快照功能
// State Snapshot Functions
// ============================

void M5PM1::setAutoSnapshot(bool enable)
{
    _autoSnapshot = enable;
}

bool M5PM1::isAutoSnapshotEnabled() const
{
    return _autoSnapshot;
}

m5pm1_err_t M5PM1::updateSnapshot()
{
    if (!_initialized) return M5PM1_ERR_NOT_INIT;
    return _snapshotAll() ? M5PM1_OK : M5PM1_ERR_I2C_COMM;
}

// ============================
// 快照验证
// Snapshot Verification
// ============================

m5pm1_snapshot_verify_t M5PM1::verifySnapshot()
{
    m5pm1_snapshot_verify_t result = {};
    result.consistent              = true;

    if (!_initialized) {
        result.consistent = false;
        return result;
    }

    if (_cacheValid) {
        uint8_t actualMode = 0;
        uint8_t actualOut  = 0;

        if (_readReg(M5PM1_REG_GPIO_MODE, &actualMode) && _readReg(M5PM1_REG_GPIO_OUT, &actualOut)) {
            uint8_t expectedMode = 0;
            uint8_t expectedOut  = 0;
            for (uint8_t i = 0; i < M5PM1_MAX_GPIO_PINS; i++) {
                if (_pinStatus[i].mode == M5PM1_GPIO_MODE_OUTPUT) {
                    expectedMode |= (1 << i);
                }
                if (_pinStatus[i].output) {
                    expectedOut |= (1 << i);
                }
            }

            result.expected_gpio_mode = expectedMode;
            result.actual_gpio_mode   = actualMode & 0x1F;
            result.expected_gpio_out  = expectedOut;
            result.actual_gpio_out    = actualOut & 0x1F;

            if (result.expected_gpio_mode != result.actual_gpio_mode ||
                result.expected_gpio_out != result.actual_gpio_out) {
                result.gpio_mismatch = true;
                result.consistent    = false;
            }
        } else {
            result.consistent = false;
        }
    }

    if (_pwmStatesValid) {
        uint16_t actualFreq = 0;
        if (_readReg16(M5PM1_REG_PWM_FREQ_L, &actualFreq)) {
            if (actualFreq != _pwmFrequency) {
                result.pwm_mismatch = true;
                result.consistent   = false;
            }
        } else {
            result.consistent = false;
        }

        for (uint8_t ch = 0; ch < M5PM1_MAX_PWM_CHANNELS; ch++) {
            uint8_t regL = (ch == 0) ? M5PM1_REG_PWM0_L : M5PM1_REG_PWM1_L;
            uint8_t regH = (ch == 0) ? M5PM1_REG_PWM0_HC : M5PM1_REG_PWM1_HC;
            uint8_t low  = 0;
            uint8_t high = 0;

            if (!_readReg(regL, &low) || !_readReg(regH, &high)) {
                result.consistent = false;
                continue;
            }

            uint16_t actualDuty = (uint16_t)(low | ((high & 0x0F) << 8));
            bool actualEnabled  = (high & 0x10) != 0;
            bool actualPolarity = (high & 0x20) != 0;

            if (actualDuty != _pwmStates[ch].duty12 || actualEnabled != _pwmStates[ch].enabled ||
                actualPolarity != _pwmStates[ch].polarity) {
                result.pwm_mismatch = true;
                result.consistent   = false;
            }
        }
    }

    if (_adcStateValid) {
        uint8_t ctrl   = 0;
        uint16_t value = 0;
        if (_readReg(M5PM1_REG_ADC_CTRL, &ctrl) && _readReg16(M5PM1_REG_ADC_RES_L, &value)) {
            uint8_t actualChannel = (ctrl >> 1) & 0x07;
            bool actualBusy       = (ctrl & 0x01) != 0;

            // 仅比较 channel 和 busy 状态，不比较 lastValue（ADC 活跃时值持续变化会导致误报）
            // Only compare channel and busy status, not lastValue (active ADC causes false positives)
            if (actualChannel != _adcState.channel || actualBusy != _adcState.busy) {
                result.adc_mismatch = true;
                result.consistent   = false;
            }
        } else {
            result.consistent = false;
        }
    }

    if (_powerConfigValid) {
        uint8_t actualPwr  = 0;
        uint8_t actualHold = 0;
        if (_readReg(M5PM1_REG_PWR_CFG, &actualPwr) && _readReg(M5PM1_REG_HOLD_CFG, &actualHold)) {
            result.expected_pwr_cfg  = _powerCfg;
            result.actual_pwr_cfg    = actualPwr;
            result.expected_hold_cfg = _holdCfg;
            result.actual_hold_cfg   = actualHold;

            if (_powerCfg != actualPwr || _holdCfg != actualHold) {
                result.power_mismatch = true;
                result.consistent     = false;
            }
        } else {
            result.consistent = false;
        }
    }

    if (_btnConfigValid) {
        uint8_t actualCfg1 = 0;
        uint8_t actualCfg2 = 0;
        if (_readReg(M5PM1_REG_BTN_CFG_1, &actualCfg1) && _readReg(M5PM1_REG_BTN_CFG_2, &actualCfg2)) {
            if (_btnCfg1 != actualCfg1 || _btnCfg2 != actualCfg2) {
                result.button_mismatch = true;
                result.consistent      = false;
            }
        } else {
            result.consistent = false;
        }
    }

    if (_irqMaskValid) {
        uint8_t actualMask1 = 0;
        uint8_t actualMask2 = 0;
        uint8_t actualMask3 = 0;
        if (_readReg(M5PM1_REG_IRQ_MASK1, &actualMask1) && _readReg(M5PM1_REG_IRQ_MASK2, &actualMask2) &&
            _readReg(M5PM1_REG_IRQ_MASK3, &actualMask3)) {
            if (_irqMask1 != actualMask1 || _irqMask2 != actualMask2 || _irqMask3 != actualMask3) {
                result.irq_mask_mismatch = true;
                result.consistent        = false;
            }
        } else {
            result.consistent = false;
        }
    }

    if (_i2cConfigValid) {
        uint8_t actualCfg = 0;
        if (_readReg(M5PM1_REG_I2C_CFG, &actualCfg)) {
            uint8_t expectedCfg = _i2cConfig.sleepTime & M5PM1_I2C_CFG_SLEEP_MASK;
            if (_i2cConfig.speed400k) {
                expectedCfg |= M5PM1_I2C_CFG_SPEED_400K;
            }
            uint8_t actualMasked = actualCfg & (M5PM1_I2C_CFG_SLEEP_MASK | M5PM1_I2C_CFG_SPEED_400K);

            result.expected_i2c_cfg = expectedCfg;
            result.actual_i2c_cfg   = actualMasked;

            if (expectedCfg != actualMasked) {
                result.i2c_mismatch = true;
                result.consistent   = false;
            }
        } else {
            result.consistent = false;
        }
    }

    if (_neoConfigValid) {
        uint8_t actualCfg = 0;
        if (_readReg(M5PM1_REG_NEO_CFG, &actualCfg)) {
            uint8_t expectedCfg  = _neoCfg & M5PM1_NEO_CFG_COUNT_MASK;
            uint8_t actualMasked = actualCfg & M5PM1_NEO_CFG_COUNT_MASK;

            result.expected_neo_cfg = expectedCfg;
            result.actual_neo_cfg   = actualMasked;

            if (expectedCfg != actualMasked) {
                result.neo_mismatch = true;
                result.consistent   = false;
            }
        } else {
            result.consistent = false;
        }
    }

    // AW8737A 验证
    // AW8737A verification
    if (_aw8737aStateValid) {
        uint8_t actualReg = 0;
        if (_readReg(M5PM1_REG_AW8737A_PULSE, &actualReg)) {
            result.expected_aw8737a = _aw8737aRegValue;
            result.actual_aw8737a   = actualReg & 0x7F;

            if (result.expected_aw8737a != result.actual_aw8737a) {
                result.aw8737a_mismatch = true;
                result.consistent       = false;
            }
        } else {
            result.consistent = false;
        }
    }

    return result;
}

// ============================
// 配置验证
// Configuration Validation
// ============================

m5pm1_validation_t M5PM1::validateConfig(uint8_t pin, m5pm1_config_type_t configType, bool enable)
{
    m5pm1_validation_t result = {false, {0}, 0xFF};

    if (!_isValidPin(pin)) {
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid pin %d", pin);
        return result;
    }

    if (!_initialized) {
        snprintf(result.error_msg, sizeof(result.error_msg), "Not initialized");
        return result;
    }

    if (!enable) {
        result.valid = true;
        return result;
    }

    switch (configType) {
        case M5PM1_CONFIG_GPIO_INPUT:
        case M5PM1_CONFIG_GPIO_OUTPUT:
            if (_hasActivePwm(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for PWM", pin);
                return result;
            }
            if (_hasActiveAdc(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for ADC", pin);
                return result;
            }
            if (_hasActiveNeo(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for LED_EN", pin);
                return result;
            }
            if (_hasActiveIrq(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has IRQ enabled", pin);
                return result;
            }
            if (_hasActiveWake(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has WAKE enabled", pin);
                return result;
            }
            break;

        case M5PM1_CONFIG_GPIO_INTERRUPT:
            if (_hasActivePwm(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for PWM", pin);
                return result;
            }
            if (_hasActiveAdc(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for ADC", pin);
                return result;
            }
            if (_hasActiveNeo(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for LED_EN", pin);
                return result;
            }
            if (_hasActiveWake(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has WAKE enabled", pin);
                return result;
            }
            break;

        case M5PM1_CONFIG_GPIO_WAKE:
            // GPIO1 不支持 WAKE（与 SDA 共用中断线）
            // GPIO1 does not support WAKE (shares interrupt line with SDA)
            if (pin == M5PM1_GPIO_NUM_1) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO1 does not support WAKE");
                return result;
            }
            // GPIO0/GPIO2 WAKE 互斥（共用中断线）
            // GPIO0/GPIO2 WAKE mutual exclusion (share interrupt line)
            if (pin == M5PM1_GPIO_NUM_0 && _hasActiveWake(M5PM1_GPIO_NUM_2)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO0/GPIO2 WAKE conflict");
                result.conflicting_pin = M5PM1_GPIO_NUM_2;
                return result;
            }
            if (pin == M5PM1_GPIO_NUM_2 && _hasActiveWake(M5PM1_GPIO_NUM_0)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO0/GPIO2 WAKE conflict");
                result.conflicting_pin = M5PM1_GPIO_NUM_0;
                return result;
            }
            // GPIO3/GPIO4 WAKE 互斥（共用中断线）
            // GPIO3/GPIO4 WAKE mutual exclusion (share interrupt line)
            if (pin == M5PM1_GPIO_NUM_3 && _hasActiveWake(M5PM1_GPIO_NUM_4)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO3/GPIO4 WAKE conflict");
                result.conflicting_pin = M5PM1_GPIO_NUM_4;
                return result;
            }
            if (pin == M5PM1_GPIO_NUM_4 && _hasActiveWake(M5PM1_GPIO_NUM_3)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO3/GPIO4 WAKE conflict");
                result.conflicting_pin = M5PM1_GPIO_NUM_3;
                return result;
            }
            if (_hasActivePwm(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for PWM", pin);
                return result;
            }
            if (_hasActiveAdc(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for ADC", pin);
                return result;
            }
            if (_hasActiveNeo(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is used for LED_EN", pin);
                return result;
            }
            if (_hasActiveIrq(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has IRQ enabled", pin);
                return result;
            }
            break;

        case M5PM1_CONFIG_ADC:
            if (!_isAdcPin(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d does not support ADC", pin);
                return result;
            }
            if (_cacheValid && _pinStatus[pin].mode == M5PM1_GPIO_MODE_OUTPUT) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is configured as output", pin);
                return result;
            }
            if (_hasActiveIrq(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has IRQ enabled", pin);
                return result;
            }
            if (_hasActiveWake(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has WAKE enabled", pin);
                return result;
            }
            break;

        case M5PM1_CONFIG_PWM:
            if (!_isPwmPin(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d does not support PWM", pin);
                return result;
            }
            if (_hasActiveIrq(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has IRQ enabled", pin);
                return result;
            }
            if (_hasActiveWake(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d has WAKE enabled", pin);
                return result;
            }
            break;

        case M5PM1_CONFIG_NEOPIXEL:
            if (!_isNeoPin(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "NeoPixel only supported on GPIO0");
                return result;
            }
            if (_hasActiveIrq(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO0 has IRQ enabled");
                return result;
            }
            if (_hasActiveWake(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "GPIO0 has WAKE enabled");
                return result;
            }
            break;

        default:
            snprintf(result.error_msg, sizeof(result.error_msg), "Unknown config type");
            return result;
    }

    result.valid = true;
    return result;
}

// ============================
// 缓存状态查询函数
// Cached State Query Functions
// ============================

m5pm1_err_t M5PM1::getCachedPwmFrequency(uint16_t* frequency)
{
    if (frequency == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_pwmStatesValid) return M5PM1_FAIL;
    *frequency = _pwmFrequency;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getCachedPwmState(m5pm1_pwm_channel_t channel, uint16_t* duty12, bool* enable, bool* polarity)
{
    if (channel > M5PM1_PWM_CH_1 || duty12 == nullptr || enable == nullptr || polarity == nullptr) {
        return M5PM1_ERR_INVALID_ARG;
    }
    if (!_pwmStatesValid) return M5PM1_FAIL;

    *duty12   = _pwmStates[channel].duty12;
    *enable   = _pwmStates[channel].enabled;
    *polarity = _pwmStates[channel].polarity;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getCachedAdcState(m5pm1_adc_channel_t* channel, bool* busy, uint16_t* lastValue)
{
    if (channel == nullptr || busy == nullptr || lastValue == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_adcStateValid) return M5PM1_FAIL;

    *channel   = (m5pm1_adc_channel_t)_adcState.channel;
    *busy      = _adcState.busy;
    *lastValue = _adcState.lastValue;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getCachedPowerConfig(uint8_t* pwrCfg, uint8_t* holdCfg)
{
    if (pwrCfg == nullptr || holdCfg == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_powerConfigValid) return M5PM1_FAIL;

    *pwrCfg  = _powerCfg;
    *holdCfg = _holdCfg;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getCachedButtonConfig(uint8_t* cfg1, uint8_t* cfg2)
{
    if (cfg1 == nullptr || cfg2 == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_btnConfigValid) return M5PM1_FAIL;

    *cfg1 = _btnCfg1;
    *cfg2 = _btnCfg2;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getCachedIrqMasks(uint8_t* mask1, uint8_t* mask2, uint8_t* mask3)
{
    if (mask1 == nullptr || mask2 == nullptr || mask3 == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_irqMaskValid) return M5PM1_FAIL;

    *mask1 = _irqMask1;
    *mask2 = _irqMask2;
    *mask3 = _irqMask3;
    return M5PM1_OK;
}

m5pm1_err_t M5PM1::getCachedIrqStatus(uint8_t* status1, uint8_t* status2, uint8_t* status3)
{
    if (status1 == nullptr || status2 == nullptr || status3 == nullptr) return M5PM1_ERR_INVALID_ARG;
    if (!_irqStatusValid) return M5PM1_FAIL;

    *status1 = _irqStatus1;
    *status2 = _irqStatus2;
    *status3 = _irqStatus3;
    return M5PM1_OK;
}
