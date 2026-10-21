/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "M5IOE1.h"
#include <string.h>

static const char* TAG      = "M5IOE1";
static const char* TAG_I2C  = "M5IOE1_I2C";
static const char* TAG_GPIO = "M5IOE1_GPIO";
static const char* TAG_ADC  = "M5IOE1_ADC";
static const char* TAG_PWM  = "M5IOE1_PWM";
static const char* TAG_LED  = "M5IOE1_LED";
static const char* TAG_AMP  = "M5IOE1_AMP";
static const char* TAG_IRQ  = "M5IOE1_IRQ";
static const char* TAG_SYS  = "M5IOE1_SYS";

#ifdef ARDUINO
#include <Arduino.h>
#define M5IOE1_DELAY_MS(ms)  delay(ms)
#define M5IOE1_GET_TIME_MS() millis()

// Arduino 日志级别控制
// Arduino log level control
static m5ioe1_log_level_t _m5ioe1_log_level = M5IOE1_LOG_LEVEL_INFO;

#define M5IOE1_LOG_I(tag, fmt, ...)                                   \
    do {                                                              \
        if (_m5ioe1_log_level >= M5IOE1_LOG_LEVEL_INFO) {             \
            Serial.printf("[I][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5IOE1_LOG_W(tag, fmt, ...)                                   \
    do {                                                              \
        if (_m5ioe1_log_level >= M5IOE1_LOG_LEVEL_WARN) {             \
            Serial.printf("[W][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5IOE1_LOG_E(tag, fmt, ...)                                   \
    do {                                                              \
        if (_m5ioe1_log_level >= M5IOE1_LOG_LEVEL_ERROR) {            \
            Serial.printf("[E][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5IOE1_LOG_D(tag, fmt, ...)                                   \
    do {                                                              \
        if (_m5ioe1_log_level >= M5IOE1_LOG_LEVEL_DEBUG) {            \
            Serial.printf("[D][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

#define M5IOE1_LOG_V(tag, fmt, ...)                                   \
    do {                                                              \
        if (_m5ioe1_log_level >= M5IOE1_LOG_LEVEL_VERBOSE) {          \
            Serial.printf("[V][%s] " fmt "\r\n", tag, ##__VA_ARGS__); \
        }                                                             \
    } while (0)

// (Arduino 轮询/中断任务句柄已移至实例成员 _pollTask，支持多实例)
// (Arduino polling/interrupt task handles moved to instance member _pollTask for multi-instance support)
#else
// 强制本组件编译所有级别的日志，实际输出由运行时的 esp_log_level_set 控制
// Force compile all log levels for this component; actual output controlled by esp_log_level_set at runtime
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#define M5IOE1_DELAY_MS(ms)         vTaskDelay(pdMS_TO_TICKS(ms))
#define M5IOE1_GET_TIME_MS()        (xTaskGetTickCount() * portTICK_PERIOD_MS)
#define M5IOE1_LOG_I(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define M5IOE1_LOG_W(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define M5IOE1_LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define M5IOE1_LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define M5IOE1_LOG_V(tag, fmt, ...) ESP_LOGV(tag, fmt, ##__VA_ARGS__)

// ESP-IDF 平台日志级别控制
// ESP-IDF platform log level control
static m5ioe1_log_level_t _m5ioe1_current_log_level = M5IOE1_LOG_LEVEL_INFO;
#endif

// ============================
// 全局日志级别控制
// Global Log Level Control
// ============================

void M5IOE1::setLogLevel(m5ioe1_log_level_t level)
{
#ifdef ARDUINO
    _m5ioe1_log_level = level;
#else
    _m5ioe1_current_log_level = level;

    // 将 M5IOE1 日志级别映射到 ESP-IDF 日志级别
    // Map M5IOE1 log level to ESP-IDF log level
    esp_log_level_t esp_level;
    switch (level) {
        case M5IOE1_LOG_LEVEL_NONE:
            esp_level = ESP_LOG_NONE;
            break;
        case M5IOE1_LOG_LEVEL_ERROR:
            esp_level = ESP_LOG_ERROR;
            break;
        case M5IOE1_LOG_LEVEL_WARN:
            esp_level = ESP_LOG_WARN;
            break;
        case M5IOE1_LOG_LEVEL_INFO:
            esp_level = ESP_LOG_INFO;
            break;
        case M5IOE1_LOG_LEVEL_DEBUG:
            esp_level = ESP_LOG_DEBUG;
            break;
        case M5IOE1_LOG_LEVEL_VERBOSE:
            esp_level = ESP_LOG_VERBOSE;
            break;
        default:
            esp_level = ESP_LOG_INFO;
            break;
    }

    esp_log_level_set(TAG, esp_level);
#endif
}

m5ioe1_log_level_t M5IOE1::getLogLevel()
{
#ifdef ARDUINO
    return _m5ioe1_log_level;
#else
    return _m5ioe1_current_log_level;
#endif
}

void M5IOE1::enableDefaultInterruptLog(bool enable)
{
    _enableDefaultIsrLog = enable;
}

// ============================
// 构造函数
// 析构函数
// ============================

M5IOE1::M5IOE1()
{
    _addr                = M5IOE1_DEFAULT_ADDR;
    _initialized         = false;
    _autoSnapshot        = true;
    _enableDefaultIsrLog = false;
    _requestedSpeed      = M5IOE1_I2C_FREQ_DEFAULT;
    _autoWakeEnabled     = false;
    _lastCommTime        = 0;
    _intMode             = M5IOE1_INT_MODE_DISABLED;
    _intPin              = -1;
    _pollingInterval     = 5000;
    _pinStatesValid      = false;
    _pwmStatesValid      = false;
    _pwmFrequency        = 0;
    _adcStateValid       = false;
    _i2cConfigValid      = false;

#ifdef ARDUINO
    _wire = nullptr;
    _sda  = 0;
    _scl  = 0;
#if M5IOE1_HAS_M5UNIFIED_I2C
    _m5_i2c   = nullptr;
    _commFreq = 0;
#endif
#else
    _i2cDriverType = M5IOE1_I2C_DRIVER_NONE;
#if M5IOE1_HAS_I2C_MASTER
    _i2c_master_bus = nullptr;
    _i2c_master_dev = nullptr;
#endif  // M5IOE1_HAS_I2C_MASTER
#if M5IOE1_HAS_I2C_BUS
    _i2c_bus    = nullptr;
    _i2c_device = nullptr;
#endif
    _busExternal = false;
    _sda         = -1;
    _scl         = -1;
    _port        = I2C_NUM_0;
    _intrQueue   = nullptr;
#if M5IOE1_HAS_M5UNIFIED_I2C
    _m5_i2c   = nullptr;
    _commFreq = M5IOE1_I2C_FREQ_100K;
#endif
#endif

    _pollTask = nullptr;

    memset(_callbacks, 0, sizeof(_callbacks));
    _clearPinStates();
    _clearPwmStates();
    _clearAdcState();
    _clearI2cConfig();
    _clearAw8737a();
}

M5IOE1::~M5IOE1()
{
#ifdef ARDUINO
    _cleanupPollingArduino();
    _cleanupHardwareInterruptArduino();
#else
    _cleanupPolling();
    _cleanupHardwareInterrupt();

    // 根据驱动类型进行清理
    // Cleanup based on driver type
    switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
        case M5IOE1_I2C_DRIVER_SELF_CREATED:
            // 自创建：先删除设备，再删除总线
            // Self-created: delete device first, then bus
            if (_i2c_master_dev) {
                i2c_master_bus_rm_device(_i2c_master_dev);
                _i2c_master_dev = nullptr;
            }
            if (_i2c_master_bus) {
                i2c_del_master_bus(_i2c_master_bus);
                _i2c_master_bus = nullptr;
            }
            break;

        case M5IOE1_I2C_DRIVER_MASTER:
            // 外部 i2c_master：总是删除我们创建的设备句柄，但不删除总线
            // External i2c_master: always delete the device handle we created, but not the bus
            if (_i2c_master_dev) {
                i2c_master_bus_rm_device(_i2c_master_dev);
                _i2c_master_dev = nullptr;
            }
            break;
#endif  // M5IOE1_HAS_I2C_MASTER

#if M5IOE1_HAS_I2C_BUS
        case M5IOE1_I2C_DRIVER_BUS:
            // 外部 i2c_bus：总是删除我们创建的设备句柄，但不删除总线
            // External i2c_bus: always delete the device handle we created, but not the bus
            if (_i2c_device) {
                i2c_bus_device_delete(&_i2c_device);
                _i2c_device = nullptr;
            }
            break;
#endif  // M5IOE1_HAS_I2C_BUS

#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
        case M5IOE1_I2C_DRIVER_LEGACY:
            // Legacy I2C：如果是自创建则卸载驱动
            // Legacy I2C: uninstall driver if self-created
            if (!_busExternal) {
                i2c_driver_delete(_port);
            }
            break;
#endif  // !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS

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

m5ioe1_err_t M5IOE1::begin(TwoWire* wire, uint8_t addr, uint8_t sda, uint8_t scl, uint32_t speed, int8_t intPin,
                           m5ioe1_int_mode_t intMode)
{
    _wire   = wire;
    _addr   = addr;
    _sda    = sda;  // 保存 SDA 引脚用于 I2C 重新初始化
    _scl    = scl;  // 保存 SCL 引脚用于 I2C 重新初始化
    _intPin = intPin;

    if (intMode == M5IOE1_INT_MODE_HARDWARE && _intPin < 0) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证 I2C 频率 - M5IOE1 仅支持 100KHz 或 400KHz
    // Validate I2C frequency - M5IOE1 only supports 100KHz or 400KHz
    if (!_isValidI2cFrequency(speed)) {
        M5IOE1_LOG_W(TAG_I2C,
                     "Invalid I2C frequency: %lu Hz. M5IOE1 only supports 100KHz or 400KHz. Falling back to 100KHz.",
                     speed);
        _requestedSpeed = M5IOE1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    // 始终以 100KHz 开始 - M5IOE1 在上电/复位后默认为 100KHz
    // Always start with 100KHz - M5IOE1 defaults to 100KHz after power-on/reset
    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (device default)");

    // 在开始新的 I2C 会话之前结束之前的会话（修复 ESP_ERR_INVALID_STATE）
    // End any previous I2C session before starting new one (fixes ESP_ERR_INVALID_STATE)
    // 注意：ESP32 Arduino Wire 库需要足够的延时
    // Note: ESP32 Arduino Wire library needs sufficient delay
    _wire->end();
    M5IOE1_DELAY_MS(50);

    // 初始化 I2C 总线并检查返回值
    // Initialize I2C bus and check return value
    if (!_wire->begin(sda, scl, M5IOE1_I2C_FREQ_100K)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to initialize I2C bus (SDA=%d, SCL=%d)", sda, scl);
        return M5IOE1_ERR_I2C_CONFIG;
    }

    // 给 I2C 总线时间在初始化后稳定
    // Give the I2C bus time to stabilize after initialization
    M5IOE1_DELAY_MS(100);

    // 尝试唤醒设备 - 发送 I2C START 信号
    // Try to wake up the device - send I2C START signal
    // M5IOE1 可能处于睡眠状态，需要先唤醒
    // M5IOE1 may be in sleep mode, need to wake up first
    M5IOE1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
    M5IOE1_DELAY_MS(10);

    // 步骤 1: 先尝试100K通信
    // Step 1: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 2: 100K失败，等待800ms后再尝试一次100K
        // Step 2: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 3: 100K第二次失败，尝试400K
            // Step 3: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");

            _wire->end();
            M5IOE1_DELAY_MS(50);

            if (!_wire->begin(sda, scl, M5IOE1_I2C_FREQ_400K)) {
                M5IOE1_LOG_E(TAG_I2C, "Failed to initialize I2C bus at 400KHz");
                return M5IOE1_ERR_I2C_CONFIG;
            }
            M5IOE1_DELAY_MS(100);

            M5IOE1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 4: 都失败，初始化失败
                // Step 4: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

    // 步骤 4: 通信成功，设置为已初始化
    // Step 4: Communication succeeded, set as initialized
    _initialized = true;

    // 步骤 5: 强制配置I2C（用户请求的频率 + 关闭休眠）
    // 注意：setI2cConfig 内部会自动切换主机 I2C 总线速度
    // Step 5: Force configure I2C (user requested speed + disable sleep)
    // Note: setI2cConfig will automatically switch host I2C bus speed internally
    m5ioe1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5IOE1_I2C_FREQ_400K) ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;

    // 检查当前速度
    // Check current speed
    m5ioe1_i2c_speed_t currentSpeed;
    if (getI2cSpeed(&currentSpeed) == M5IOE1_OK) {
        if (currentSpeed == targetSpeed) {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed matches user request (%s)",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        } else {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed differs from user request (Current: %s, Requested: %s)",
                         (currentSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        }
    } else {
        M5IOE1_LOG_W(TAG_I2C, "Failed to read current I2C speed");
    }

    // 使用setI2cConfig一次性配置：sleepTime=0, 用户速度, 默认唤醒边沿, 默认上拉
    // Use setI2cConfig to configure at once: sleepTime=0, user speed, default wake edge, default pull
    if (setI2cConfig(0, targetSpeed, M5IOE1_WAKE_EDGE_FALLING, M5IOE1_PULL_ENABLED) != M5IOE1_OK) {
        M5IOE1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤 6: 快照
    // Step 6: Snapshot
    _snapshotPinStates();
    _snapshotPwmStates();
    _snapshotAdcState();
    _snapshotAw8737a();
    _snapshotI2cConfig();
    M5IOE1_LOG_D(TAG_I2C, "Snapshot completed (pins/pwm/adc/aw8737a/i2c)");

    M5IOE1_LOG_I(TAG_I2C, "M5IOE1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, _requestedSpeed);

    // 步骤 7: 设置中断模式
    // Step 7: Set interrupt mode
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        m5ioe1_err_t err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }

    return M5IOE1_OK;
}

#if M5IOE1_HAS_M5UNIFIED_I2C
// =====================================================
// Type A (Arduino): M5Unified I2C_Class, no hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed, m5ioe1_int_mode_t intMode)
{
    if (!i2c || !i2c->isEnabled()) {
        M5IOE1_LOG_E(TAG_I2C, "M5Unified I2C_Class not initialized");
        return M5IOE1_ERR_INVALID_ARG;
    }

    _wire   = nullptr;
    _m5_i2c = i2c;
    _addr   = addr;
    _sda    = 0;
    _scl    = 0;
    _intPin = -1;

    if (intMode == M5IOE1_INT_MODE_HARDWARE) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证 I2C 频率
    // Validate I2C frequency
    if (!_isValidI2cFrequency(speed)) {
        M5IOE1_LOG_W(TAG_I2C, "Invalid I2C frequency: %lu Hz. Falling back to 100KHz.", speed);
        _requestedSpeed = M5IOE1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (device default)");

    // 步骤 1: 尝试唤醒设备
    // Step 1: Try to wake up the device
    _commFreq = M5IOE1_I2C_FREQ_100K;
    M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
    M5IOE1_DELAY_MS(10);

    // 步骤 2: 先尝试100K通信
    // Step 2: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 3: 100K失败，等待800ms后再尝试一次100K
        // Step 3: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 4: 100K第二次失败，尝试400K
            // Step 4: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");
            _commFreq = M5IOE1_I2C_FREQ_400K;

            M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 5: 都失败，初始化失败
                // Step 5: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                _m5_i2c = nullptr;
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

    // 步骤 6: 通信成功，设置为已初始化
    // Step 6: Communication succeeded, set as initialized
    _initialized = true;

    // 步骤 7: 强制配置I2C（用户请求的频率 + 关闭休眠）
    // Step 7: Force configure I2C (user requested speed + disable sleep)
    m5ioe1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5IOE1_I2C_FREQ_400K) ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;

    // 检查当前速度
    // Check current speed
    m5ioe1_i2c_speed_t currentSpeed;
    if (getI2cSpeed(&currentSpeed) == M5IOE1_OK) {
        if (currentSpeed == targetSpeed) {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed matches user request (%s)",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        } else {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed differs from user request (Current: %s, Requested: %s)",
                         (currentSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        }
    } else {
        M5IOE1_LOG_W(TAG_I2C, "Failed to read current I2C speed");
    }

    if (setI2cConfig(0, targetSpeed, M5IOE1_WAKE_EDGE_FALLING, M5IOE1_PULL_ENABLED) != M5IOE1_OK) {
        M5IOE1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 切换到目标频率
    // Switch to target frequency
    _commFreq = _requestedSpeed;

    // 步骤 8: 快照
    // Step 8: Snapshot
    _snapshotPinStates();
    _snapshotPwmStates();
    _snapshotAdcState();
    _snapshotAw8737a();
    _snapshotI2cConfig();
    M5IOE1_LOG_D(TAG_I2C, "Snapshot completed (pins/pwm/adc/aw8737a/i2c)");

    M5IOE1_LOG_I(TAG_I2C, "M5IOE1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, _requestedSpeed);

    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        m5ioe1_err_t err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }

    return M5IOE1_OK;
}

// =====================================================
// Type B (Arduino): M5Unified I2C_Class, with hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed, int intPin, m5ioe1_int_mode_t intMode)
{
    m5ioe1_err_t err = begin(i2c, addr, speed, M5IOE1_INT_MODE_DISABLED);
    if (err != M5IOE1_OK) {
        return err;
    }

    _intPin = intPin;
    if (intMode == M5IOE1_INT_MODE_HARDWARE && _intPin < 0) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }
    return M5IOE1_OK;
}
#endif  // M5IOE1_HAS_M5UNIFIED_I2C

#else  // ESP-IDF

// =====================================================
// Type 1A: Self-created I2C bus, no hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(i2c_port_t port, uint8_t addr, int sda, int scl, uint32_t speed, m5ioe1_int_mode_t intMode)
{
    _addr        = addr;
    _busExternal = false;
#if M5IOE1_HAS_I2C_MASTER
    _i2cDriverType = M5IOE1_I2C_DRIVER_SELF_CREATED;
#else
    _i2cDriverType = M5IOE1_I2C_DRIVER_LEGACY;
#endif
    _intPin = -1;
    _port   = port;
    _sda    = sda;
    _scl    = scl;

    if (intMode == M5IOE1_INT_MODE_HARDWARE) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证 I2C 频率 - M5IOE1 仅支持 100KHz 或 400KHz
    // Validate I2C frequency - M5IOE1 only supports 100KHz or 400KHz
    if (!_isValidI2cFrequency(speed)) {
        M5IOE1_LOG_W(TAG_I2C,
                     "Invalid I2C frequency: %lu Hz. M5IOE1 only supports 100KHz or 400KHz. Falling back to 100KHz.",
                     speed);
        _requestedSpeed = M5IOE1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

#if M5IOE1_HAS_I2C_MASTER
    // 始终以 100KHz 开始 - M5IOE1 在上电/复位后默认为 100KHz
    // Always start with 100KHz - M5IOE1 defaults to 100KHz after power-on/reset
    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (device default)");

    // 使用 ESP-IDF 原生驱动创建 I2C 主总线
    // Create I2C master bus using ESP-IDF native driver
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
        M5IOE1_LOG_E(TAG_I2C, "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return M5IOE1_ERR_I2C_CONFIG;
    }

    // 在 100KHz 创建设备句柄
    // Create device handle at 100KHz
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = _addr,
        .scl_speed_hz    = M5IOE1_I2C_FREQ_100K,
        .scl_wait_us     = 0,
        .flags =
            {
                .disable_ack_check = false,
            },
    };

    // 在 100KHz 创建设备句柄
    // Create device handle at 100KHz
    ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
    if (ret != ESP_OK) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_del_master_bus(_i2c_master_bus);
        _i2c_master_bus = nullptr;
        return M5IOE1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备 - 发送 I2C START 信号
    // Try to wake up the device - send I2C START signal
    // M5IOE1 可能处于睡眠状态，需要先唤醒
    // M5IOE1 may be in sleep mode, need to wake up first
    M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
    M5IOE1_DELAY_MS(10);

    // 步骤 1: 先尝试100K通信
    // Step 1: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 2: 100K失败，等待800ms后再尝试一次100K
        // Step 2: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 3: 100K第二次失败，尝试400K
            // Step 3: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");

            // 删除当前100K设备句柄
            // Remove current 100K device handle
            i2c_master_bus_rm_device(_i2c_master_dev);
            _i2c_master_dev = nullptr;

            // 以400K重新创建设备句柄
            // Recreate device handle at 400K
            i2c_device_config_t dev_config_400k = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address  = _addr,
                .scl_speed_hz    = M5IOE1_I2C_FREQ_400K,
                .scl_wait_us     = 0,
                .flags =
                    {
                        .disable_ack_check = false,
                    },
            };

            ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config_400k, &_i2c_master_dev);
            if (ret != ESP_OK) {
                M5IOE1_LOG_E(TAG_I2C, "Failed to add I2C device at 400KHz: %s", esp_err_to_name(ret));
                i2c_del_master_bus(_i2c_master_bus);
                _i2c_master_bus = nullptr;
                return M5IOE1_ERR_I2C_CONFIG;
            }

            M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 4: 都失败，初始化失败
                // Step 4: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_master_bus_rm_device(_i2c_master_dev);
                i2c_del_master_bus(_i2c_master_bus);
                _i2c_master_dev = nullptr;
                _i2c_master_bus = nullptr;
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

#else  // !M5IOE1_HAS_I2C_MASTER
    // 始终以 100KHz 开始 - Legacy API (IDF < 5.3.0)
    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (Legacy API, IDF < 5.3.0)");

    i2c_config_t i2c_conf     = {};
    i2c_conf.mode             = I2C_MODE_MASTER;
    i2c_conf.sda_io_num       = sda;
    i2c_conf.scl_io_num       = scl;
    i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = M5IOE1_I2C_FREQ_100K;

    esp_err_t ret = i2c_param_config(port, &i2c_conf);
    if (ret != ESP_OK) {
        M5IOE1_LOG_E(TAG_I2C, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return M5IOE1_ERR_I2C_CONFIG;
    }

    ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        M5IOE1_LOG_E(TAG_I2C, "i2c_driver_install failed: %s", esp_err_to_name(ret));
        return M5IOE1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备 - 发送 I2C START 信号
    // Try to wake up the device - send I2C START signal
    M5IOE1_I2C_LEGACY_SEND_WAKE(port, _addr);
    M5IOE1_DELAY_MS(10);

    // 步骤 1: 先尝试100K通信
    // Step 1: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 2: 100K失败，等待800ms后再尝试一次100K
        // Step 2: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_I2C_LEGACY_SEND_WAKE(port, _addr);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 3: 100K第二次失败，尝试400K
            // Step 3: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");

            i2c_conf.master.clk_speed = M5IOE1_I2C_FREQ_400K;
            ret                       = i2c_param_config(port, &i2c_conf);
            if (ret != ESP_OK) {
                M5IOE1_LOG_E(TAG_I2C, "i2c_param_config (400K) failed: %s", esp_err_to_name(ret));
                i2c_driver_delete(port);
                return M5IOE1_ERR_I2C_CONFIG;
            }

            M5IOE1_I2C_LEGACY_SEND_WAKE(port, _addr);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 4: 都失败，初始化失败
                // Step 4: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_driver_delete(port);
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

#endif  // M5IOE1_HAS_I2C_MASTER

    // 步骤 5: 通信成功，设置为已初始化
    // Step 5: Communication succeeded, set as initialized
    _initialized = true;

    // 步骤 6: 强制配置I2C（用户请求的频率 + 关闭休眠）
    // 注意：setI2cConfig 内部会自动切换主机 I2C 总线速度
    // Step 5: Force configure I2C (user requested speed + disable sleep)
    // Note: setI2cConfig will automatically switch host I2C bus speed internally
    m5ioe1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5IOE1_I2C_FREQ_400K) ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;

    // 检查当前速度
    // Check current speed
    m5ioe1_i2c_speed_t currentSpeed;
    if (getI2cSpeed(&currentSpeed) == M5IOE1_OK) {
        if (currentSpeed == targetSpeed) {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed matches user request (%s)",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        } else {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed differs from user request (Current: %s, Requested: %s)",
                         (currentSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        }
    } else {
        M5IOE1_LOG_W(TAG_I2C, "Failed to read current I2C speed");
    }

    if (setI2cConfig(0, targetSpeed, M5IOE1_WAKE_EDGE_FALLING, M5IOE1_PULL_ENABLED) != M5IOE1_OK) {
        M5IOE1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤 6: 快照
    // Step 6: Snapshot
    _snapshotPinStates();
    _snapshotPwmStates();
    _snapshotAdcState();
    _snapshotAw8737a();
    _snapshotI2cConfig();
    M5IOE1_LOG_D(TAG_I2C, "Snapshot completed (pins/pwm/adc/aw8737a/i2c)");

    M5IOE1_LOG_I(TAG_I2C, "M5IOE1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, _requestedSpeed);

    // 如果未禁用，设置中断模式
    // Set interrupt mode if not disabled
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        m5ioe1_err_t err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }

    return M5IOE1_OK;
}

// =====================================================
// Type 1B: Self-created I2C bus, with hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(i2c_port_t port, uint8_t addr, int sda, int scl, uint32_t speed, int intPin,
                           m5ioe1_int_mode_t intMode)
{
    m5ioe1_err_t err = begin(port, addr, sda, scl, speed, M5IOE1_INT_MODE_DISABLED);
    if (err != M5IOE1_OK) {
        return err;
    }

    _intPin = intPin;
    if (intMode == M5IOE1_INT_MODE_HARDWARE && _intPin < 0) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }
    return M5IOE1_OK;
}

#if M5IOE1_HAS_I2C_MASTER
// =====================================================
// Type 2A: Existing i2c_master_bus_handle_t, no hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t speed, m5ioe1_int_mode_t intMode)
{
    _addr           = addr;
    _busExternal    = true;
    _i2cDriverType  = M5IOE1_I2C_DRIVER_MASTER;
    _intPin         = -1;
    _i2c_master_bus = bus;
    _sda            = -1;  // 外部总线未知
                           // Unknown for external bus
    _scl = -1;

    if (intMode == M5IOE1_INT_MODE_HARDWARE) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证 I2C 频率
    // Validate I2C frequency
    if (!_isValidI2cFrequency(speed)) {
        M5IOE1_LOG_W(TAG_I2C, "Invalid I2C frequency: %lu Hz. Falling back to 100KHz.", speed);
        _requestedSpeed = M5IOE1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (device default)");

    // 在 100KHz 创建设备句柄
    // Create device handle at 100KHz
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = _addr,
        .scl_speed_hz    = M5IOE1_I2C_FREQ_100K,
        .scl_wait_us     = 0,
        .flags =
            {
                .disable_ack_check = false,
            },
    };

    // 在 100KHz 创建设备句柄
    // Create device handle at 100KHz
    esp_err_t ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
    if (ret != ESP_OK) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return M5IOE1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备 - 发送 I2C START 信号
    // Try to wake up the device - send I2C START signal
    // M5IOE1 可能处于睡眠状态，需要先唤醒
    // M5IOE1 may be in sleep mode, need to wake up first
    M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
    M5IOE1_DELAY_MS(10);

    // 步骤 1: 先尝试100K通信
    // Step 1: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 2: 100K失败，等待800ms后再尝试一次100K
        // Step 2: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 3: 100K第二次失败，尝试400K
            // Step 3: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");

            // 删除当前100K设备句柄
            // Remove current 100K device handle
            i2c_master_bus_rm_device(_i2c_master_dev);
            _i2c_master_dev = nullptr;

            // 以400K重新创建设备句柄
            // Recreate device handle at 400K
            i2c_device_config_t dev_config_400k = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address  = _addr,
                .scl_speed_hz    = M5IOE1_I2C_FREQ_400K,
                .scl_wait_us     = 0,
                .flags =
                    {
                        .disable_ack_check = false,
                    },
            };

            ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config_400k, &_i2c_master_dev);
            if (ret != ESP_OK) {
                M5IOE1_LOG_E(TAG_I2C, "Failed to add I2C device at 400KHz: %s", esp_err_to_name(ret));
                return M5IOE1_ERR_I2C_CONFIG;
            }

            M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 4: 都失败，初始化失败
                // Step 4: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_master_bus_rm_device(_i2c_master_dev);
                _i2c_master_dev = nullptr;
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

    // 步骤 5: 通信成功，设置为已初始化
    // Step 5: Communication succeeded, set as initialized
    _initialized = true;

    // 步骤 6: 强制配置I2C（用户请求的频率 + 关闭休眠）
    // 注意：setI2cConfig 内部会自动切换主机 I2C 总线速度
    // Step 5: Force configure I2C (user requested speed + disable sleep)
    // Note: setI2cConfig will automatically switch host I2C bus speed internally
    m5ioe1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5IOE1_I2C_FREQ_400K) ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;

    // 检查当前速度
    // Check current speed
    m5ioe1_i2c_speed_t currentSpeed;
    if (getI2cSpeed(&currentSpeed) == M5IOE1_OK) {
        if (currentSpeed == targetSpeed) {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed matches user request (%s)",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        } else {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed differs from user request (Current: %s, Requested: %s)",
                         (currentSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        }
    } else {
        M5IOE1_LOG_W(TAG_I2C, "Failed to read current I2C speed");
    }

    if (setI2cConfig(0, targetSpeed, M5IOE1_WAKE_EDGE_FALLING, M5IOE1_PULL_ENABLED) != M5IOE1_OK) {
        M5IOE1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤 6: 快照
    // Step 6: Snapshot
    _snapshotPinStates();
    _snapshotPwmStates();
    _snapshotAdcState();
    _snapshotAw8737a();
    _snapshotI2cConfig();
    M5IOE1_LOG_D(TAG_I2C, "Snapshot completed (pins/pwm/adc/aw8737a/i2c)");

    M5IOE1_LOG_I(TAG_I2C, "M5IOE1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, _requestedSpeed);

    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        m5ioe1_err_t err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }

    return M5IOE1_OK;
}

// =====================================================
// Type 2B: Existing i2c_master_bus_handle_t, with hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t speed, int intPin,
                           m5ioe1_int_mode_t intMode)
{
    m5ioe1_err_t err = begin(bus, addr, speed, M5IOE1_INT_MODE_DISABLED);
    if (err != M5IOE1_OK) {
        return err;
    }

    _intPin = intPin;
    if (intMode == M5IOE1_INT_MODE_HARDWARE && _intPin < 0) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }
    return M5IOE1_OK;
}
#endif  // M5IOE1_HAS_I2C_MASTER

// =====================================================
// Type 3A: Existing i2c_bus_handle_t, no hardware interrupt
// =====================================================
#if M5IOE1_HAS_I2C_BUS
m5ioe1_err_t M5IOE1::begin(i2c_bus_handle_t bus, uint8_t addr, uint32_t speed, m5ioe1_int_mode_t intMode)
{
    _addr          = addr;
    _busExternal   = true;
    _i2cDriverType = M5IOE1_I2C_DRIVER_BUS;
    _intPin        = -1;
    _i2c_bus       = bus;
    _sda           = -1;  // 外部总线未知
                          // Unknown for external bus
    _scl = -1;

    if (intMode == M5IOE1_INT_MODE_HARDWARE) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证 I2C 频率
    // Validate I2C frequency
    if (!_isValidI2cFrequency(speed)) {
        M5IOE1_LOG_W(TAG_I2C, "Invalid I2C frequency: %lu Hz. Falling back to 100KHz.", speed);
        _requestedSpeed = M5IOE1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (device default)");

    // 在 100KHz 创建设备句柄
    // Create device handle at 100KHz
    _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, M5IOE1_I2C_FREQ_100K);
    if (_i2c_device == nullptr) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to create I2C device");
        return M5IOE1_ERR_I2C_CONFIG;
    }

    // 尝试唤醒设备 - 发送 I2C START 信号
    // Try to wake up the device - send I2C START signal
    // M5IOE1 可能处于睡眠状态，需要先唤醒
    // M5IOE1 may be in sleep mode, need to wake up first
    M5IOE1_I2C_BUS_SEND_WAKE(_i2c_device, M5IOE1_REG_REV);
    M5IOE1_DELAY_MS(10);

    // 步骤 1: 先尝试100K通信
    // Step 1: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 2: 100K失败，等待800ms后再尝试一次100K
        // Step 2: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_I2C_BUS_SEND_WAKE(_i2c_device, M5IOE1_REG_REV);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 3: 100K第二次失败，尝试400K
            // Step 3: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");

            // 删除当前100K设备句柄
            // Remove current 100K device handle
            i2c_bus_device_delete(&_i2c_device);
            _i2c_device = nullptr;

            // 以400K重新创建设备句柄
            // Recreate device handle at 400K
            _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, M5IOE1_I2C_FREQ_400K);
            if (_i2c_device == nullptr) {
                M5IOE1_LOG_E(TAG_I2C, "Failed to create I2C device at 400KHz");
                return M5IOE1_ERR_I2C_CONFIG;
            }

            M5IOE1_I2C_BUS_SEND_WAKE(_i2c_device, M5IOE1_REG_REV);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 4: 都失败，初始化失败
                // Step 4: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                i2c_bus_device_delete(&_i2c_device);
                _i2c_device = nullptr;
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

    // 步骤 5: 通信成功，设置为已初始化
    // Step 5: Communication succeeded, set as initialized
    _initialized = true;

    // 步骤 6: 强制配置I2C（用户请求的频率 + 关闭休眠）
    // 注意：setI2cConfig 内部会自动切换主机 I2C 总线速度
    // Step 5: Force configure I2C (user requested speed + disable sleep)
    // Note: setI2cConfig will automatically switch host I2C bus speed internally
    m5ioe1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5IOE1_I2C_FREQ_400K) ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;

    // 检查当前速度
    // Check current speed
    m5ioe1_i2c_speed_t currentSpeed;
    if (getI2cSpeed(&currentSpeed) == M5IOE1_OK) {
        if (currentSpeed == targetSpeed) {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed matches user request (%s)",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        } else {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed differs from user request (Current: %s, Requested: %s)",
                         (currentSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        }
    } else {
        M5IOE1_LOG_W(TAG_I2C, "Failed to read current I2C speed");
    }

    if (setI2cConfig(0, targetSpeed, M5IOE1_WAKE_EDGE_FALLING, M5IOE1_PULL_ENABLED) != M5IOE1_OK) {
        M5IOE1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 步骤 6: 快照
    // Step 6: Snapshot
    _snapshotPinStates();
    _snapshotPwmStates();
    _snapshotAdcState();
    _snapshotAw8737a();
    _snapshotI2cConfig();
    M5IOE1_LOG_D(TAG_I2C, "Snapshot completed (pins/pwm/adc/aw8737a/i2c)");

    M5IOE1_LOG_I(TAG_I2C, "M5IOE1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, _requestedSpeed);

    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        m5ioe1_err_t err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }

    return M5IOE1_OK;
}

// =====================================================
// Type 3B: Existing i2c_bus_handle_t, with hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(i2c_bus_handle_t bus, uint8_t addr, uint32_t speed, int intPin, m5ioe1_int_mode_t intMode)
{
    m5ioe1_err_t err = begin(bus, addr, speed, M5IOE1_INT_MODE_DISABLED);
    if (err != M5IOE1_OK) {
        return err;
    }

    _intPin = intPin;
    if (intMode == M5IOE1_INT_MODE_HARDWARE && _intPin < 0) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }
    return M5IOE1_OK;
}

#endif  // M5IOE1_HAS_I2C_BUS

#if M5IOE1_HAS_M5UNIFIED_I2C
// =====================================================
// Type 4A: M5Unified I2C_Class, no hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed, m5ioe1_int_mode_t intMode)
{
    if (!i2c || !i2c->isEnabled()) {
        M5IOE1_LOG_E(TAG_I2C, "M5Unified I2C_Class not initialized");
        return M5IOE1_ERR_INVALID_ARG;
    }

    _addr          = addr;
    _busExternal   = true;
    _i2cDriverType = M5IOE1_I2C_DRIVER_M5UNIFIED;
    _intPin        = -1;
    _m5_i2c        = i2c;
    _sda           = -1;
    _scl           = -1;

    if (intMode == M5IOE1_INT_MODE_HARDWARE) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证 I2C 频率
    // Validate I2C frequency
    if (!_isValidI2cFrequency(speed)) {
        M5IOE1_LOG_W(TAG_I2C, "Invalid I2C frequency: %lu Hz. Falling back to 100KHz.", speed);
        _requestedSpeed = M5IOE1_I2C_FREQ_100K;
    } else {
        _requestedSpeed = speed;
    }

    M5IOE1_LOG_I(TAG_I2C, "Initializing M5IOE1 with 100KHz (device default)");

    // 步骤 1: 尝试唤醒设备
    // Step 1: Try to wake up the device
    _commFreq = M5IOE1_I2C_FREQ_100K;
    M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
    M5IOE1_DELAY_MS(10);

    // 步骤 2: 先尝试100K通信
    // Step 2: Try 100K communication first
    if (!_initDevice()) {
        // 步骤 3: 100K失败，等待800ms后再尝试一次100K
        // Step 3: 100K failed, wait 800ms and retry 100K
        M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz, waiting 800ms and retrying 100KHz...");
        M5IOE1_DELAY_MS(800);

        M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
        M5IOE1_DELAY_MS(10);

        if (!_initDevice()) {
            // 步骤 4: 100K第二次失败，尝试400K
            // Step 4: 100K failed again, try 400K
            M5IOE1_LOG_W(TAG_I2C, "Failed at 100KHz (retry), trying 400KHz...");
            _commFreq = M5IOE1_I2C_FREQ_400K;

            M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
            M5IOE1_DELAY_MS(10);

            if (!_initDevice()) {
                // 步骤 5: 都失败，初始化失败
                // Step 5: All attempts failed, initialization failed
                M5IOE1_LOG_E(TAG_I2C, "Failed at 100KHz (twice) and 400KHz");
                _m5_i2c = nullptr;
                return M5IOE1_ERR_I2C_COMM;
            }
        }
    }

    // 步骤 6: 通信成功，设置为已初始化
    // Step 6: Communication succeeded, set as initialized
    _initialized = true;

    // 步骤 7: 强制配置I2C（用户请求的频率 + 关闭休眠）
    // Step 7: Force configure I2C (user requested speed + disable sleep)
    m5ioe1_i2c_speed_t targetSpeed =
        (_requestedSpeed == M5IOE1_I2C_FREQ_400K) ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;

    // 检查当前速度
    // Check current speed
    m5ioe1_i2c_speed_t currentSpeed;
    if (getI2cSpeed(&currentSpeed) == M5IOE1_OK) {
        if (currentSpeed == targetSpeed) {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed matches user request (%s)",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        } else {
            M5IOE1_LOG_I(TAG_I2C, "Current I2C speed differs from user request (Current: %s, Requested: %s)",
                         (currentSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K",
                         (targetSpeed == M5IOE1_I2C_SPEED_400K) ? "400K" : "100K");
        }
    } else {
        M5IOE1_LOG_W(TAG_I2C, "Failed to read current I2C speed");
    }

    if (setI2cConfig(0, targetSpeed, M5IOE1_WAKE_EDGE_FALLING, M5IOE1_PULL_ENABLED) != M5IOE1_OK) {
        M5IOE1_LOG_W(TAG_I2C, "Failed to set I2C config");
    }

    // 切换到目标频率
    // Switch to target frequency
    _commFreq = _requestedSpeed;

    // 步骤 8: 快照
    // Step 8: Snapshot
    _snapshotPinStates();
    _snapshotPwmStates();
    _snapshotAdcState();
    _snapshotAw8737a();
    _snapshotI2cConfig();
    M5IOE1_LOG_D(TAG_I2C, "Snapshot completed (pins/pwm/adc/aw8737a/i2c)");

    M5IOE1_LOG_I(TAG_I2C, "M5IOE1 initialized at address 0x%02X (I2C: %lu Hz)", _addr, _requestedSpeed);

    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        m5ioe1_err_t err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }

    return M5IOE1_OK;
}

// =====================================================
// Type 4B: M5Unified I2C_Class, with hardware interrupt
// =====================================================
m5ioe1_err_t M5IOE1::begin(m5::I2C_Class* i2c, uint8_t addr, uint32_t speed, int intPin, m5ioe1_int_mode_t intMode)
{
    m5ioe1_err_t err = begin(i2c, addr, speed, M5IOE1_INT_MODE_DISABLED);
    if (err != M5IOE1_OK) {
        return err;
    }

    _intPin = intPin;
    if (intMode == M5IOE1_INT_MODE_HARDWARE && _intPin < 0) {
        M5IOE1_LOG_E(TAG_I2C, "Hardware interrupt mode requires interrupt pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (intMode != M5IOE1_INT_MODE_DISABLED) {
        err = setInterruptMode(intMode);
        if (err != M5IOE1_OK) {
            _initialized = false;
            return err;
        }
    }
    return M5IOE1_OK;
}
#endif  // M5IOE1_HAS_M5UNIFIED_I2C

#endif  // !ARDUINO

m5ioe1_err_t M5IOE1::setInterruptMode(m5ioe1_int_mode_t intMode, uint32_t pollingIntervalMs)
{
    const char* modeName = "UNKNOWN";
    if (intMode == M5IOE1_INT_MODE_DISABLED) {
        modeName = "DISABLED";
    } else if (intMode == M5IOE1_INT_MODE_POLLING) {
        modeName = "POLLING";
    } else if (intMode == M5IOE1_INT_MODE_HARDWARE) {
        modeName = "HARDWARE";
    }

    if (intMode == M5IOE1_INT_MODE_POLLING) {
        M5IOE1_LOG_I(TAG_IRQ, "Interrupt mode -> %s (pause if I2C sleep on; resume when off)", modeName);
    } else {
        M5IOE1_LOG_I(TAG_IRQ, "Interrupt mode -> %s", modeName);
    }

    _intMode         = intMode;
    _pollingInterval = pollingIntervalMs;

#ifdef ARDUINO
    _cleanupPollingArduino();
    _cleanupHardwareInterruptArduino();

    switch (intMode) {
        case M5IOE1_INT_MODE_POLLING:
            if (_i2cConfigValid && _i2cConfig.sleepTime > 0) {
                return M5IOE1_OK;
            }
            return _setupPollingArduino() ? M5IOE1_OK : M5IOE1_ERR_INTERNAL;
        case M5IOE1_INT_MODE_HARDWARE:
            return _setupHardwareInterruptArduino() ? M5IOE1_OK : M5IOE1_ERR_INTERNAL;
        default:
            break;
    }
#else
    _cleanupPolling();
    _cleanupHardwareInterrupt();

    switch (intMode) {
        case M5IOE1_INT_MODE_POLLING:
            if (_i2cConfigValid && _i2cConfig.sleepTime > 0) {
                return M5IOE1_OK;
            }
            return _setupPolling() ? M5IOE1_OK : M5IOE1_ERR_INTERNAL;
        case M5IOE1_INT_MODE_HARDWARE:
            return _setupHardwareInterrupt() ? M5IOE1_OK : M5IOE1_ERR_INTERNAL;
        default:
            break;
    }
#endif

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setPollingInterval(float seconds)
{
    if (seconds < 0.001f || seconds > 3600.0f) {
        M5IOE1_LOG_E(TAG_IRQ, "Invalid polling interval: %.3f seconds (valid range: 0.001-3600)", seconds);
        return M5IOE1_ERR_INVALID_ARG;
    }

    uint32_t intervalMs = (uint32_t)(seconds * 1000.0f);
    _pollingInterval    = intervalMs;

    // 如果当前处于轮询模式，使用新间隔重新启动
    // If currently in polling mode, restart with new interval
    if (_intMode == M5IOE1_INT_MODE_POLLING) {
#ifdef ARDUINO
        _cleanupPollingArduino();
        _cleanupHardwareInterruptArduino();
        if (_i2cConfigValid && _i2cConfig.sleepTime > 0) {
            return M5IOE1_OK;
        }
        return _setupPollingArduino() ? M5IOE1_OK : M5IOE1_ERR_INTERNAL;
#else
        _cleanupPolling();
        _cleanupHardwareInterrupt();
        if (_i2cConfigValid && _i2cConfig.sleepTime > 0) {
            return M5IOE1_OK;
        }
        return _setupPolling() ? M5IOE1_OK : M5IOE1_ERR_INTERNAL;
#endif
    }

    M5IOE1_LOG_I(TAG_IRQ, "Polling interval set to %.3f seconds (%u ms)", seconds, intervalMs);
    return M5IOE1_OK;
}

// ============================
// 设备信息
// Device Information
// ============================

m5ioe1_err_t M5IOE1::getUID(uint16_t* uid)
{
    if (uid == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_UID_L, uid) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getVersion(uint8_t* version)
{
    if (version == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg(M5IOE1_REG_REV, version) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getRefVoltage(uint16_t* voltage_mv)
{
    if (voltage_mv == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_REF_VOLTAGE_L, voltage_mv) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

// ============================
// GPIO 功能
// GPIO Functions
// ============================

m5ioe1_err_t M5IOE1::_pinModeWithErr(uint8_t pin, uint8_t mode)
{
    if (!_isValidPin(pin)) {
        M5IOE1_LOG_E(TAG_GPIO, "Invalid pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_GPIO, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint16_t modeReg = 0, puReg = 0, pdReg = 0, drvReg = 0;

    M5IOE1_LOG_D(TAG_GPIO, "_pinModeWithErr: pin=%d mode=0x%02X", pin, mode);

    if (!_readReg16(M5IOE1_REG_GPIO_MODE_L, &modeReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_MODE register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_PU_L, &puReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_PU register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_PD_L, &pdReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_PD register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_DRV_L, &drvReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_DRV register");
        return M5IOE1_ERR_I2C_COMM;
    }

    switch (mode) {
        case INPUT:  // 0x01
            modeReg &= ~(1 << pin);
            puReg &= ~(1 << pin);
            pdReg &= ~(1 << pin);
            _pinStates[pin].isOutput = false;
            _pinStates[pin].pull     = 0;
            break;
        case PULLUP:        // 0x04 - 仅上拉
        case INPUT_PULLUP:  // 0x05
            modeReg &= ~(1 << pin);
            puReg |= (1 << pin);
            pdReg &= ~(1 << pin);
            _pinStates[pin].isOutput = false;
            _pinStates[pin].pull     = 1;
            break;
        case PULLDOWN:        // 0x08 - 仅下拉
        case INPUT_PULLDOWN:  // 0x09
            modeReg &= ~(1 << pin);
            puReg &= ~(1 << pin);
            pdReg |= (1 << pin);
            _pinStates[pin].isOutput = false;
            _pinStates[pin].pull     = 2;
            break;
        case OUTPUT:  // 0x03
            // 如果此引脚上启用了 PWM 则禁用
            // Disable PWM if enabled on this pin
            if (_isPwmPin(pin)) {
                uint8_t ch       = _getPwmChannel(pin);
                uint8_t regL     = M5IOE1_REG_PWM1_DUTY_L + ch * 2;
                uint16_t pwmData = 0;
                if (_readReg16(regL, &pwmData)) {
                    if (pwmData & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) {
                        pwmData &= ~((uint16_t)M5IOE1_PWM_ENABLE << 8);
                        _writeReg16(regL, pwmData);
                    }
                }
            }
            modeReg |= (1 << pin);
            puReg &= ~(1 << pin);
            pdReg &= ~(1 << pin);
            drvReg &= ~(1 << pin);  // 推挽
                                    // Push-pull
            _pinStates[pin].isOutput = true;
            _pinStates[pin].drive    = 0;
            break;
        case OPEN_DRAIN:         // 0x10
        case OUTPUT_OPEN_DRAIN:  // 0x13
            // 如果此引脚上启用了 PWM 则禁用
            // Disable PWM if enabled on this pin
            if (_isPwmPin(pin)) {
                uint8_t ch       = _getPwmChannel(pin);
                uint8_t regL     = M5IOE1_REG_PWM1_DUTY_L + ch * 2;
                uint16_t pwmData = 0;
                if (_readReg16(regL, &pwmData)) {
                    if (pwmData & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) {
                        pwmData &= ~((uint16_t)M5IOE1_PWM_ENABLE << 8);
                        _writeReg16(regL, pwmData);
                    }
                }
            }
            modeReg |= (1 << pin);
            puReg &= ~(1 << pin);
            pdReg &= ~(1 << pin);
            drvReg |= (1 << pin);  // 开漏
                                   // Open-drain
            _pinStates[pin].isOutput = true;
            _pinStates[pin].drive    = 1;
            break;
        case ANALOG:  // 0xC0
            // 模拟模式 - 设置为输入，无上拉下拉
            // Analog mode - set as input, no pull-up/down
            modeReg &= ~(1 << pin);
            puReg &= ~(1 << pin);
            pdReg &= ~(1 << pin);
            _pinStates[pin].isOutput = false;
            _pinStates[pin].pull     = 0;
            break;
        default:
            M5IOE1_LOG_E(TAG_GPIO, "Invalid mode: %d", mode);
            return M5IOE1_ERR_INVALID_ARG;
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write registers
    if (!_writeReg16(M5IOE1_REG_GPIO_PU_L, puReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_PU register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_PD_L, pdReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_PD register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_DRV_L, drvReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_DRV register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_MODE_L, modeReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_MODE register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualPu = 0, actualPd = 0, actualDrv = 0, actualMode = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_PU_L, &actualPu)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_PU register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_PD_L, &actualPd)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_PD register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_DRV_L, &actualDrv)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_DRV register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_MODE_L, &actualMode)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_MODE register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    bool puMatch   = ((actualPu & (1 << pin)) == (puReg & (1 << pin)));
    bool pdMatch   = ((actualPd & (1 << pin)) == (pdReg & (1 << pin)));
    bool drvMatch  = ((actualDrv & (1 << pin)) == (drvReg & (1 << pin)));
    bool modeMatch = ((actualMode & (1 << pin)) == (modeReg & (1 << pin)));

    if (!puMatch || !pdMatch || !drvMatch || !modeMatch) {
        M5IOE1_LOG_E(TAG_GPIO, "Pin %d mode verification failed: PU=%c, PD=%c, DRV=%c, MODE=%c", pin,
                     puMatch ? 'Y' : 'N', pdMatch ? 'Y' : 'N', drvMatch ? 'Y' : 'N', modeMatch ? 'Y' : 'N');
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_GPIO);

    const char* modeName = (mode == INPUT)               ? "INPUT"
                           : (mode == OUTPUT)            ? "OUTPUT"
                           : (mode == INPUT_PULLUP)      ? "INPUT_PULLUP"
                           : (mode == INPUT_PULLDOWN)    ? "INPUT_PULLDOWN"
                           : (mode == OPEN_DRAIN)        ? "OPEN_DRAIN"
                           : (mode == OUTPUT_OPEN_DRAIN) ? "OUTPUT_OPEN_DRAIN"
                                                         : "UNKNOWN";
    M5IOE1_LOG_I(TAG_GPIO, "Pin %d mode set and verified: %s (0x%02X)", pin, modeName, mode);
    return M5IOE1_OK;
}

void M5IOE1::pinMode(uint8_t pin, uint8_t mode)
{
    (void)_pinModeWithErr(pin, mode);
}

void M5IOE1::pinModeWithRes(uint8_t pin, uint8_t mode, m5ioe1_err_t* err)
{
    if (err == nullptr) {
        M5IOE1_LOG_E(TAG_GPIO, "pinModeWithRes err is null");
        return;
    }
    *err = _pinModeWithErr(pin, mode);
}

m5ioe1_err_t M5IOE1::_digitalWriteWithErr(uint8_t pin, uint8_t value)
{
    if (!_isValidPin(pin)) {
        M5IOE1_LOG_E(TAG_GPIO, "Invalid pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_GPIO, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint16_t outReg = 0;
    M5IOE1_LOG_D(TAG_GPIO, "_digitalWriteWithErr: pin=%d val=%d", pin, value);
    if (!_readReg16(M5IOE1_REG_GPIO_OUT_L, &outReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_OUT register");
        return M5IOE1_ERR_I2C_COMM;
    }

    if (value) {
        outReg |= (1 << pin);
    } else {
        outReg &= ~(1 << pin);
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg16(M5IOE1_REG_GPIO_OUT_L, outReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_OUT register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualOut = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_OUT_L, &actualOut)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_OUT register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    bool outMatch = ((actualOut & (1 << pin)) == (outReg & (1 << pin)));

    if (!outMatch) {
        M5IOE1_LOG_E(TAG_GPIO, "Pin %d write verification failed: expected %d, actual %d", pin, value ? HIGH : LOW,
                     (actualOut & (1 << pin)) ? HIGH : LOW);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _pinStates[pin].outputLevel = value ? 1 : 0;

    // 步骤 5: 打印日志
    // Step 5: Log
    M5IOE1_LOG_I(TAG_GPIO, "Pin %d digital write and verified: %d", pin, value ? HIGH : LOW);

    return M5IOE1_OK;
}

void M5IOE1::digitalWrite(uint8_t pin, uint8_t value)
{
    (void)_digitalWriteWithErr(pin, value);
}

void M5IOE1::digitalWriteWithRes(uint8_t pin, uint8_t value, m5ioe1_err_t* err)
{
    if (err == nullptr) {
        M5IOE1_LOG_E(TAG_GPIO, "digitalWriteWithRes err is null");
        return;
    }
    *err = _digitalWriteWithErr(pin, value);
}

m5ioe1_err_t M5IOE1::_digitalReadWithErr(uint8_t pin, int* value)
{
    if (value == nullptr) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_isValidPin(pin)) {
        M5IOE1_LOG_E(TAG_GPIO, "Invalid pin");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_GPIO, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint16_t inReg = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_IN_L, &inReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_IN register");
        return M5IOE1_ERR_I2C_COMM;
    }

    *value = (inReg & (1 << pin)) ? HIGH : LOW;
    M5IOE1_LOG_D(TAG_GPIO, "_digitalReadWithErr: pin=%d -> %d", pin, *value);
    return M5IOE1_OK;
}

int M5IOE1::digitalRead(uint8_t pin)
{
    int value = -1;
    if (_digitalReadWithErr(pin, &value) != M5IOE1_OK) {
        return -1;
    }
    return value;
}

int M5IOE1::digitalReadWithRes(uint8_t pin, m5ioe1_err_t* err)
{
    if (err == nullptr) {
        M5IOE1_LOG_E(TAG_GPIO, "digitalReadWithRes err is null");
        return -1;
    }

    int value = -1;
    *err      = _digitalReadWithErr(pin, &value);
    return value;
}

// ============================
// 高级 GPIO 功能
// Advanced GPIO Functions
// ============================

m5ioe1_err_t M5IOE1::setPullMode(uint8_t pin, uint8_t pullMode)
{
    if (!_isValidPin(pin)) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_FAIL;

    M5IOE1_LOG_D(TAG_GPIO, "setPullMode: pin=%d pull=%d", pin, pullMode);

    uint16_t puReg = 0, pdReg = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_PU_L, &puReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_PU register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_PD_L, &pdReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_PD register");
        return M5IOE1_ERR_I2C_COMM;
    }

    puReg &= ~(1 << pin);
    pdReg &= ~(1 << pin);

    if (pullMode == M5IOE1_PULL_UP) {
        puReg |= (1 << pin);
        _pinStates[pin].pull = 1;
    } else if (pullMode == M5IOE1_PULL_DOWN) {
        pdReg |= (1 << pin);
        _pinStates[pin].pull = 2;
    } else {
        _pinStates[pin].pull = 0;
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write registers
    if (!_writeReg16(M5IOE1_REG_GPIO_PU_L, puReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_PU register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_PD_L, pdReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_PD register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualPu = 0, actualPd = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_PU_L, &actualPu)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_PU register");
        return M5IOE1_ERR_I2C_COMM;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_PD_L, &actualPd)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_PD register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    bool puMatch = ((actualPu & (1 << pin)) == (puReg & (1 << pin)));
    bool pdMatch = ((actualPd & (1 << pin)) == (pdReg & (1 << pin)));

    if (!puMatch || !pdMatch) {
        M5IOE1_LOG_E(TAG_GPIO, "Pin %d pull mode verification failed: PU=%c, PD=%c", pin, puMatch ? 'Y' : 'N',
                     pdMatch ? 'Y' : 'N');
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_GPIO);

    const char* pullName = (pullMode == M5IOE1_PULL_NONE)   ? "NONE"
                           : (pullMode == M5IOE1_PULL_UP)   ? "PULL_UP"
                           : (pullMode == M5IOE1_PULL_DOWN) ? "PULL_DOWN"
                                                            : "UNKNOWN";
    M5IOE1_LOG_I(TAG_GPIO, "Pin %d pull mode set and verified: %s", pin, pullName);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setDriveMode(uint8_t pin, uint8_t driveMode)
{
    if (!_isValidPin(pin)) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_FAIL;

    M5IOE1_LOG_D(TAG_GPIO, "setDriveMode: pin=%d drive=%d", pin, driveMode);

    uint16_t drvReg = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_DRV_L, &drvReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read GPIO_DRV register");
        return M5IOE1_ERR_I2C_COMM;
    }

    if (driveMode == M5IOE1_DRIVE_OPENDRAIN) {
        drvReg |= (1 << pin);
        _pinStates[pin].drive = 1;
    } else {
        drvReg &= ~(1 << pin);
        _pinStates[pin].drive = 0;
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg16(M5IOE1_REG_GPIO_DRV_L, drvReg)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to write GPIO_DRV register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualDrv = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_DRV_L, &actualDrv)) {
        M5IOE1_LOG_E(TAG_GPIO, "Failed to read back GPIO_DRV register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    bool drvMatch = ((actualDrv & (1 << pin)) == (drvReg & (1 << pin)));

    if (!drvMatch) {
        M5IOE1_LOG_E(TAG_GPIO, "Pin %d drive mode verification failed: DRV=%c", pin, drvMatch ? 'Y' : 'N');
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_GPIO);

    M5IOE1_LOG_I(TAG_GPIO, "Pin %d drive mode set and verified: %s", pin,
                 (driveMode == M5IOE1_DRIVE_OPENDRAIN) ? "OpenDrain" : "PushPull");
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getInputState(uint8_t pin, uint8_t* state)
{
    if (!_isValidPin(pin)) return M5IOE1_ERR_INVALID_ARG;
    if (state == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_FAIL;

    int val = digitalRead(pin);
    if (val < 0) return M5IOE1_ERR_I2C_COMM;

    *state = (uint8_t)val;
    return M5IOE1_OK;
}

// ============================
// 中断功能
// Interrupt Functions
// ============================

void M5IOE1::attachInterrupt(uint8_t pin, m5ioe1_callback_t callback, uint8_t mode)
{
    if (!_isValidPin(pin) || callback == nullptr || !_initialized) return;

    // 检查冲突的中断
    // Check for conflicting interrupts
    if (_hasConflictingInterrupt(pin)) {
        M5IOE1_LOG_E(TAG_IRQ, "Interrupt conflict on pin %d", pin);
        return;
    }

    _callbacks[pin].callback    = callback;
    _callbacks[pin].callbackArg = nullptr;
    _callbacks[pin].arg         = nullptr;
    _callbacks[pin].enabled     = true;
    _callbacks[pin].rising      = (mode == RISING);

    // 将引脚配置为输入
    // Configure pin as input
    uint16_t modeReg = 0;
    _readReg16(M5IOE1_REG_GPIO_MODE_L, &modeReg);
    modeReg &= ~(1 << pin);

    // 步骤 1: 写入 GPIO_MODE 寄存器
    // Step 1: Write GPIO_MODE register
    if (!_writeReg16(M5IOE1_REG_GPIO_MODE_L, modeReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_MODE register for interrupt");
        return;
    }

    // 配置中断
    // Configure interrupt
    uint16_t ieReg = 0, itReg = 0;
    _readReg16(M5IOE1_REG_GPIO_IE_L, &ieReg);
    _readReg16(M5IOE1_REG_GPIO_IP_L, &itReg);

    ieReg |= (1 << pin);
    if (mode == RISING) {
        itReg |= (1 << pin);
    } else {
        itReg &= ~(1 << pin);
    }

    // 步骤 2: 写入中断寄存器
    // Step 2: Write interrupt registers
    if (!_writeReg16(M5IOE1_REG_GPIO_IE_L, ieReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IE register");
        return;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_IP_L, itReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IP register");
        return;
    }

    // 步骤 3: 回读验证
    // Step 3: Read-back verification
    uint16_t actualMode = 0, actualIe = 0, actualIt = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_MODE_L, &actualMode)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_MODE register");
        return;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_IE_L, &actualIe)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_IE register");
        return;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_IP_L, &actualIt)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_IP register");
        return;
    }

    // 步骤 4: 验证关键位是否匹配
    // Step 4: Verify critical bits match
    bool modeMatch = ((actualMode & (1 << pin)) == 0);                   // 输入模式
    bool ieMatch   = ((actualIe & (1 << pin)) != 0);                     // 中断使能
    bool itMatch   = ((actualIt & (1 << pin)) == (itReg & (1 << pin)));  // 触发方式

    if (!modeMatch || !ieMatch || !itMatch) {
        M5IOE1_LOG_E(TAG_IRQ, "Pin %d interrupt verification failed: MODE=%c, IE=%c, IT=%c", pin, modeMatch ? 'Y' : 'N',
                     ieMatch ? 'Y' : 'N', itMatch ? 'Y' : 'N');
        return;
    }

    // 步骤 5: 验证成功，更新缓存
    // Step 5: Verification passed, update cache
    _pinStates[pin].intrEnabled = true;
    _pinStates[pin].intrRising  = (mode == RISING);

    M5IOE1_LOG_I(TAG_IRQ, "Pin %d interrupt attached and verified: mode=%s", pin,
                 mode == RISING ? "RISING" : "FALLING");
}

void M5IOE1::attachInterruptArg(uint8_t pin, m5ioe1_callback_arg_t callback, void* arg, uint8_t mode)
{
    if (!_isValidPin(pin) || callback == nullptr || !_initialized) return;

    if (_hasConflictingInterrupt(pin)) {
        M5IOE1_LOG_E(TAG_IRQ, "Interrupt conflict on pin %d", pin);
        return;
    }

    _callbacks[pin].callback    = nullptr;
    _callbacks[pin].callbackArg = callback;
    _callbacks[pin].arg         = arg;
    _callbacks[pin].enabled     = true;
    _callbacks[pin].rising      = (mode == RISING);

    // 将引脚配置为输入
    // Configure pin as input
    uint16_t modeReg = 0;
    _readReg16(M5IOE1_REG_GPIO_MODE_L, &modeReg);
    modeReg &= ~(1 << pin);

    // 配置中断
    // Configure interrupt
    uint16_t ieReg = 0, itReg = 0;
    _readReg16(M5IOE1_REG_GPIO_IE_L, &ieReg);
    _readReg16(M5IOE1_REG_GPIO_IP_L, &itReg);

    ieReg |= (1 << pin);
    if (mode == RISING) {
        itReg |= (1 << pin);
    } else {
        itReg &= ~(1 << pin);
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write registers
    if (!_writeReg16(M5IOE1_REG_GPIO_MODE_L, modeReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_MODE register");
        return;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_IE_L, ieReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IE register");
        return;
    }
    if (!_writeReg16(M5IOE1_REG_GPIO_IP_L, itReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IP register");
        return;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualMode = 0, actualIe = 0, actualIt = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_MODE_L, &actualMode)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_MODE register");
        return;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_IE_L, &actualIe)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_IE register");
        return;
    }
    if (!_readReg16(M5IOE1_REG_GPIO_IP_L, &actualIt)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_IP register");
        return;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    bool modeMatch = ((actualMode & (1 << pin)) == 0);                   // 输入模式
    bool ieMatch   = ((actualIe & (1 << pin)) != 0);                     // 中断使能
    bool itMatch   = ((actualIt & (1 << pin)) == (itReg & (1 << pin)));  // 触发方式

    if (!modeMatch || !ieMatch || !itMatch) {
        M5IOE1_LOG_E(TAG_IRQ, "Pin %d interrupt verification failed: MODE=%c, IE=%c, IT=%c", pin, modeMatch ? 'Y' : 'N',
                     ieMatch ? 'Y' : 'N', itMatch ? 'Y' : 'N');
        return;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _pinStates[pin].intrEnabled = true;
    _pinStates[pin].intrRising  = (mode == RISING);

    M5IOE1_LOG_I(TAG_IRQ, "Pin %d interrupt attached and verified: mode=%s", pin,
                 mode == RISING ? "RISING" : "FALLING");
}

void M5IOE1::detachInterrupt(uint8_t pin)
{
    if (!_isValidPin(pin) || !_initialized) return;

    uint16_t ieReg = 0;
    _readReg16(M5IOE1_REG_GPIO_IE_L, &ieReg);
    ieReg &= ~(1 << pin);

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg16(M5IOE1_REG_GPIO_IE_L, ieReg)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IE register for detach");
        return;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualIe = 0;
    if (!_readReg16(M5IOE1_REG_GPIO_IE_L, &actualIe)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to read back GPIO_IE register for detach");
        return;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    bool ieMatch = ((actualIe & (1 << pin)) == 0);  // 中断已禁用

    if (!ieMatch) {
        M5IOE1_LOG_E(TAG_IRQ, "Pin %d interrupt detach verification failed: IE=%c", pin, ieMatch ? 'Y' : 'N');
        return;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _callbacks[pin].callback    = nullptr;
    _callbacks[pin].callbackArg = nullptr;
    _callbacks[pin].arg         = nullptr;
    _callbacks[pin].enabled     = false;
    _pinStates[pin].intrEnabled = false;

    M5IOE1_LOG_I(TAG_IRQ, "Pin %d interrupt detached and verified", pin);
}

void M5IOE1::enableInterrupt(uint8_t pin)
{
    if (!_isValidPin(pin)) return;
    _callbacks[pin].enabled = true;
}

void M5IOE1::disableInterrupt(uint8_t pin)
{
    if (!_isValidPin(pin)) return;
    _callbacks[pin].enabled = false;
}

m5ioe1_err_t M5IOE1::getInterruptStatus(uint16_t* status)
{
    if (status == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_FAIL;

    if (!_readReg16(M5IOE1_REG_GPIO_IS_L, status)) {
        return M5IOE1_ERR_I2C_COMM;
    }
    M5IOE1_LOG_D(TAG_IRQ, "IRQ status: 0x%04X", *status);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::clearInterrupt()
{
    if (!_initialized) return M5IOE1_FAIL;

    // GPIO_IS 寄存器是"写0清除"语义，必须写入0来清除所有中断
    // GPIO_IS register uses "write 0 to clear" semantics, must write 0 to clear all interrupts
    if (!_writeReg16(M5IOE1_REG_GPIO_IS_L, 0)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IS register for clear");
        return M5IOE1_ERR_I2C_COMM;
    }

    M5IOE1_LOG_I(TAG_IRQ, "Interrupt cleared (all pins)");
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::clearInterrupt(uint8_t pin)
{
    if (!_initialized) return M5IOE1_FAIL;
    if (pin >= M5IOE1_MAX_GPIO_PINS) {
        M5IOE1_LOG_E(TAG_IRQ, "Invalid pin: %d", pin);
        return M5IOE1_ERR_INVALID_ARG;
    }

    // GPIO_IS 寄存器为"写 0 清除"语义：写 0 清除对应位，写 1 不影响
    // GPIO_IS register uses "write 0 to clear" semantics: write 0 clears the bit, write 1 has no effect
    uint16_t mask = ~(1U << pin) & 0xFFFF;
    if (!_writeReg16(M5IOE1_REG_GPIO_IS_L, mask)) {
        M5IOE1_LOG_E(TAG_IRQ, "Failed to write GPIO_IS register for pin %d", pin);
        return M5IOE1_ERR_I2C_COMM;
    }

    M5IOE1_LOG_I(TAG_IRQ, "Interrupt cleared (pin %d)", pin);
    return M5IOE1_OK;
}

// ============================
// ADC 功能
// ADC Functions
// ============================

m5ioe1_err_t M5IOE1::analogRead(uint8_t channel, uint16_t* result)
{
    if (result == nullptr) {
        M5IOE1_LOG_E(TAG_ADC, "analogRead result is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (channel < 1 || channel > 4) {
        M5IOE1_LOG_E(TAG_ADC, "Invalid ADC channel: %d", channel);
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_ADC, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    // 开始转换
    // Start conversion
    uint8_t ctrl = (channel & M5IOE1_ADC_CH_MASK) | M5IOE1_ADC_START;
    if (!_writeReg(M5IOE1_REG_ADC_CTRL, ctrl)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to write ADC_CTRL register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 等待完成
    // Wait for completion
    uint8_t reg = 0;
    int tries   = 0;
    do {
        M5IOE1_DELAY_MS(1);
        if (!_readReg(M5IOE1_REG_ADC_CTRL, &reg)) {
            M5IOE1_LOG_E(TAG_ADC, "Failed to read ADC_CTRL register");
            return M5IOE1_ERR_I2C_COMM;
        }
        M5IOE1_LOG_V(TAG_ADC, "ADC ch%d waiting... tries=%d status=0x%02X", channel, tries, reg);
        tries++;
    } while ((reg & M5IOE1_ADC_BUSY) && tries < 20);

    if (reg & M5IOE1_ADC_BUSY) {
        M5IOE1_LOG_E(TAG_ADC, "ADC conversion timeout on channel %d", channel);
        return M5IOE1_ERR_TIMEOUT;
    }

    if (!_readReg16(M5IOE1_REG_ADC_DATA_L, result)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to read ADC_DATA register");
        return M5IOE1_ERR_I2C_COMM;
    }
    M5IOE1_LOG_D(TAG_ADC, "ADC ch%d: raw=%u (tries=%d)", channel, *result, tries);
    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_ADC);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::isAdcBusy(bool* busy)
{
    if (busy == nullptr) {
        M5IOE1_LOG_E(TAG_ADC, "isAdcBusy busy is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_ADC, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t ctrl = 0;
    if (!_readReg(M5IOE1_REG_ADC_CTRL, &ctrl)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to read ADC_CTRL register");
        return M5IOE1_ERR_I2C_COMM;
    }

    *busy = (ctrl & M5IOE1_ADC_BUSY) != 0;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::disableAdc()
{
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_ADC_CTRL, 0)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to write ADC_CTRL register for disable");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCtrl = 0;
    if (!_readReg(M5IOE1_REG_ADC_CTRL, &actualCtrl)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to read back ADC_CTRL register for disable");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    // START/BUSY 位应该清零，通道位也应该为 0
    // START/BUSY bits should be cleared, channel bits should also be 0
    if (actualCtrl != 0) {
        M5IOE1_LOG_E(TAG_ADC, "ADC disable verification failed: expected=0, actual=0x%02X", actualCtrl);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_ADC);

    M5IOE1_LOG_I(TAG_ADC, "ADC disabled and verified");
    return M5IOE1_OK;
}

// ============================
// 温度传感器
// Temperature Sensor
// ============================

m5ioe1_err_t M5IOE1::readTemperature(uint16_t* temperature)
{
    if (temperature == nullptr) {
        M5IOE1_LOG_E(TAG_ADC, "readTemperature temperature is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_ADC, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    if (!_writeReg(M5IOE1_REG_TEMP_CTRL, M5IOE1_TEMP_START)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to write TEMP_CTRL register");
        return M5IOE1_ERR_I2C_COMM;
    }

    uint8_t ctrl = 0;
    int tries    = 0;
    do {
        M5IOE1_DELAY_MS(1);
        if (!_readReg(M5IOE1_REG_TEMP_CTRL, &ctrl)) {
            M5IOE1_LOG_E(TAG_ADC, "Failed to read TEMP_CTRL register");
            return M5IOE1_ERR_I2C_COMM;
        }
        tries++;
    } while ((ctrl & M5IOE1_TEMP_BUSY) && tries < 20);

    if (ctrl & M5IOE1_TEMP_BUSY) {
        M5IOE1_LOG_E(TAG_ADC, "Temperature conversion timeout");
        return M5IOE1_ERR_TIMEOUT;
    }

    if (!_readReg16(M5IOE1_REG_TEMP_DATA_L, temperature)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to read TEMP_DATA register");
        return M5IOE1_ERR_I2C_COMM;
    }
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::isTemperatureBusy(bool* busy)
{
    if (busy == nullptr) {
        M5IOE1_LOG_E(TAG_ADC, "isTemperatureBusy busy is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_ADC, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t ctrl = 0;
    if (!_readReg(M5IOE1_REG_TEMP_CTRL, &ctrl)) {
        M5IOE1_LOG_E(TAG_ADC, "Failed to read TEMP_CTRL register");
        return M5IOE1_ERR_I2C_COMM;
    }

    *busy = (ctrl & M5IOE1_TEMP_BUSY) != 0;
    return M5IOE1_OK;
}

// ============================
// PWM 功能
// PWM Functions
// ============================

m5ioe1_err_t M5IOE1::setPwmFrequency(uint16_t frequency)
{
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    M5IOE1_LOG_D(TAG_PWM, "setPwmFrequency: %d Hz", frequency);

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg16(M5IOE1_REG_PWM_FREQ_L, frequency)) {
        M5IOE1_LOG_E(TAG_PWM, "Failed to write PWM_FREQ register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualFreq = 0;
    if (!_readReg16(M5IOE1_REG_PWM_FREQ_L, &actualFreq)) {
        M5IOE1_LOG_E(TAG_PWM, "Failed to read back PWM_FREQ register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    if (actualFreq != frequency) {
        M5IOE1_LOG_E(TAG_PWM, "PWM frequency verification failed: expected=%d, actual=%d", frequency, actualFreq);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _pwmFrequency = frequency;
    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_PWM);

    M5IOE1_LOG_I(TAG_PWM, "PWM frequency set and verified: %d Hz", frequency);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getPwmFrequency(uint16_t* frequency)
{
    if (frequency == nullptr) {
        M5IOE1_LOG_E(TAG_PWM, "getPwmFrequency frequency is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_PWM, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    if (!_readReg16(M5IOE1_REG_PWM_FREQ_L, frequency)) {
        M5IOE1_LOG_E(TAG_PWM, "Failed to read PWM_FREQ register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 更新缓存
    // Update cache
    _pwmFrequency = *frequency;

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setPwmDuty(uint8_t channel, uint8_t duty, bool polarity, bool enable)
{
    if (channel > 3 || duty > 100) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    // 将百分比转换为 12 位 (0-4095)
    // Convert percentage to 12-bit (0-4095)
    uint16_t duty12 = (uint16_t)((duty * 0x0FFF) / 100);
    return setPwmDuty12bit(channel, duty12, polarity, enable);
}

m5ioe1_err_t M5IOE1::getPwmDuty(uint8_t channel, uint8_t* duty, bool* polarity, bool* enable)
{
    if (channel > 3) {
        M5IOE1_LOG_E(TAG_PWM, "Invalid PWM channel: %d", channel);
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (duty == nullptr || polarity == nullptr || enable == nullptr) {
        M5IOE1_LOG_E(TAG_PWM, "getPwmDuty output pointer is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_PWM, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t regL  = M5IOE1_REG_PWM1_DUTY_L + (channel * 2);
    uint16_t data = 0;
    if (!_readReg16(regL, &data)) {
        M5IOE1_LOG_E(TAG_PWM, "Failed to read PWM channel %d duty register", channel);
        return M5IOE1_ERR_I2C_COMM;
    }

    uint16_t duty12 = data & 0x0FFF;
    *duty           = (uint8_t)((duty12 * 100) / 0x0FFF);
    *polarity       = (data & ((uint16_t)M5IOE1_PWM_POLARITY << 8)) != 0;
    *enable         = (data & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) != 0;

    // 更新缓存
    // Update cache
    _pwmStates[channel].duty12   = duty12;
    _pwmStates[channel].polarity = *polarity;
    _pwmStates[channel].enabled  = *enable;

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setPwmDuty12bit(uint8_t channel, uint16_t duty12, bool polarity, bool enable)
{
    if (channel > 3 || duty12 > 0x0FFF) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    M5IOE1_LOG_D(TAG_PWM, "setPwmDuty12bit: ch=%d duty=%d pol=%s en=%s", channel, duty12, polarity ? "Inv" : "Norm",
                 enable ? "On" : "Off");

    // 获取对应的引脚
    // Get corresponding pin
    uint8_t pin = (channel == 0) ? 8 : (channel == 1) ? 7 : (channel == 2) ? 10 : 9;

    uint8_t regL = M5IOE1_REG_PWM1_DUTY_L + (channel * 2);

    uint8_t dataL = (uint8_t)(duty12 & 0xFF);
    uint8_t dataH = (uint8_t)((duty12 >> 8) & 0x0F);
    if (polarity) dataH |= M5IOE1_PWM_POLARITY;
    if (enable) dataH |= M5IOE1_PWM_ENABLE;

    uint8_t buf[2] = {dataL, dataH};

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeBytes(regL, buf, 2)) {
        M5IOE1_LOG_E(TAG_PWM, "Failed to write PWM channel %d duty register", channel);
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualData = 0;
    if (!_readReg16(regL, &actualData)) {
        M5IOE1_LOG_E(TAG_PWM, "Failed to read back PWM channel %d duty register", channel);
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    uint16_t actualDuty12 = actualData & 0x0FFF;
    bool actualPolarity   = (actualData & ((uint16_t)M5IOE1_PWM_POLARITY << 8)) != 0;
    bool actualEnable     = (actualData & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) != 0;

    if (actualDuty12 != duty12 || actualPolarity != polarity || actualEnable != enable) {
        M5IOE1_LOG_E(TAG_PWM, "PWM channel %d verification failed: duty=%d/%d, pol=%d/%d, en=%d/%d", channel, duty12,
                     actualDuty12, polarity, actualPolarity, enable, actualEnable);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 如果启用，将引脚设置为输出模式
    // Step 4: If enabled, set pin to output mode
    if (enable) {
        uint16_t modeReg = 0;
        if (_readReg16(M5IOE1_REG_GPIO_MODE_L, &modeReg)) {
            modeReg |= (1 << pin);
            if (!_writeReg16(M5IOE1_REG_GPIO_MODE_L, modeReg)) {
                M5IOE1_LOG_W(TAG_PWM, "Failed to set pin %d to output mode for PWM", pin);
            }
        }
    }

    // 步骤 5: 验证成功，更新缓存
    // Step 5: Verification passed, update cache
    _pwmStates[channel].duty12   = duty12;
    _pwmStates[channel].polarity = polarity;
    _pwmStates[channel].enabled  = enable;

    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_PWM | M5IOE1_SNAPSHOT_DOMAIN_GPIO);

    M5IOE1_LOG_I(TAG_PWM, "PWM ch%d verified: duty=%d pol=%s en=%s", channel, duty12, polarity ? "Inv" : "Norm",
                 enable ? "On" : "Off");

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setPwmConfig(uint8_t channel, bool enable, bool polarity, uint16_t frequency, uint16_t duty12)
{
    if (channel > 3 || duty12 > 0x0FFF) {
        M5IOE1_LOG_E(TAG_PWM, "Invalid PWM config: ch=%d duty12=%d", channel, duty12);
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_PWM, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    static const uint8_t kPwmPinMap[4] = {8, 7, 10, 9};
    uint8_t pin                        = kPwmPinMap[channel];

    m5ioe1_validation_t validation = validateConfig(pin, M5IOE1_CONFIG_PWM, enable);
    if (!validation.valid) {
        M5IOE1_LOG_W(TAG_PWM, "PWM config warning on IO%d: %s", pin + 1, validation.error_msg);
    }

    if (_pinStatesValid && _pinStates[pin].intrEnabled) {
        M5IOE1_LOG_W(TAG_PWM, "PWM pin IO%d has interrupt enabled", pin + 1);
    }

    if (_pwmStatesValid && _pwmFrequency != frequency) {
        for (uint8_t other = 0; other < M5IOE1_MAX_PWM_CHANNELS; other++) {
            if (other == channel) continue;
            if (_pwmStates[other].enabled) {
                M5IOE1_LOG_W(TAG_PWM, "PWM frequency change affects other channels: %d -> %d", _pwmFrequency,
                             frequency);
                break;
            }
        }
    }

    m5ioe1_err_t err = setPwmDuty12bit(channel, duty12, polarity, enable);
    if (err != M5IOE1_OK) return err;
    return setPwmFrequency(frequency);
}

m5ioe1_err_t M5IOE1::getPwmDuty12bit(uint8_t channel, uint16_t* duty12, bool* polarity, bool* enable)
{
    if (channel > 3 || duty12 == nullptr || polarity == nullptr || enable == nullptr) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    uint8_t regL  = M5IOE1_REG_PWM1_DUTY_L + (channel * 2);
    uint16_t data = 0;
    if (!_readReg16(regL, &data)) {
        return M5IOE1_ERR_I2C_COMM;
    }

    *duty12   = data & 0x0FFF;
    *polarity = (data & ((uint16_t)M5IOE1_PWM_POLARITY << 8)) != 0;
    *enable   = (data & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) != 0;

    // 更新缓存
    // Update cache
    _pwmStates[channel].duty12   = *duty12;
    _pwmStates[channel].polarity = *polarity;
    _pwmStates[channel].enabled  = *enable;

    return M5IOE1_OK;
}

// ============================
// Arduino 兼容 analogWrite
// Arduino-compatible analogWrite
// ============================

m5ioe1_err_t M5IOE1::analogWrite(uint8_t channel, uint8_t value)
{
    if (channel > 3) {
        M5IOE1_LOG_E(TAG_PWM, "Invalid channel: ch=%d", channel);
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_PWM, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    // 值为 0 时关闭 PWM 输出
    // Turn off PWM when value is 0
    if (value == 0) {
        return setPwmDuty12bit(channel, 0, false, false);
    }

    // 将 8-bit 值 (0-255) 缩放到 12-bit (0-4095)
    // Scale 8-bit value (0-255) to 12-bit (0-4095)
    // 公式：duty12 = (value * 4095) / 255 = value * 16 + value / 16
    // Formula: duty12 = (value * 4095) / 255 = value * 16 + value / 16
    uint16_t duty12 = (uint16_t)value * 16 + (uint16_t)value / 16;

    // 设置 PWM，默认启用输出，正常极性
    // Set PWM with output enabled, normal polarity
    return setPwmDuty12bit(channel, duty12, false, true);
}

// ============================
// NeoPixel LED 功能
// NeoPixel LED Functions
// ============================

m5ioe1_err_t M5IOE1::setLedCount(uint8_t count)
{
    if (count > M5IOE1_MAX_LED_COUNT) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    uint8_t cfg = count & M5IOE1_LED_NUM_MASK;

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_LED_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to write LED_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_LED_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to read back LED_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    // 只比对 LED 数量位 [5:0]，忽略 REFRESH 位 [6]
    // Only compare LED count bits [5:0], ignore REFRESH bit [6]
    bool countMatch = ((actualCfg & M5IOE1_LED_NUM_MASK) == (cfg & M5IOE1_LED_NUM_MASK));

    if (!countMatch) {
        M5IOE1_LOG_E(TAG_LED, "LED count verification failed: expected=%d, actual=%d", count,
                     actualCfg & M5IOE1_LED_NUM_MASK);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache

    M5IOE1_LOG_I(TAG_LED, "LED count set and verified: %d", count);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= M5IOE1_MAX_LED_COUNT) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    M5IOE1_LOG_D(TAG_LED, "setLedColor: idx=%d RGB(%d,%d,%d)", index, r, g, b);

    // 转换为 RGB565
    // Convert to RGB565
    uint16_t r5     = (r >> 3) & 0x1F;
    uint16_t g6     = (g >> 2) & 0x3F;
    uint16_t b5     = (b >> 3) & 0x1F;
    uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5;

    uint8_t regAddr = M5IOE1_REG_LED_RAM_START + (index * 2);
    uint8_t data[2] = {(uint8_t)(rgb565 & 0xFF), (uint8_t)((rgb565 >> 8) & 0xFF)};

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeBytes(regAddr, data, 2)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to write LED_RAM register for index %d", index);
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint16_t actualRgb565 = 0;
    uint8_t actualData[2];
    if (!_readBytes(regAddr, actualData, 2)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to read back LED_RAM register for index %d", index);
        return M5IOE1_ERR_I2C_COMM;
    }
    actualRgb565 = ((uint16_t)actualData[1] << 8) | actualData[0];

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    if (actualRgb565 != rgb565) {
        M5IOE1_LOG_E(TAG_LED, "LED color verification failed for index %d: expected=0x%04X, actual=0x%04X", index,
                     rgb565, actualRgb565);
        return M5IOE1_FAIL;
    }

    M5IOE1_LOG_I(TAG_LED, "LED color set and verified for index %d: RGB(%d,%d,%d)", index, r, g, b);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setLedColor(uint8_t index, m5ioe1_rgb_t color)
{
    return setLedColor(index, color.r, color.g, color.b);
}

m5ioe1_err_t M5IOE1::refreshLeds()
{
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_LED, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_LED_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to read LED_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    cfg |= M5IOE1_LED_REFRESH;
    if (!_writeReg(M5IOE1_REG_LED_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to write LED_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::disableLeds()
{
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_LED_CFG, 0)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to write LED_CFG register for disable");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_LED_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to read back LED_CFG register for disable");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    // LED 数量为 0 且 REFRESH 位为 0
    // LED count is 0 and REFRESH bit is 0
    if (actualCfg != 0) {
        M5IOE1_LOG_E(TAG_LED, "LED disable verification failed: expected=0, actual=0x%02X", actualCfg);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache

    M5IOE1_LOG_I(TAG_LED, "LEDs disabled and verified");
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::clearLedRam()
{
    M5IOE1_LOG_D(TAG_LED, "clearLedRam");
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_LED, "Device not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    // 清除所有 LED RAM (0x30 - 0x6F, 共 64 字节，支持 32 个 LED)
    // Clear all LED RAM (0x30 - 0x6F, 64 bytes total, supports 32 LEDs)
    uint8_t zeroData[64] = {0};

    M5IOE1_LOG_I(TAG_LED, "Clearing LED RAM...");

    // 分块写入，每次写入 8 字节，避免 I2C 缓冲区溢出
    // Write in chunks of 8 bytes to avoid I2C buffer overflow
    for (uint8_t offset = 0; offset < 64; offset += 8) {
        uint8_t regAddr = M5IOE1_REG_LED_RAM_START + offset;
        if (!_writeBytes(regAddr, zeroData, 8)) {
            M5IOE1_LOG_E(TAG_LED, "Failed to clear LED RAM at offset %d", offset);
            return M5IOE1_ERR_I2C_COMM;
        }
        // 每次写入后增加延迟，确保设备有时间处理
        // Add delay after each write to ensure device has time to process
        M5IOE1_DELAY_MS(2);
    }

    M5IOE1_LOG_I(TAG_LED, "LED RAM cleared successfully");
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setLeds(const m5ioe1_rgb_t* colors, uint8_t count, uint8_t arraySize, bool autoRefresh)
{
    // 步骤 1: 参数验证
    // Step 1: Parameter validation
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_LED, "Device not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    if (colors == nullptr) {
        M5IOE1_LOG_E(TAG_LED, "Colors array is null");
        return M5IOE1_ERR_INVALID_ARG;
    }

    if (count == 0) {
        M5IOE1_LOG_E(TAG_LED, "LED count cannot be 0");
        return M5IOE1_ERR_INVALID_ARG;
    }

    if (count > M5IOE1_MAX_LED_COUNT) {
        M5IOE1_LOG_E(TAG_LED, "LED count %d exceeds maximum %d", count, M5IOE1_MAX_LED_COUNT);
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 边界检查：确保不会越界访问数组
    // Bounds check: ensure no out-of-bounds array access
    if (count > arraySize) {
        M5IOE1_LOG_E(TAG_LED, "LED count %d exceeds array size %d (would cause out-of-bounds access)", count,
                     arraySize);
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 步骤 2: 设置 LED 数量
    // Step 2: Set LED count
    uint8_t cfg = count & M5IOE1_LED_NUM_MASK;
    if (!_writeReg(M5IOE1_REG_LED_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to write LED_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 验证 LED 数量设置
    // Verify LED count setting
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_LED_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_LED, "Failed to read back LED_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    if ((actualCfg & M5IOE1_LED_NUM_MASK) != cfg) {
        M5IOE1_LOG_E(TAG_LED, "LED count verification failed: expected=%d, actual=%d", cfg,
                     actualCfg & M5IOE1_LED_NUM_MASK);
        return M5IOE1_FAIL;
    }

    // 步骤 3: 写入所有 LED 颜色数据并回读验证
    // Step 3: Write all LED color data and read-back verification
    for (uint8_t i = 0; i < count; i++) {
        // 转换为 RGB565
        // Convert to RGB565
        uint16_t r5     = (colors[i].r >> 3) & 0x1F;
        uint16_t g6     = (colors[i].g >> 2) & 0x3F;
        uint16_t b5     = (colors[i].b >> 3) & 0x1F;
        uint16_t rgb565 = (r5 << 11) | (g6 << 5) | b5;

        uint8_t regAddr = M5IOE1_REG_LED_RAM_START + (i * 2);
        uint8_t data[2] = {(uint8_t)(rgb565 & 0xFF), (uint8_t)((rgb565 >> 8) & 0xFF)};

        if (!_writeBytes(regAddr, data, 2)) {
            M5IOE1_LOG_E(TAG_LED, "Failed to write LED_RAM for index %d", i);
            return M5IOE1_ERR_I2C_COMM;
        }

        // 回读验证颜色数据
        // Read-back verification for color data
        uint8_t actualData[2];
        if (!_readBytes(regAddr, actualData, 2)) {
            M5IOE1_LOG_E(TAG_LED, "Failed to read back LED_RAM for index %d", i);
            return M5IOE1_ERR_I2C_COMM;
        }
        uint16_t actualRgb565 = ((uint16_t)actualData[1] << 8) | actualData[0];
        if (actualRgb565 != rgb565) {
            M5IOE1_LOG_E(TAG_LED, "LED color verification failed for index %d: expected=0x%04X, actual=0x%04X", i,
                         rgb565, actualRgb565);
            return M5IOE1_FAIL;
        }
    }

    // 步骤 4: 更新缓存
    // Step 4: Update cache

    // 步骤 5: 可选刷新
    // Step 5: Optional refresh
    if (autoRefresh) {
        if (!_readReg(M5IOE1_REG_LED_CFG, &actualCfg)) {
            M5IOE1_LOG_E(TAG_LED, "Failed to read LED_CFG for refresh");
            return M5IOE1_ERR_I2C_COMM;
        }
        actualCfg |= M5IOE1_LED_REFRESH;
        if (!_writeReg(M5IOE1_REG_LED_CFG, actualCfg)) {
            M5IOE1_LOG_E(TAG_LED, "Failed to trigger LED refresh");
            return M5IOE1_ERR_I2C_COMM;
        }
    }

    M5IOE1_LOG_I(TAG_LED, "Set %d LEDs successfully%s", count, autoRefresh ? " (refreshed)" : "");
    return M5IOE1_OK;
}

// ============================
// AW8737A 脉冲功能
// AW8737A Pulse Functions
// ============================

m5ioe1_err_t M5IOE1::setAw8737aPulse(uint8_t pin, m5ioe1_aw8737a_pulse_t pulseNum, m5ioe1_aw8737a_refresh_t refresh)
{
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_AMP, "Device not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    // 验证引脚范围 (0-13)
    // Validate pin range (0-13)
    if (pin >= M5IOE1_MAX_GPIO_PINS) {
        M5IOE1_LOG_E(TAG_AMP, "Invalid pin number: %d (valid range: 0-%d)", pin, M5IOE1_MAX_GPIO_PINS - 1);
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 验证脉冲编号 (0-3)
    // Validate pulse number (0-3)
    if (pulseNum > M5IOE1_AW8737A_PULSE_3) {
        M5IOE1_LOG_E(TAG_AMP, "Invalid pulse number: %d (valid range: 0-3)", pulseNum);
        return M5IOE1_ERR_INVALID_ARG;
    }

    // 检查引脚是否为输出模式，如果不是则自动配置为推挽输出
    // Check if pin is output mode, if not auto-configure as push-pull output
    if (!_pinStates[pin].isOutput) {
        M5IOE1_LOG_I(TAG_AMP, "AW8737A: Pin %d not output mode, auto-configuring as push-pull output", pin);
        pinMode(pin, OUTPUT);
    }

    // 保存配置到成员变量
    // Save configuration to member variables
    _aw8737aConfigured = true;
    _aw8737aPin        = pin;
    _aw8737aPulseNum   = pulseNum;

    // 构建寄存器值（不含 REFRESH 位）
    // Build register value (without REFRESH bit)
    // [7] REFRESH | [6:5] NUM[1:0] | [4:0] GPIO[4:0]
    uint8_t regValue = 0;
    regValue |= (pin & M5IOE1_AW8737A_GPIO_MASK);                                    // 位[4:0]: GPIO 选择
                                                                                     // Bits[4:0]: GPIO selection
    regValue |= ((pulseNum & M5IOE1_AW8737A_NUM_MASK) << M5IOE1_AW8737A_NUM_SHIFT);  // 位[6:5]: 脉冲编号
                                                                                     // Bits[6:5]: Pulse number

    // 写入寄存器（不含 REFRESH 位）
    // Write register (without REFRESH bit)
    if (!_writeReg(M5IOE1_REG_AW8737A_PULSE, regValue)) {
        M5IOE1_LOG_E(TAG_AMP, "Failed to set AW8737A pulse config");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 更新缓存
    // Update cache
    _aw8737aRegValue   = regValue;
    _aw8737aStateValid = true;

    // 回读验证
    // Read-back verification
    uint8_t actualReg = 0;
    if (!_readReg(M5IOE1_REG_AW8737A_PULSE, &actualReg)) {
        M5IOE1_LOG_E(TAG_AMP, "Failed to read back AW8737A pulse register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 验证关键位是否匹配
    // Verify critical bits match
    uint8_t actualValue = actualReg & 0x7F;
    if (actualValue != regValue) {
        M5IOE1_LOG_E(TAG_AMP, "AW8737A pulse verification failed: expected=0x%02X, actual=0x%02X", regValue,
                     actualValue);
        return M5IOE1_FAIL;
    }

    M5IOE1_LOG_I(TAG_AMP, "AW8737A pulse configured: pin=%d, num=%d (reg=0x%02X)", pin, pulseNum, regValue);

    // 如果需要立即刷新，调用 refreshAw8737aPulse
    // If immediate refresh needed, call refreshAw8737aPulse
    if (refresh == M5IOE1_AW8737A_REFRESH_NOW) {
        return refreshAw8737aPulse();
    }

    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_AW8737A);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::refreshAw8737aPulse()
{
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_AMP, "Device not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    // 检查是否已配置
    // Check if configured
    if (!_aw8737aConfigured) {
        M5IOE1_LOG_W(TAG_AMP, "AW8737A pulse not configured, executing anyway");
    }

    // 检查引脚是否为输出模式，如果不是则自动配置为推挽输出
    // Check if pin is output mode, if not auto-configure as push-pull output
    if (!_pinStates[_aw8737aPin].isOutput) {
        M5IOE1_LOG_I(TAG_AMP, "AW8737A: Pin %d not output mode, auto-configuring as push-pull output", _aw8737aPin);
        pinMode(_aw8737aPin, OUTPUT);
    }

    // 读取当前寄存器值
    // Read current register value
    uint8_t regValue = 0;
    if (!_readReg(M5IOE1_REG_AW8737A_PULSE, &regValue)) {
        M5IOE1_LOG_E(TAG_AMP, "Failed to read AW8737A pulse register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 设置 REFRESH 位
    // Set REFRESH bit
    regValue |= M5IOE1_AW8737A_REFRESH;

    if (!_writeReg(M5IOE1_REG_AW8737A_PULSE, regValue)) {
        M5IOE1_LOG_E(TAG_AMP, "Failed to refresh AW8737A pulse");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 输出详细日志
    // Output detailed log
    const char* driveStr = (_pinStates[_aw8737aPin].drive == M5IOE1_DRIVE_PUSHPULL) ? "PUSH-PULL" : "OPEN-DRAIN";
    M5IOE1_LOG_I(TAG_AMP, "AW8737A pulse refresh: pin=%d, pulseNum=%d, mode=OUTPUT, drive=%s", _aw8737aPin,
                 _aw8737aPulseNum, driveStr);

    // 写入位 7 后等待 20ms，因为它会影响 I2C 通信
    // Wait 20ms after writing bit 7, as it affects I2C communication
    M5IOE1_DELAY_MS(20);

    _autoSnapshotUpdate(M5IOE1_SNAPSHOT_DOMAIN_AW8737A);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setAw8737aMode(uint8_t pin, m5ioe1_aw8737a_mode_t mode, m5ioe1_aw8737a_refresh_t refresh)
{
    // Mode 直接映射到 Pulse (MODE_1=0pulse, MODE_2=1pulse, MODE_3=2pulses, MODE_4=3pulses)
    // Mode directly maps to Pulse
    if (mode > M5IOE1_AW8737A_MODE_4) {
        M5IOE1_LOG_E(TAG_AMP, "Invalid AW8737A mode: %d (valid range: 0-3)", mode);
        return M5IOE1_ERR_INVALID_ARG;
    }

    return setAw8737aPulse(pin, (m5ioe1_aw8737a_pulse_t)mode, refresh);
}

m5ioe1_err_t M5IOE1::refreshAw8737aMode()
{
    return refreshAw8737aPulse();
}

// ============================
// RTC RAM 功能
// RTC RAM Functions
// ============================

m5ioe1_err_t M5IOE1::writeRtcRAM(uint8_t offset, const uint8_t* data, uint8_t length)
{
    if (data == nullptr || offset >= M5IOE1_RTC_RAM_SIZE || length == 0 || (offset + length) > M5IOE1_RTC_RAM_SIZE) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t regAddr = M5IOE1_REG_RTC_RAM_START + offset;

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeBytes(regAddr, data, length)) {
        M5IOE1_LOG_E(TAG_SYS, "Failed to write RTC_RAM register at offset %d", offset);
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualData[M5IOE1_RTC_RAM_SIZE];
    if (!_readBytes(regAddr, actualData, length)) {
        M5IOE1_LOG_E(TAG_SYS, "Failed to read back RTC_RAM register at offset %d", offset);
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    // 逐字节比较写入的数据
    // Compare written data byte by byte
    bool allMatch = true;
    for (uint8_t i = 0; i < length; i++) {
        if (actualData[i] != data[i]) {
            allMatch = false;
            M5IOE1_LOG_E(TAG_SYS, "RTC_RAM verification failed at offset %d: expected=0x%02X, actual=0x%02X",
                         offset + i, data[i], actualData[i]);
            break;
        }
    }

    if (!allMatch) {
        return M5IOE1_FAIL;
    }

    M5IOE1_LOG_I(TAG_SYS, "RTC_RAM write and verified: offset=%d, length=%d", offset, length);
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::readRtcRAM(uint8_t offset, uint8_t* data, uint8_t length)
{
    if (data == nullptr || offset >= M5IOE1_RTC_RAM_SIZE || length == 0 || (offset + length) > M5IOE1_RTC_RAM_SIZE) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t regAddr = M5IOE1_REG_RTC_RAM_START + offset;
    return _readBytes(regAddr, data, length) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

// ============================
// 系统配置
// System Configuration
// ============================

m5ioe1_err_t M5IOE1::setI2cConfig(uint8_t sleepTime, m5ioe1_i2c_speed_t speed, m5ioe1_wake_edge_t wakeEdge,
                                  m5ioe1_pull_config_t pullConfig)
{
    if (sleepTime > 15) {
        M5IOE1_LOG_E(TAG_I2C, "Invalid I2C sleep time: %d", sleepTime);
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint32_t targetFreq = (speed == M5IOE1_I2C_SPEED_400K) ? M5IOE1_I2C_FREQ_400K : M5IOE1_I2C_FREQ_100K;
    bool speedChanged   = (targetFreq != _requestedSpeed);

    uint8_t cfg = (sleepTime & M5IOE1_I2C_SLEEP_MASK);
    if (speed == M5IOE1_I2C_SPEED_400K) cfg |= M5IOE1_I2C_SPEED_400K_BIT;
    if (wakeEdge == M5IOE1_WAKE_EDGE_RISING) cfg |= M5IOE1_I2C_WAKE_RISING;
    if (pullConfig == M5IOE1_PULL_DISABLED) cfg |= M5IOE1_I2C_PULL_OFF;

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_I2C_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to write I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read back I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证关键位是否匹配
    // Step 3: Verify critical bits match
    uint8_t expectedSleep = sleepTime & M5IOE1_I2C_SLEEP_MASK;
    uint8_t actualSleep   = actualCfg & M5IOE1_I2C_SLEEP_MASK;
    bool actualSpeed400k  = (actualCfg & M5IOE1_I2C_SPEED_400K_BIT) != 0;
    bool actualWakeRising = (actualCfg & M5IOE1_I2C_WAKE_RISING) != 0;
    bool actualPullOff    = (actualCfg & M5IOE1_I2C_PULL_OFF) != 0;

    if (actualSleep != expectedSleep || actualSpeed400k != (speed == M5IOE1_I2C_SPEED_400K) ||
        actualWakeRising != (wakeEdge == M5IOE1_WAKE_EDGE_RISING) ||
        actualPullOff != (pullConfig == M5IOE1_PULL_DISABLED)) {
        M5IOE1_LOG_E(TAG_I2C, "I2C_CFG verification failed: expected=0x%02X, actual=0x%02X", cfg, actualCfg);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 如果速度改变，切换主机 I2C 总线
    // Step 4: If speed changed, switch host I2C bus
    if (speedChanged) {
        M5IOE1_LOG_D(TAG_I2C, "Switching host I2C bus from %lu Hz to %lu Hz", _requestedSpeed, targetFreq);

#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            _commFreq = targetFreq;
            M5IOE1_LOG_D(TAG_I2C, "M5Unified I2C frequency updated to %lu Hz", targetFreq);
        } else
#endif
            if (_wire != nullptr) {
            _wire->end();
            M5IOE1_DELAY_MS(10);
            if (!_wire->begin(_sda, _scl, targetFreq)) {
                M5IOE1_LOG_E(TAG_I2C, "Failed to reinitialize I2C bus at %lu Hz", targetFreq);
                // 回滚设备配置
                // Rollback device config
                uint8_t rollbackCfg = actualCfg;
                if (speed == M5IOE1_I2C_SPEED_400K) {
                    rollbackCfg &= ~M5IOE1_I2C_SPEED_400K_BIT;
                } else {
                    rollbackCfg |= M5IOE1_I2C_SPEED_400K_BIT;
                }
                _writeReg(M5IOE1_REG_I2C_CFG, rollbackCfg);
                return M5IOE1_ERR_I2C_CONFIG;
            }
            M5IOE1_DELAY_MS(10);
        }
#else
        esp_err_t ret;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                if (_i2c_master_dev != nullptr) {
                    ret = i2c_master_bus_rm_device(_i2c_master_dev);
                    if (ret != ESP_OK) {
                        M5IOE1_LOG_E(TAG_I2C, "Failed to remove I2C device: %s", esp_err_to_name(ret));
                        return M5IOE1_ERR_I2C_CONFIG;
                    }
                    _i2c_master_dev = nullptr;

                    i2c_device_config_t dev_config = {
                        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                        .device_address  = _addr,
                        .scl_speed_hz    = targetFreq,
                        .scl_wait_us     = 0,
                        .flags           = {.disable_ack_check = false},
                    };

                    ret = i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
                    if (ret != ESP_OK) {
                        M5IOE1_LOG_E(TAG_I2C, "Failed to add I2C device at %lu Hz: %s", targetFreq,
                                     esp_err_to_name(ret));
                        return M5IOE1_ERR_I2C_CONFIG;
                    }
                }
                break;
#endif  // M5IOE1_HAS_I2C_MASTER

#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                if (_i2c_device != nullptr) {
                    ret = i2c_bus_device_delete(&_i2c_device);
                    if (ret != ESP_OK) {
                        M5IOE1_LOG_E(TAG_I2C, "Failed to delete I2C device: %s", esp_err_to_name(ret));
                        return M5IOE1_ERR_I2C_CONFIG;
                    }

                    _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, targetFreq);
                    if (_i2c_device == nullptr) {
                        M5IOE1_LOG_E(TAG_I2C, "Failed to create I2C device at %lu Hz", targetFreq);
                        return M5IOE1_ERR_I2C_CONFIG;
                    }
                }
                break;
#endif  // M5IOE1_HAS_I2C_BUS

#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY: {
                i2c_config_t i2c_conf     = {};
                i2c_conf.mode             = I2C_MODE_MASTER;
                i2c_conf.sda_io_num       = _sda;
                i2c_conf.scl_io_num       = _scl;
                i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
                i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
                i2c_conf.master.clk_speed = targetFreq;
                ret                       = i2c_param_config(_port, &i2c_conf);
                if (ret != ESP_OK) {
                    M5IOE1_LOG_E(TAG_I2C, "i2c_param_config failed: %s", esp_err_to_name(ret));
                    return M5IOE1_ERR_I2C_CONFIG;
                }
                break;
            }
#endif  // !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS

#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                _commFreq = targetFreq;
                M5IOE1_LOG_D(TAG_I2C, "M5Unified I2C frequency updated to %lu Hz", targetFreq);
                break;
#endif
            default:
                break;
        }
#endif

        // 步骤 5: 验证通信仍然有效
        // Step 5: Verify communication still works
        uint16_t uid = 0;
        if (!_readReg16(M5IOE1_REG_UID_L, &uid)) {
            M5IOE1_LOG_E(TAG_I2C, "Communication failed after switching to %lu Hz", targetFreq);
            // 回滚
            // Rollback
            uint32_t originalFreq = _requestedSpeed;
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
            if (_m5_i2c) {
                _commFreq = originalFreq;
            } else
#endif
                if (_wire != nullptr) {
                _wire->end();
                M5IOE1_DELAY_MS(10);
                _wire->begin(_sda, _scl, originalFreq);
                M5IOE1_DELAY_MS(10);
            }
#else
            switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
                case M5IOE1_I2C_DRIVER_SELF_CREATED:
                case M5IOE1_I2C_DRIVER_MASTER:
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
#endif  // M5IOE1_HAS_I2C_MASTER
#if M5IOE1_HAS_I2C_BUS
                case M5IOE1_I2C_DRIVER_BUS:
                    if (_i2c_device != nullptr) {
                        i2c_bus_device_delete(&_i2c_device);
                        _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, originalFreq);
                    }
                    break;
#endif  // M5IOE1_HAS_I2C_BUS
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
                case M5IOE1_I2C_DRIVER_LEGACY: {
                    i2c_config_t i2c_conf     = {};
                    i2c_conf.mode             = I2C_MODE_MASTER;
                    i2c_conf.sda_io_num       = _sda;
                    i2c_conf.scl_io_num       = _scl;
                    i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
                    i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
                    i2c_conf.master.clk_speed = originalFreq;
                    i2c_param_config(_port, &i2c_conf);  // best effort rollback
                    break;
                }
#endif  // !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
#if M5IOE1_HAS_M5UNIFIED_I2C
                case M5IOE1_I2C_DRIVER_M5UNIFIED:
                    _commFreq = originalFreq;
                    break;
#endif
                default:
                    break;
            }
#endif
            // 回滚设备配置
            // Rollback device config
            uint8_t rollbackCfg = actualCfg;
            if (speed == M5IOE1_I2C_SPEED_400K) {
                rollbackCfg &= ~M5IOE1_I2C_SPEED_400K_BIT;
            } else {
                rollbackCfg |= M5IOE1_I2C_SPEED_400K_BIT;
            }
            _writeReg(M5IOE1_REG_I2C_CFG, rollbackCfg);
            return M5IOE1_ERR_I2C_COMM;
        }

        // 更新请求的速度
        // Update requested speed
        _requestedSpeed = targetFreq;
        M5IOE1_LOG_D(TAG_I2C, "Host I2C bus switched to %lu Hz", targetFreq);
    }

    // 步骤 6: 验证成功，更新缓存
    // Step 6: Verification passed, update cache
    _i2cConfig.sleepTime  = sleepTime;
    _i2cConfig.speed400k  = (speed == M5IOE1_I2C_SPEED_400K);
    _i2cConfig.wakeRising = (wakeEdge == M5IOE1_WAKE_EDGE_RISING);
    _i2cConfig.pullOff    = (pullConfig == M5IOE1_PULL_DISABLED);
    _i2cConfigValid       = true;

    // 若配置了休眠时间，自动启用 autoWake 防止数据丢失
    // If sleep time is configured, auto-enable autoWake to prevent data loss
    if (sleepTime > 0 && !_autoWakeEnabled) {
        setAutoWakeEnable(true);
        M5IOE1_LOG_W(TAG_I2C, "I2C sleep enabled (sleepTime=%d), auto-wake automatically enabled", sleepTime);
    }

    _updatePollingForI2cSleep(sleepTime);

    M5IOE1_LOG_I(TAG_I2C, "I2C config set and verified: sleep=%d, speed=%s, wake=%s, pull=%s", sleepTime,
                 speed == M5IOE1_I2C_SPEED_400K ? "400K" : "100K",
                 wakeEdge == M5IOE1_WAKE_EDGE_RISING ? "rising" : "falling",
                 pullConfig == M5IOE1_PULL_DISABLED ? "off" : "on");

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setI2cSleepTime(uint8_t sleepTime)
{
    if (sleepTime > 15) {
        M5IOE1_LOG_E(TAG_I2C, "Invalid I2C sleep time: %d", sleepTime);
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 清除 SLEEP 位 [3:0]，设置新值
    // Clear SLEEP bits [3:0], set new value
    cfg = (cfg & ~M5IOE1_I2C_SLEEP_MASK) | (sleepTime & M5IOE1_I2C_SLEEP_MASK);

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_I2C_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to write I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read back I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证 SLEEP 位是否匹配
    // Step 3: Verify SLEEP bits match
    uint8_t actualSleep = actualCfg & M5IOE1_I2C_SLEEP_MASK;
    if (actualSleep != sleepTime) {
        M5IOE1_LOG_E(TAG_I2C, "I2C_CFG sleep time verification failed: expected=%d, actual=%d", sleepTime, actualSleep);
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _i2cConfig.sleepTime = sleepTime;
    _i2cConfigValid      = true;

    M5IOE1_LOG_I(TAG_I2C, "I2C sleep time set and verified to %d", sleepTime);

    // 若配置了休眠时间，自动启用 autoWake 防止数据丢失
    // If sleep time is configured, auto-enable autoWake to prevent data loss
    if (sleepTime > 0 && !_autoWakeEnabled) {
        setAutoWakeEnable(true);
        M5IOE1_LOG_W(TAG_I2C, "I2C sleep enabled, auto-wake automatically enabled");
    }

    _updatePollingForI2cSleep(sleepTime);

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getI2cSleepTime(uint8_t* sleepTime)
{
    if (sleepTime == nullptr) {
        M5IOE1_LOG_E(TAG_I2C, "getI2cSleepTime sleepTime is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 更新缓存
    // Update cache
    _i2cConfig.sleepTime = cfg & M5IOE1_I2C_SLEEP_MASK;
    _i2cConfigValid      = true;

    *sleepTime = _i2cConfig.sleepTime;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setI2cWakeEdge(m5ioe1_wake_edge_t edge)
{
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 设置或清除 WAKE 位 [5]
    // Set or clear WAKE bit [5]
    if (edge == M5IOE1_WAKE_EDGE_RISING) {
        cfg |= M5IOE1_I2C_WAKE_RISING;
    } else {
        cfg &= ~M5IOE1_I2C_WAKE_RISING;
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_I2C_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to write I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read back I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证 WAKE 位是否匹配
    // Step 3: Verify WAKE bit matches
    bool actualWakeRising   = (actualCfg & M5IOE1_I2C_WAKE_RISING) != 0;
    bool expectedWakeRising = (edge == M5IOE1_WAKE_EDGE_RISING);
    if (actualWakeRising != expectedWakeRising) {
        M5IOE1_LOG_E(TAG_I2C, "I2C_CFG wake edge verification failed: expected=%s, actual=%s",
                     expectedWakeRising ? "rising" : "falling", actualWakeRising ? "rising" : "falling");
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _i2cConfig.wakeRising = expectedWakeRising;
    _i2cConfigValid       = true;

    M5IOE1_LOG_I(TAG_I2C, "I2C wake edge set and verified to %s", expectedWakeRising ? "rising" : "falling");

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getI2cWakeEdge(m5ioe1_wake_edge_t* edge)
{
    if (edge == nullptr) {
        M5IOE1_LOG_E(TAG_I2C, "getI2cWakeEdge edge is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 更新缓存
    // Update cache
    _i2cConfig.wakeRising = (cfg & M5IOE1_I2C_WAKE_RISING) != 0;
    _i2cConfigValid       = true;

    *edge = _i2cConfig.wakeRising ? M5IOE1_WAKE_EDGE_RISING : M5IOE1_WAKE_EDGE_FALLING;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::setI2cPullConfig(m5ioe1_pull_config_t config)
{
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 设置或清除 INT_PU/PD 位 [6]
    // Set or clear INT_PU/PD bit [6]
    if (config == M5IOE1_PULL_DISABLED) {
        cfg |= M5IOE1_I2C_PULL_OFF;
    } else {
        cfg &= ~M5IOE1_I2C_PULL_OFF;
    }

    // 步骤 1: 写入寄存器
    // Step 1: Write register
    if (!_writeReg(M5IOE1_REG_I2C_CFG, cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to write I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 回读验证
    // Step 2: Read-back verification
    uint8_t actualCfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &actualCfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read back I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 3: 验证 INT_PU/PD 位是否匹配
    // Step 3: Verify INT_PU/PD bit matches
    bool actualPullOff   = (actualCfg & M5IOE1_I2C_PULL_OFF) != 0;
    bool expectedPullOff = (config == M5IOE1_PULL_DISABLED);
    if (actualPullOff != expectedPullOff) {
        M5IOE1_LOG_E(TAG_I2C, "I2C_CFG pull config verification failed: expected=%s, actual=%s",
                     expectedPullOff ? "disabled" : "enabled", actualPullOff ? "disabled" : "enabled");
        return M5IOE1_FAIL;
    }

    // 步骤 4: 验证成功，更新缓存
    // Step 4: Verification passed, update cache
    _i2cConfig.pullOff = expectedPullOff;
    _i2cConfigValid    = true;

    M5IOE1_LOG_I(TAG_I2C, "I2C internal pull-up set and verified to %s", expectedPullOff ? "disabled" : "enabled");

    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getI2cPullConfig(m5ioe1_pull_config_t* config)
{
    if (config == nullptr) {
        M5IOE1_LOG_E(TAG_I2C, "getI2cPullConfig config is null");
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C_CFG register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 更新缓存
    // Update cache
    _i2cConfig.pullOff = (cfg & M5IOE1_I2C_PULL_OFF) != 0;
    _i2cConfigValid    = true;

    *config = _i2cConfig.pullOff ? M5IOE1_PULL_DISABLED : M5IOE1_PULL_ENABLED;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::factoryReset()
{
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    if (!_writeReg(M5IOE1_REG_FACTORY_RESET, M5IOE1_FACTORY_RESET_KEY)) {
        return M5IOE1_ERR_I2C_COMM;
    }

    M5IOE1_DELAY_MS(100);
    _initialized    = false;
    _pinStatesValid = false;
    _pwmStatesValid = false;
    _adcStateValid  = false;
    _clearPinStates();
    _clearPwmStates();
    _clearAdcState();
    M5IOE1_LOG_W(TAG_SYS, "Factory reset complete. Call begin() to reinitialize.");

    return M5IOE1_OK;
}

// ============================
// 自动唤醒功能
// Auto Wake Feature
// ============================

void M5IOE1::setAutoWakeEnable(bool enable)
{
    _autoWakeEnabled = enable;
    if (enable) {
        _lastCommTime = M5IOE1_GET_TIME_MS();
    }
    M5IOE1_LOG_I(TAG_I2C, "Auto wake %s", enable ? "enabled" : "disabled");
}

bool M5IOE1::isAutoWakeEnabled() const
{
    return _autoWakeEnabled;
}

m5ioe1_err_t M5IOE1::sendWakeSignal()
{
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
    if (_m5_i2c) {
        M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq);
        return M5IOE1_OK;
    }
#endif
    M5IOE1_I2C_ARDUINO_SEND_WAKE(_wire, _addr);
    return M5IOE1_OK;
#else
    switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
        case M5IOE1_I2C_DRIVER_SELF_CREATED:
        case M5IOE1_I2C_DRIVER_MASTER:
            return M5IOE1_I2C_MASTER_SEND_WAKE(_i2c_master_bus, _addr) == ESP_OK ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
#endif
#if M5IOE1_HAS_I2C_BUS
        case M5IOE1_I2C_DRIVER_BUS:
            return M5IOE1_I2C_BUS_SEND_WAKE(_i2c_device, M5IOE1_REG_REV) == ESP_OK ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
        case M5IOE1_I2C_DRIVER_LEGACY:
            return M5IOE1_I2C_LEGACY_SEND_WAKE(_port, _addr) == ESP_OK ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
        case M5IOE1_I2C_DRIVER_M5UNIFIED:
            return M5IOE1_M5UNIFIED_SEND_WAKE(_m5_i2c, _addr, _commFreq) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
#endif
        default:
            return M5IOE1_ERR_INTERNAL;
    }
#endif
}

void M5IOE1::_checkAutoWake()
{
    if (!_autoWakeEnabled || !_i2cConfigValid || _i2cConfig.sleepTime == 0) return;

    uint32_t now     = M5IOE1_GET_TIME_MS();
    uint32_t elapsed = now - _lastCommTime;

    // 如果距离上次通信超过1秒，发送唤醒信号
    // If more than 1 second since last communication, send wake signal
    if (elapsed >= 1000) {
        sendWakeSignal();
        M5IOE1_DELAY_MS(10);
    }
    _lastCommTime = now;
}

void M5IOE1::_updatePollingForI2cSleep(uint8_t sleepTime)
{
    if (_intMode != M5IOE1_INT_MODE_POLLING) return;

#ifdef ARDUINO
    if (sleepTime > 0) {
        if (_pollTask != nullptr) {
            _cleanupPollingArduino();
        }
    } else {
        if (_pollTask == nullptr) {
            _setupPollingArduino();
        }
    }
#else
    if (sleepTime > 0) {
        if (_pollTask != nullptr) {
            _cleanupPolling();
        }
    } else {
        if (_pollTask == nullptr) {
            _setupPolling();
        }
    }
#endif
}

// ============================
// 状态快照功能
// State Snapshot Functions
// ============================

void M5IOE1::setAutoSnapshot(bool enable)
{
    _autoSnapshot = enable;
}

bool M5IOE1::isAutoSnapshotEnabled() const
{
    return _autoSnapshot;
}

m5ioe1_err_t M5IOE1::updateSnapshot()
{
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    bool gpio    = _snapshotPinStates();
    bool pwm     = _snapshotPwmStates();
    bool adc     = _snapshotAdcState();
    bool aw8737a = _snapshotAw8737a();

    return (gpio && pwm && adc && aw8737a) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

// ============================
// 调试功能
// Debug Functions
// ============================

m5ioe1_err_t M5IOE1::getModeReg(uint16_t* reg)
{
    if (reg == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_GPIO_MODE_L, reg) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getOutputReg(uint16_t* reg)
{
    if (reg == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_GPIO_OUT_L, reg) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getInputReg(uint16_t* reg)
{
    if (reg == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_GPIO_IN_L, reg) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getPullUpReg(uint16_t* reg)
{
    if (reg == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_GPIO_PU_L, reg) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getPullDownReg(uint16_t* reg)
{
    if (reg == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_GPIO_PD_L, reg) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

m5ioe1_err_t M5IOE1::getDriveReg(uint16_t* reg)
{
    if (reg == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;
    return _readReg16(M5IOE1_REG_GPIO_DRV_L, reg) ? M5IOE1_OK : M5IOE1_ERR_I2C_COMM;
}

// ============================
// 配置验证
// Configuration Validation
// ============================

m5ioe1_validation_t M5IOE1::validateConfig(uint8_t pin, m5ioe1_config_type_t configType, bool enable)
{
    m5ioe1_validation_t result = {false, {0}, 0xFF};

    // 基本验证
    // Basic validation
    if (!_isValidPin(pin)) {
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid pin %d", pin);
        return result;
    }

    if (!_initialized) {
        snprintf(result.error_msg, sizeof(result.error_msg), "Not initialized");
        return result;
    }

    // 如果禁用，无需冲突检查
    // If disabling, no conflict check needed
    if (!enable) {
        result.valid = true;
        return result;
    }

    uint8_t mutexPin = 0xFF;

    switch (configType) {
        case M5IOE1_CONFIG_GPIO_INPUT:
        case M5IOE1_CONFIG_GPIO_OUTPUT:
            // 检查引脚是否用于特殊功能
            // Check if pin is being used for special functions
            if (_hasActivePwm(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg),
                         "Pin %d is used for PWM. Disable first (e.g. setPwmDuty)", pin);
                return result;
            }
            if (_hasActiveAdc(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg),
                         "Pin %d is used for ADC. Disable first (disableAdc())", pin);
                return result;
            }
            if (_isNeopixelPin(pin) && _isLedEnabled()) {
                snprintf(result.error_msg, sizeof(result.error_msg),
                         "Pin %d (IO14) used for NeoPixel; call disableLeds()", pin);
                return result;
            }
            break;

        case M5IOE1_CONFIG_GPIO_INTERRUPT:
            // 检查中断互斥约束
            // Check interrupt mutex constraint
            if (_getInterruptMutexPin(pin, &mutexPin)) {
                if (_hasActiveInterrupt(mutexPin)) {
                    snprintf(result.error_msg, sizeof(result.error_msg), "Interrupt conflict: IO%d and IO%d are mutex",
                             pin + 1, mutexPin + 1);
                    result.conflicting_pin = mutexPin;
                    return result;
                }
            }
            // 检查 IO5 的 I2C 睡眠模式冲突
            // Check I2C sleep mode conflict for IO5
            if (pin == 4 && _hasI2cSleepEnabled()) {  // IO5 is pin index 4
                snprintf(result.error_msg, sizeof(result.error_msg), "IO5 interrupt disabled when I2C sleep enabled");
                return result;
            }
            break;

        case M5IOE1_CONFIG_ADC:
            // 检查引脚是否支持 ADC
            // Check if pin supports ADC
            if (!_isAdcPin(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d does not support ADC", pin);
                return result;
            }
            // 检查引脚是否配置为输出
            // Check if pin is configured as output
            if (_pinStatesValid && _pinStates[pin].isOutput) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d is configured as output", pin);
                return result;
            }
            break;

        case M5IOE1_CONFIG_PWM:
            // 检查引脚是否支持 PWM
            // Check if pin supports PWM
            if (!_isPwmPin(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "Pin %d does not support PWM", pin);
                return result;
            }
            break;

        case M5IOE1_CONFIG_NEOPIXEL:
            // NeoPixel 仅在 IO14 上工作（引脚索引 13）
            // NeoPixel only works on IO14 (pin index 13)
            if (!_isNeopixelPin(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg), "NeoPixel only supported on IO14 (pin 13)");
                return result;
            }
            // 检查引脚是否有活动中断
            // Check if pin has active interrupt
            if (_hasActiveInterrupt(pin)) {
                snprintf(result.error_msg, sizeof(result.error_msg),
                         "IO14 has active interrupt, conflicts with NeoPixel");
                return result;
            }
            // 检查中断互斥（IO10 和 IO14 是互斥的）
            // Check interrupt mutex (IO10 and IO14 are mutex)
            if (_getInterruptMutexPin(pin, &mutexPin)) {
                if (_hasActiveInterrupt(mutexPin)) {
                    snprintf(result.error_msg, sizeof(result.error_msg),
                             "IO10 has interrupt enabled, mutex with IO14 NeoPixel");
                    result.conflicting_pin = mutexPin;
                    return result;
                }
            }
            break;

        case M5IOE1_CONFIG_I2C_SLEEP:
            // I2C 睡眠模式禁用 IO5 中断
            // I2C sleep mode disables IO5 interrupt
            if (_hasActiveInterrupt(4)) {  // IO5 is pin index 4
                snprintf(result.error_msg, sizeof(result.error_msg), "I2C sleep mode will disable IO5 interrupt");
                result.conflicting_pin = 4;
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
// 内部辅助函数
// Internal Helper Functions
// ============================

bool M5IOE1::_writeReg(uint8_t reg, uint8_t value)
{
    _checkAutoWake();
    for (int attempt = 0; attempt < M5IOE1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            if (M5IOE1_M5UNIFIED_WRITE_BYTE(_m5_i2c, _addr, reg, value, _commFreq)) {
                M5IOE1_LOG_D(TAG_I2C, "Write Reg[0x%02X] <- 0x%02X", reg, value);
                return true;
            }
        } else
#endif
            if (M5IOE1_I2C_ARDUINO_WRITE_BYTE(_wire, _addr, reg, value)) {
            M5IOE1_LOG_D(TAG_I2C, "Write Reg[0x%02X] <- 0x%02X", reg, value);
            return true;
        }
#else
        bool ok = false;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                ok = (M5IOE1_I2C_MASTER_WRITE_BYTE(_i2c_master_dev, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                ok = (M5IOE1_I2C_BUS_WRITE_BYTE(_i2c_device, reg, value) == ESP_OK);
                break;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY:
                ok = (M5IOE1_I2C_LEGACY_WRITE_BYTE(_port, _addr, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                ok = M5IOE1_M5UNIFIED_WRITE_BYTE(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                ok = false;
                break;
        }
        if (ok) {
            M5IOE1_LOG_D(TAG_I2C, "Write Reg[0x%02X] <- 0x%02X", reg, value);
            return true;
        }
#endif
        if (attempt + 1 < M5IOE1_I2C_RETRY_COUNT) {
            M5IOE1_DELAY_MS(M5IOE1_I2C_RETRY_DELAY_MS);
        }
    }
    M5IOE1_LOG_E(TAG_I2C, "Write Reg[0x%02X] failed", reg);
    return false;
}

bool M5IOE1::_writeReg16(uint8_t reg, uint16_t value)
{
    _checkAutoWake();
    for (int attempt = 0; attempt < M5IOE1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            if (M5IOE1_M5UNIFIED_WRITE_REG16(_m5_i2c, _addr, reg, value, _commFreq)) {
                M5IOE1_LOG_D(TAG_I2C, "Write16 Reg[0x%02X] <- 0x%04X", reg, value);
                return true;
            }
        } else
#endif
            if (M5IOE1_I2C_ARDUINO_WRITE_REG16(_wire, _addr, reg, value)) {
            M5IOE1_LOG_D(TAG_I2C, "Write16 Reg[0x%02X] <- 0x%04X", reg, value);
            return true;
        }
#else
        bool ok = false;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                ok = (M5IOE1_I2C_MASTER_WRITE_REG16(_i2c_master_dev, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                ok = (M5IOE1_I2C_BUS_WRITE_REG16(_i2c_device, reg, value) == ESP_OK);
                break;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY:
                ok = (M5IOE1_I2C_LEGACY_WRITE_REG16(_port, _addr, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                ok = M5IOE1_M5UNIFIED_WRITE_REG16(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                ok = false;
                break;
        }
        if (ok) {
            M5IOE1_LOG_D(TAG_I2C, "Write16 Reg[0x%02X] <- 0x%04X", reg, value);
            return true;
        }
#endif
        if (attempt + 1 < M5IOE1_I2C_RETRY_COUNT) {
            M5IOE1_DELAY_MS(M5IOE1_I2C_RETRY_DELAY_MS);
        }
    }
    M5IOE1_LOG_E(TAG_I2C, "Write16 Reg[0x%02X] failed", reg);
    return false;
}

bool M5IOE1::_readReg(uint8_t reg, uint8_t* value)
{
    _checkAutoWake();
    for (int attempt = 0; attempt < M5IOE1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            if (M5IOE1_M5UNIFIED_READ_BYTE(_m5_i2c, _addr, reg, value, _commFreq)) {
                M5IOE1_LOG_D(TAG_I2C, "Read  Reg[0x%02X] -> 0x%02X", reg, *value);
                return true;
            }
        } else
#endif
            if (M5IOE1_I2C_ARDUINO_READ_BYTE(_wire, _addr, reg, value)) {
            M5IOE1_LOG_D(TAG_I2C, "Read  Reg[0x%02X] -> 0x%02X", reg, *value);
            return true;
        }
#else
        bool ok = false;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                ok = (M5IOE1_I2C_MASTER_READ_BYTE(_i2c_master_dev, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                ok = (M5IOE1_I2C_BUS_READ_BYTE(_i2c_device, reg, value) == ESP_OK);
                break;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY:
                ok = (M5IOE1_I2C_LEGACY_READ_BYTE(_port, _addr, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                ok = M5IOE1_M5UNIFIED_READ_BYTE(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                ok = false;
                break;
        }
        if (ok) {
            M5IOE1_LOG_D(TAG_I2C, "Read  Reg[0x%02X] -> 0x%02X", reg, *value);
            return true;
        }
#endif
        if (attempt + 1 < M5IOE1_I2C_RETRY_COUNT) {
            M5IOE1_DELAY_MS(M5IOE1_I2C_RETRY_DELAY_MS);
        }
    }
    M5IOE1_LOG_E(TAG_I2C, "Read  Reg[0x%02X] failed", reg);
    return false;
}

bool M5IOE1::_readReg16(uint8_t reg, uint16_t* value)
{
    _checkAutoWake();
    for (int attempt = 0; attempt < M5IOE1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            if (M5IOE1_M5UNIFIED_READ_REG16(_m5_i2c, _addr, reg, value, _commFreq)) {
                M5IOE1_LOG_D(TAG_I2C, "Read16 Reg[0x%02X] -> 0x%04X", reg, *value);
                return true;
            }
        } else
#endif
            if (M5IOE1_I2C_ARDUINO_READ_REG16(_wire, _addr, reg, value)) {
            M5IOE1_LOG_D(TAG_I2C, "Read16 Reg[0x%02X] -> 0x%04X", reg, *value);
            return true;
        }
#else
        bool ok = false;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                ok = (M5IOE1_I2C_MASTER_READ_REG16(_i2c_master_dev, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                ok = (M5IOE1_I2C_BUS_READ_REG16(_i2c_device, reg, value) == ESP_OK);
                break;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY:
                ok = (M5IOE1_I2C_LEGACY_READ_REG16(_port, _addr, reg, value) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                ok = M5IOE1_M5UNIFIED_READ_REG16(_m5_i2c, _addr, reg, value, _commFreq);
                break;
#endif
            default:
                ok = false;
                break;
        }
        if (ok) {
            M5IOE1_LOG_D(TAG_I2C, "Read16 Reg[0x%02X] -> 0x%04X", reg, *value);
            return true;
        }
#endif
        if (attempt + 1 < M5IOE1_I2C_RETRY_COUNT) {
            M5IOE1_DELAY_MS(M5IOE1_I2C_RETRY_DELAY_MS);
        }
    }
    M5IOE1_LOG_E(TAG_I2C, "Read16 Reg[0x%02X] failed", reg);
    return false;
}

bool M5IOE1::_writeBytes(uint8_t reg, const uint8_t* data, uint8_t len)
{
    _checkAutoWake();
    for (int attempt = 0; attempt < M5IOE1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            if (M5IOE1_M5UNIFIED_WRITE_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq)) {
                M5IOE1_LOG_V(TAG_I2C, "WriteBytes Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len,
                             len > 0 ? data[0] : 0, len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
                return true;
            }
        } else
#endif
            if (M5IOE1_I2C_ARDUINO_WRITE_BYTES(_wire, _addr, reg, len, data)) {
            M5IOE1_LOG_V(TAG_I2C, "WriteBytes Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len, len > 0 ? data[0] : 0,
                         len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
            return true;
        }
#else
        bool ok = false;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                ok = (M5IOE1_I2C_MASTER_WRITE_BYTES(_i2c_master_dev, reg, len, data) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                ok = (M5IOE1_I2C_BUS_WRITE_BYTES(_i2c_device, reg, len, data) == ESP_OK);
                break;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY:
                ok = (M5IOE1_I2C_LEGACY_WRITE_BYTES(_port, _addr, reg, len, data) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                ok = M5IOE1_M5UNIFIED_WRITE_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq);
                break;
#endif
            default:
                ok = false;
                break;
        }
        if (ok) {
            M5IOE1_LOG_V(TAG_I2C, "WriteBytes Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len, len > 0 ? data[0] : 0,
                         len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
            return true;
        }
#endif
        if (attempt + 1 < M5IOE1_I2C_RETRY_COUNT) {
            M5IOE1_DELAY_MS(M5IOE1_I2C_RETRY_DELAY_MS);
        }
    }
    M5IOE1_LOG_E(TAG_I2C, "WriteBytes Reg[0x%02X] len=%d failed", reg, len);
    return false;
}

bool M5IOE1::_readBytes(uint8_t reg, uint8_t* data, uint8_t len)
{
    _checkAutoWake();
    for (int attempt = 0; attempt < M5IOE1_I2C_RETRY_COUNT; ++attempt) {
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            if (M5IOE1_M5UNIFIED_READ_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq)) {
                M5IOE1_LOG_V(TAG_I2C, "ReadBytes  Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len,
                             len > 0 ? data[0] : 0, len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
                return true;
            }
        } else
#endif
            if (M5IOE1_I2C_ARDUINO_READ_BYTES(_wire, _addr, reg, len, data)) {
            M5IOE1_LOG_V(TAG_I2C, "ReadBytes  Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len, len > 0 ? data[0] : 0,
                         len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
            return true;
        }
#else
        bool ok = false;
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
                ok = (M5IOE1_I2C_MASTER_READ_BYTES(_i2c_master_dev, reg, len, data) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                ok = (M5IOE1_I2C_BUS_READ_BYTES(_i2c_device, reg, len, data) == ESP_OK);
                break;
#endif
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY:
                ok = (M5IOE1_I2C_LEGACY_READ_BYTES(_port, _addr, reg, len, data) == ESP_OK);
                break;
#endif
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                ok = M5IOE1_M5UNIFIED_READ_BYTES(_m5_i2c, _addr, reg, len, data, _commFreq);
                break;
#endif
            default:
                ok = false;
                break;
        }
        if (ok) {
            M5IOE1_LOG_V(TAG_I2C, "ReadBytes  Reg[0x%02X] len=%d: %02X %02X %02X...", reg, len, len > 0 ? data[0] : 0,
                         len > 1 ? data[1] : 0, len > 2 ? data[2] : 0);
            return true;
        }
#endif
        if (attempt + 1 < M5IOE1_I2C_RETRY_COUNT) {
            M5IOE1_DELAY_MS(M5IOE1_I2C_RETRY_DELAY_MS);
        }
    }
    M5IOE1_LOG_E(TAG_I2C, "ReadBytes  Reg[0x%02X] len=%d failed", reg, len);
    return false;
}

bool M5IOE1::_isValidPin(uint8_t pin)
{
    return pin < M5IOE1_MAX_GPIO_PINS;
}

bool M5IOE1::_isAdcPin(uint8_t pin)
{
    // ADC 引脚：IO2(1), IO4(3), IO5(4), IO7(6)
    // ADC pins: IO2(1), IO4(3), IO5(4), IO7(6)
    return (pin == 1 || pin == 3 || pin == 4 || pin == 6);
}

bool M5IOE1::_isPwmPin(uint8_t pin)
{
    // PWM 引脚：IO8(7), IO9(8), IO10(9), IO11(10)
    // PWM pins: IO8(7), IO9(8), IO10(9), IO11(10)
    return (pin == 7 || pin == 8 || pin == 9 || pin == 10);
}

uint8_t M5IOE1::_getAdcChannel(uint8_t pin)
{
    switch (pin) {
        case 1:
            return 1;  // IO2
        case 3:
            return 2;  // IO4
        case 4:
            return 3;  // IO5
        case 6:
            return 4;  // IO7
        default:
            return 0;
    }
}

uint8_t M5IOE1::_getPwmChannel(uint8_t pin)
{
    switch (pin) {
        case 8:
            return 0;  // IO9 -> PWM1
        case 7:
            return 1;  // IO8 -> PWM2
        case 10:
            return 2;  // IO11 -> PWM3
        case 9:
            return 3;  // IO10 -> PWM4
        default:
            return 0;
    }
}

void M5IOE1::_clearPinStates()
{
    memset(_pinStates, 0, sizeof(_pinStates));
    _pinStatesValid = false;
}

void M5IOE1::_clearPwmStates()
{
    memset(_pwmStates, 0, sizeof(_pwmStates));
    _pwmFrequency   = 0;
    _pwmStatesValid = false;
}

void M5IOE1::_clearAdcState()
{
    memset(&_adcState, 0, sizeof(_adcState));
    _adcStateValid = false;
}

bool M5IOE1::_snapshotPinStates()
{
    if (!_initialized) return false;

    uint16_t modeReg = 0, outReg = 0, inReg = 0, puReg = 0, pdReg = 0, drvReg = 0, ieReg = 0, itReg = 0;

    if (!_readReg16(M5IOE1_REG_GPIO_MODE_L, &modeReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_OUT_L, &outReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_IN_L, &inReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_PU_L, &puReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_PD_L, &pdReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_DRV_L, &drvReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_IE_L, &ieReg)) return false;
    if (!_readReg16(M5IOE1_REG_GPIO_IP_L, &itReg)) return false;

    for (uint8_t pin = 0; pin < M5IOE1_MAX_GPIO_PINS; pin++) {
        _pinStates[pin].isOutput    = (modeReg & (1 << pin)) != 0;
        _pinStates[pin].outputLevel = (outReg & (1 << pin)) ? 1 : 0;
        _pinStates[pin].inputLevel  = (inReg & (1 << pin)) ? 1 : 0;
        _pinStates[pin].pull        = (puReg & (1 << pin)) ? 1 : ((pdReg & (1 << pin)) ? 2 : 0);
        _pinStates[pin].drive       = (drvReg & (1 << pin)) ? 1 : 0;
        _pinStates[pin].intrEnabled = (ieReg & (1 << pin)) != 0;
        _pinStates[pin].intrRising  = (itReg & (1 << pin)) != 0;
    }

    _pinStatesValid = true;
    return true;
}

bool M5IOE1::_snapshotPwmStates()
{
    if (!_initialized) return false;

    if (!_readReg16(M5IOE1_REG_PWM_FREQ_L, &_pwmFrequency)) return false;

    for (uint8_t ch = 0; ch < M5IOE1_MAX_PWM_CHANNELS; ch++) {
        uint8_t regL  = M5IOE1_REG_PWM1_DUTY_L + (ch * 2);
        uint16_t data = 0;
        if (!_readReg16(regL, &data)) return false;

        _pwmStates[ch].duty12   = data & 0x0FFF;
        _pwmStates[ch].enabled  = (data & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) != 0;
        _pwmStates[ch].polarity = (data & ((uint16_t)M5IOE1_PWM_POLARITY << 8)) != 0;
    }

    _pwmStatesValid = true;
    return true;
}

bool M5IOE1::_snapshotAdcState()
{
    if (!_initialized) return false;

    uint8_t ctrl = 0;
    if (!_readReg(M5IOE1_REG_ADC_CTRL, &ctrl)) return false;

    _adcState.activeChannel = ctrl & M5IOE1_ADC_CH_MASK;
    _adcState.busy          = (ctrl & M5IOE1_ADC_BUSY) != 0;

    if (!_adcState.busy) {
        if (!_readReg16(M5IOE1_REG_ADC_DATA_L, &_adcState.lastValue)) {
            _adcState.lastValue = 0;
        }
    }

    _adcStateValid = true;
    return true;
}

void M5IOE1::_autoSnapshotUpdate(uint8_t domains)
{
    if (!_autoSnapshot || !_initialized) return;

    if (domains & M5IOE1_SNAPSHOT_DOMAIN_GPIO) {
        _snapshotPinStates();
    }
    if (domains & M5IOE1_SNAPSHOT_DOMAIN_PWM) {
        _snapshotPwmStates();
    }
    if (domains & M5IOE1_SNAPSHOT_DOMAIN_ADC) {
        _snapshotAdcState();
    }
    if (domains & M5IOE1_SNAPSHOT_DOMAIN_AW8737A) {
        _snapshotAw8737a();
    }
}

bool M5IOE1::_isValidI2cFrequency(uint32_t speed)
{
    return (speed == M5IOE1_I2C_FREQ_100K || speed == M5IOE1_I2C_FREQ_400K);
}

m5ioe1_err_t M5IOE1::getI2cSpeed(m5ioe1_i2c_speed_t* speed)
{
    if (speed == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_initialized) return M5IOE1_ERR_NOT_INIT;

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) return M5IOE1_ERR_I2C_COMM;

    // 更新缓存
    // Update cache
    _i2cConfig.speed400k = (cfg & M5IOE1_I2C_SPEED_400K_BIT) != 0;
    _i2cConfigValid      = true;

    *speed = _i2cConfig.speed400k ? M5IOE1_I2C_SPEED_400K : M5IOE1_I2C_SPEED_100K;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::switchI2cSpeed(m5ioe1_i2c_speed_t speed)
{
    if (!_initialized) {
        M5IOE1_LOG_E(TAG_I2C, "Cannot switch I2C speed: device not initialized");
        return M5IOE1_ERR_NOT_INIT;
    }

    uint32_t targetFreq = (speed == M5IOE1_I2C_SPEED_400K) ? M5IOE1_I2C_FREQ_400K : M5IOE1_I2C_FREQ_100K;

    // 如果目标速度与当前速度相同，直接返回成功
    // If target speed is same as current, return success directly
    if (targetFreq == _requestedSpeed) {
        M5IOE1_LOG_I(TAG_I2C, "I2C speed already at %lu Hz, no change needed", targetFreq);
        return M5IOE1_OK;
    }

    // 步骤 1: 读取当前 I2C 配置
    // Step 1: Read current I2C config
    uint8_t i2cCfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &i2cCfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to read I2C config register");
        return M5IOE1_ERR_I2C_COMM;
    }

    // 步骤 2: 根据目标速度设置或清除 400KHz 模式位
    // Step 2: Set or clear 400KHz mode bit based on target speed
    if (speed == M5IOE1_I2C_SPEED_400K) {
        i2cCfg |= M5IOE1_I2C_SPEED_400K_BIT;
    } else {
        i2cCfg &= ~M5IOE1_I2C_SPEED_400K_BIT;
    }

    if (!_writeReg(M5IOE1_REG_I2C_CFG, i2cCfg)) {
        M5IOE1_LOG_E(TAG_I2C, "Failed to write I2C config register");
        return M5IOE1_ERR_I2C_COMM;
    }

    M5IOE1_LOG_D(TAG_I2C, "M5IOE1 I2C config set to %lu Hz mode", targetFreq);

    // 步骤 3: 短暂延迟以允许设备处理配置更改
    // Step 3: Small delay to allow device to process the configuration change
    M5IOE1_DELAY_MS(5);

    // 步骤 4: 将主机 I2C 总线切换到目标速度
    // Step 4: Switch host I2C bus to target speed
#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
    if (_m5_i2c) {
        _commFreq = targetFreq;
        M5IOE1_LOG_D(TAG_I2C, "M5Unified I2C frequency updated to %lu Hz", targetFreq);
    } else
#endif
        if (_wire != nullptr) {
        // 必须使用 Wire.end() + Wire.begin() 在 ESP32 上正确切换 I2C 频率
        // Must use Wire.end() + Wire.begin() to properly switch I2C frequency on ESP32
        _wire->end();
        M5IOE1_DELAY_MS(10);
        if (!_wire->begin(_sda, _scl, targetFreq)) {
            M5IOE1_LOG_E(TAG_I2C, "Failed to re-initialize I2C bus at %lu Hz", targetFreq);
            // 尝试恢复到原来的速度
            // Try to recover with original speed
            uint32_t originalFreq = (speed == M5IOE1_I2C_SPEED_400K) ? M5IOE1_I2C_FREQ_100K : M5IOE1_I2C_FREQ_400K;
            _wire->begin(_sda, _scl, originalFreq);
            // 恢复设备配置
            // Revert device config
            if (speed == M5IOE1_I2C_SPEED_400K) {
                i2cCfg &= ~M5IOE1_I2C_SPEED_400K_BIT;
            } else {
                i2cCfg |= M5IOE1_I2C_SPEED_400K_BIT;
            }
            _writeReg(M5IOE1_REG_I2C_CFG, i2cCfg);
            return M5IOE1_ERR_I2C_CONFIG;
        }
        M5IOE1_DELAY_MS(10);
        M5IOE1_LOG_D(TAG_I2C, "Host I2C bus switched to %lu Hz", targetFreq);
    }
#else
    // ESP-IDF：处理不同的驱动类型
    // ESP-IDF: Handle different driver types
    esp_err_t ret;

    switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
        case M5IOE1_I2C_DRIVER_SELF_CREATED:
        case M5IOE1_I2C_DRIVER_MASTER:
            // 对于 i2c_master 驱动：删除设备并以新速度重新添加
            // For i2c_master driver: remove device and add with new speed
            if (_i2c_master_dev != nullptr) {
                ret = i2c_master_bus_rm_device(_i2c_master_dev);
                if (ret != ESP_OK) {
                    M5IOE1_LOG_E(TAG_I2C, "Failed to remove I2C device: %s", esp_err_to_name(ret));
                    return M5IOE1_ERR_I2C_CONFIG;
                }
                _i2c_master_dev = nullptr;

                // 以目标速度重新创建设备句柄
                // Recreate device handle with target speed
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
                    M5IOE1_LOG_E(TAG_I2C, "Failed to add I2C device at %lu Hz: %s", targetFreq, esp_err_to_name(ret));
                    // 尝试恢复到原来的速度
                    // Try to recover with original speed
                    uint32_t originalFreq =
                        (speed == M5IOE1_I2C_SPEED_400K) ? M5IOE1_I2C_FREQ_100K : M5IOE1_I2C_FREQ_400K;
                    dev_config.scl_speed_hz = originalFreq;
                    i2c_master_bus_add_device(_i2c_master_bus, &dev_config, &_i2c_master_dev);
                    return M5IOE1_ERR_I2C_CONFIG;
                }
                M5IOE1_LOG_D(TAG_I2C, "I2C master device recreated at %lu Hz", targetFreq);
            }
            break;
#endif  // M5IOE1_HAS_I2C_MASTER

#if M5IOE1_HAS_I2C_BUS
        case M5IOE1_I2C_DRIVER_BUS:
            // 对于 i2c_bus 驱动：删除设备并以新速度创建
            // For i2c_bus driver: delete device and create with new speed
            if (_i2c_device != nullptr) {
                ret = i2c_bus_device_delete(&_i2c_device);
                if (ret != ESP_OK) {
                    M5IOE1_LOG_E(TAG_I2C, "Failed to delete I2C device: %s", esp_err_to_name(ret));
                    return M5IOE1_ERR_I2C_CONFIG;
                }

                // 以目标速度重新创建设备句柄
                // Recreate device handle with target speed
                _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, targetFreq);
                if (_i2c_device == nullptr) {
                    M5IOE1_LOG_E(TAG_I2C, "Failed to create I2C device at %lu Hz", targetFreq);
                    // 尝试恢复到原来的速度
                    // Try to recover with original speed
                    uint32_t originalFreq =
                        (speed == M5IOE1_I2C_SPEED_400K) ? M5IOE1_I2C_FREQ_100K : M5IOE1_I2C_FREQ_400K;
                    _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, originalFreq);
                    return M5IOE1_ERR_I2C_CONFIG;
                }
                M5IOE1_LOG_D(TAG_I2C, "I2C bus device recreated at %lu Hz", targetFreq);
            }
            break;
#endif  // M5IOE1_HAS_I2C_BUS

#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
        case M5IOE1_I2C_DRIVER_LEGACY: {
            i2c_config_t i2c_conf     = {};
            i2c_conf.mode             = I2C_MODE_MASTER;
            i2c_conf.sda_io_num       = _sda;
            i2c_conf.scl_io_num       = _scl;
            i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
            i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
            i2c_conf.master.clk_speed = targetFreq;
            ret                       = i2c_param_config(_port, &i2c_conf);
            if (ret != ESP_OK) {
                M5IOE1_LOG_E(TAG_I2C, "i2c_param_config failed: %s", esp_err_to_name(ret));
                return M5IOE1_ERR_I2C_CONFIG;
            }
            M5IOE1_LOG_D(TAG_I2C, "Legacy I2C reconfigured to %lu Hz", targetFreq);
            break;
        }
#endif  // !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS

#if M5IOE1_HAS_M5UNIFIED_I2C
        case M5IOE1_I2C_DRIVER_M5UNIFIED:
            _commFreq = targetFreq;
            M5IOE1_LOG_D(TAG_I2C, "M5Unified I2C frequency updated to %lu Hz", targetFreq);
            break;
#endif
        default:
            M5IOE1_LOG_E(TAG_I2C, "Unknown I2C driver type");
            return M5IOE1_ERR_INTERNAL;
    }
#endif

    // 步骤 5: 验证通信仍然有效
    // Step 5: Verify communication still works
    uint16_t uid = 0;
    if (!_readReg16(M5IOE1_REG_UID_L, &uid)) {
        M5IOE1_LOG_E(TAG_I2C, "Communication failed after switching to %lu Hz, reverting", targetFreq);

        // 恢复设备配置
        // Revert device config
        if (speed == M5IOE1_I2C_SPEED_400K) {
            i2cCfg &= ~M5IOE1_I2C_SPEED_400K_BIT;
        } else {
            i2cCfg |= M5IOE1_I2C_SPEED_400K_BIT;
        }
        uint32_t originalFreq = (speed == M5IOE1_I2C_SPEED_400K) ? M5IOE1_I2C_FREQ_100K : M5IOE1_I2C_FREQ_400K;

#ifdef ARDUINO
#if M5IOE1_HAS_M5UNIFIED_I2C
        if (_m5_i2c) {
            _commFreq = originalFreq;
        } else
#endif
            if (_wire != nullptr) {
            _wire->end();
            M5IOE1_DELAY_MS(10);
            _wire->begin(_sda, _scl, originalFreq);
            M5IOE1_DELAY_MS(10);
        }
#else
        switch (_i2cDriverType) {
#if M5IOE1_HAS_I2C_MASTER
            case M5IOE1_I2C_DRIVER_SELF_CREATED:
            case M5IOE1_I2C_DRIVER_MASTER:
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
#endif  // M5IOE1_HAS_I2C_MASTER
#if M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_BUS:
                if (_i2c_device != nullptr) {
                    i2c_bus_device_delete(&_i2c_device);
                    _i2c_device = i2c_bus_device_create(_i2c_bus, _addr, originalFreq);
                }
                break;
#endif  // M5IOE1_HAS_I2C_BUS
#if !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
            case M5IOE1_I2C_DRIVER_LEGACY: {
                i2c_config_t i2c_conf     = {};
                i2c_conf.mode             = I2C_MODE_MASTER;
                i2c_conf.sda_io_num       = _sda;
                i2c_conf.scl_io_num       = _scl;
                i2c_conf.sda_pullup_en    = GPIO_PULLUP_ENABLE;
                i2c_conf.scl_pullup_en    = GPIO_PULLUP_ENABLE;
                i2c_conf.master.clk_speed = originalFreq;
                i2c_param_config(_port, &i2c_conf);  // best effort rollback
                break;
            }
#endif  // !M5IOE1_HAS_I2C_MASTER && !M5IOE1_HAS_I2C_BUS
#if M5IOE1_HAS_M5UNIFIED_I2C
            case M5IOE1_I2C_DRIVER_M5UNIFIED:
                _commFreq = originalFreq;
                break;
#endif
            default:
                break;
        }
#endif
        _writeReg(M5IOE1_REG_I2C_CFG, i2cCfg);
        return M5IOE1_ERR_I2C_COMM;
    }

    // 更新 I2C 配置缓存和请求速度
    // Update I2C config cache and requested speed
    _i2cConfig.speed400k = (speed == M5IOE1_I2C_SPEED_400K);
    _requestedSpeed      = targetFreq;

    M5IOE1_LOG_I(TAG_I2C, "Successfully switched to %lu Hz I2C mode", targetFreq);
    return M5IOE1_OK;
}

bool M5IOE1::_initDevice()
{
    // 读取设备信息以验证通信
    // Read device info to verify communication
    uint16_t uid    = 0;
    uint8_t version = 0;

    if (!_readReg16(M5IOE1_REG_UID_L, &uid)) {
        M5IOE1_LOG_E(TAG_SYS, "Failed to read device UID");
        return false;
    }

    if (!_readReg(M5IOE1_REG_REV, &version)) {
        M5IOE1_LOG_E(TAG_SYS, "Failed to read device version");
        return false;
    }

    M5IOE1_LOG_I(TAG_SYS, "Device UID: 0x%04X, FW Version: %02X", uid, version);
    return true;
}

void M5IOE1::_handleInterrupt()
{
    // 步骤 1: 读取一次中断状态
    // Step 1: Read interrupt status once
    uint16_t status = 0;
    if (getInterruptStatus(&status) != M5IOE1_OK) return;

    if (status == 0) return;

    // 步骤 2: 立即写入0清除所有中断（避免在处理回调期间丢失新中断）
    // Step 2: Immediately write 0 to clear all interrupts (avoid losing new interrupts during callback processing)
    _writeReg16(M5IOE1_REG_GPIO_IS_L, 0);

    // 步骤 3: 处理所有触发的引脚回调
    // Step 3: Process all triggered pin callbacks
    for (uint8_t pin = 0; pin < M5IOE1_MAX_GPIO_PINS; pin++) {
        if ((status & (1 << pin)) && _callbacks[pin].enabled) {
            if (_enableDefaultIsrLog) {
                M5IOE1_LOG_I(TAG_IRQ, "Pin %d triggered by %s edge", pin,
                             _callbacks[pin].rising ? "RISING" : "FALLING");
            }

            if (_callbacks[pin].callbackArg != nullptr) {
                _callbacks[pin].callbackArg(_callbacks[pin].arg);
            } else if (_callbacks[pin].callback != nullptr) {
                _callbacks[pin].callback();
            }
        }
    }
}

bool M5IOE1::_pinsConflict(uint8_t a, uint8_t b)
{
    // 中断冲突对（1-based IO 编号）：
    // Interrupt conflict pairs (1-based IO numbers):
    // (1,6), (2,3), (7,12), (8,9), (10,14), (11,13)
    uint8_t A = a + 1, B = b + 1;

    auto eq = [](uint8_t x, uint8_t y, uint8_t m, uint8_t n) { return (x == m && y == n) || (x == n && y == m); };

    return eq(A, B, 1, 6) || eq(A, B, 2, 3) || eq(A, B, 7, 12) || eq(A, B, 8, 9) || eq(A, B, 10, 14) ||
           eq(A, B, 11, 13);
}

bool M5IOE1::_hasConflictingInterrupt(uint8_t pin)
{
    for (uint8_t i = 0; i < M5IOE1_MAX_GPIO_PINS; i++) {
        if (i != pin && _pinStates[i].intrEnabled && _pinsConflict(pin, i)) {
            return true;
        }
    }
    return false;
}

// ============================
// 配置验证辅助函数
// Configuration Validation Helpers
// ============================

bool M5IOE1::_getInterruptMutexPin(uint8_t pin, uint8_t* mutexPin)
{
    if (mutexPin == nullptr) return false;

    // 中断互斥对（0-based 引脚索引）：
    // Interrupt mutex pairs (0-based pin indices):
    // IO1(0) <-> IO6(5)
    // IO2(1) <-> IO3(2)
    // IO7(6) <-> IO12(11)
    // IO8(7) <-> IO9(8)
    // IO10(9) <-> IO14(13)
    // IO11(10) <-> IO13(12)

    static const uint8_t mutexPairs[][2] = {
        {0, 5},   // IO1 <-> IO6
        {1, 2},   // IO2 <-> IO3
        {6, 11},  // IO7 <-> IO12
        {7, 8},   // IO8 <-> IO9
        {9, 13},  // IO10 <-> IO14
        {10, 12}  // IO11 <-> IO13
    };

    for (size_t i = 0; i < sizeof(mutexPairs) / sizeof(mutexPairs[0]); i++) {
        if (pin == mutexPairs[i][0]) {
            *mutexPin = mutexPairs[i][1];
            return true;
        }
        if (pin == mutexPairs[i][1]) {
            *mutexPin = mutexPairs[i][0];
            return true;
        }
    }

    return false;
}

bool M5IOE1::_isNeopixelPin(uint8_t pin)
{
    // NeoPixel LED 功能仅在 IO14 上可用（引脚索引 13）
    // NeoPixel LED function only available on IO14 (pin index 13)
    return (pin == 13);
}

bool M5IOE1::_hasActiveInterrupt(uint8_t pin)
{
    if (!_isValidPin(pin)) return false;
    if (!_pinStatesValid) return false;
    return _pinStates[pin].intrEnabled;
}

bool M5IOE1::_hasActiveAdc(uint8_t pin)
{
    if (!_isAdcPin(pin)) return false;
    if (!_adcStateValid) return false;

    // 检查 ADC 当前是否正在使用此引脚的通道
    // Check if the ADC is currently using this pin's channel
    uint8_t channel = _getAdcChannel(pin);
    return (_adcState.activeChannel == channel);
}

bool M5IOE1::_hasActivePwm(uint8_t pin)
{
    if (!_isPwmPin(pin)) return false;
    if (!_pwmStatesValid) return false;

    uint8_t channel = _getPwmChannel(pin);
    return _pwmStates[channel].enabled;
}

bool M5IOE1::_hasI2cSleepEnabled()
{
    if (!_i2cConfigValid) return false;
    return (_i2cConfig.sleepTime > 0);
}

bool M5IOE1::_isLedEnabled()
{
    // 直接读取 LED_CFG 寄存器
    // Read LED_CFG register directly
    uint8_t ledCfg = 0;
    if (!_readReg(M5IOE1_REG_LED_CFG, &ledCfg)) {
        return false;
    }
    uint8_t ledCount = ledCfg & M5IOE1_LED_NUM_MASK;
    return (ledCount > 0);
}

// ============================
// I2C 配置快照
// I2C Config Snapshot
// ============================

void M5IOE1::_clearI2cConfig()
{
    memset(&_i2cConfig, 0, sizeof(_i2cConfig));
    _i2cConfigValid = false;
}

bool M5IOE1::_snapshotI2cConfig()
{
    if (!_initialized) return false;

    uint8_t cfg = 0;
    if (!_readReg(M5IOE1_REG_I2C_CFG, &cfg)) return false;

    _i2cConfig.sleepTime  = cfg & M5IOE1_I2C_SLEEP_MASK;
    _i2cConfig.speed400k  = (cfg & M5IOE1_I2C_SPEED_400K_BIT) != 0;
    _i2cConfig.wakeRising = (cfg & M5IOE1_I2C_WAKE_RISING) != 0;
    _i2cConfig.pullOff    = (cfg & M5IOE1_I2C_PULL_OFF) != 0;
    _i2cConfigValid       = true;

    return true;
}

bool M5IOE1::_snapshotAw8737a()
{
    if (!_initialized) return false;

    uint8_t regValue = 0;
    if (!_readReg(M5IOE1_REG_AW8737A_PULSE, &regValue)) {
        M5IOE1_LOG_E(TAG_AMP, "Failed to read AW8737A pulse register");
        _aw8737aStateValid = false;
        return false;
    }

    // 解析寄存器值（不含 REFRESH 位）
    // Parse register value (without REFRESH bit)
    _aw8737aRegValue   = regValue & 0x7F;
    _aw8737aPin        = regValue & M5IOE1_AW8737A_GPIO_MASK;
    _aw8737aPulseNum   = (m5ioe1_aw8737a_pulse_t)((regValue >> M5IOE1_AW8737A_NUM_SHIFT) & M5IOE1_AW8737A_NUM_MASK);
    _aw8737aStateValid = true;

    return true;
}

void M5IOE1::_clearAw8737a()
{
    _aw8737aConfigured = false;
    _aw8737aPin        = 0;
    _aw8737aPulseNum   = M5IOE1_AW8737A_PULSE_0;
    _aw8737aRegValue   = 0;
    _aw8737aStateValid = false;
}

// ============================
// 快照验证
// Snapshot Verification
// ============================

m5ioe1_snapshot_verify_t M5IOE1::verifySnapshot()
{
    m5ioe1_snapshot_verify_t result = {};
    result.consistent               = true;

    if (!_initialized) {
        result.consistent = false;
        return result;
    }

    // 验证 GPIO 寄存器
    // Verify GPIO registers
    if (_pinStatesValid) {
        uint16_t actualMode = 0, actualOutput = 0;

        if (_readReg16(M5IOE1_REG_GPIO_MODE_L, &actualMode) && _readReg16(M5IOE1_REG_GPIO_OUT_L, &actualOutput)) {
            // 从缓存构建期望值
            // Build expected values from cache
            uint16_t expectedMode = 0, expectedOutput = 0;
            for (uint8_t i = 0; i < M5IOE1_MAX_GPIO_PINS; i++) {
                if (_pinStates[i].isOutput) {
                    expectedMode |= (1 << i);
                }
                if (_pinStates[i].outputLevel) {
                    expectedOutput |= (1 << i);
                }
            }

            result.expected_mode   = expectedMode;
            result.actual_mode     = actualMode;
            result.expected_output = expectedOutput;
            result.actual_output   = actualOutput;

            if (expectedMode != actualMode || expectedOutput != actualOutput) {
                result.gpio_mismatch = true;
                result.consistent    = false;
            }
        } else {
            result.consistent = false;
        }
    }

    // 验证 PWM 寄存器
    // Verify PWM registers
    if (_pwmStatesValid) {
        uint16_t actualFreq = 0;
        if (_readReg16(M5IOE1_REG_PWM_FREQ_L, &actualFreq)) {
            if (actualFreq != _pwmFrequency) {
                result.pwm_mismatch = true;
                result.consistent   = false;
            }
        }

        for (uint8_t ch = 0; ch < M5IOE1_MAX_PWM_CHANNELS; ch++) {
            uint8_t regL  = M5IOE1_REG_PWM1_DUTY_L + (ch * 2);
            uint16_t data = 0;
            if (_readReg16(regL, &data)) {
                uint16_t actualDuty = data & 0x0FFF;
                bool actualEnabled  = (data & ((uint16_t)M5IOE1_PWM_ENABLE << 8)) != 0;
                bool actualPolarity = (data & ((uint16_t)M5IOE1_PWM_POLARITY << 8)) != 0;

                if (actualDuty != _pwmStates[ch].duty12 || actualEnabled != _pwmStates[ch].enabled ||
                    actualPolarity != _pwmStates[ch].polarity) {
                    result.pwm_mismatch = true;
                    result.consistent   = false;
                }
            }
        }
    }

    // 验证 ADC 寄存器
    // Verify ADC registers
    if (_adcStateValid) {
        uint8_t ctrl = 0;
        if (_readReg(M5IOE1_REG_ADC_CTRL, &ctrl)) {
            uint8_t actualChannel = ctrl & M5IOE1_ADC_CH_MASK;
            if (actualChannel != _adcState.activeChannel) {
                result.adc_mismatch = true;
                result.consistent   = false;
            }
        }
    }

    // 验证 AW8737A 寄存器
    // Verify AW8737A registers
    if (_aw8737aStateValid) {
        uint8_t actualReg = 0;
        if (_readReg(M5IOE1_REG_AW8737A_PULSE, &actualReg)) {
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
// 缓存状态查询函数
// Cached State Query Functions
// ============================

m5ioe1_err_t M5IOE1::getCachedPwmFrequency(uint16_t* frequency)
{
    if (frequency == nullptr) return M5IOE1_ERR_INVALID_ARG;
    if (!_pwmStatesValid) return M5IOE1_FAIL;
    *frequency = _pwmFrequency;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getCachedPwmState(uint8_t channel, uint16_t* duty12, bool* polarity, bool* enabled)
{
    if (channel > 3 || duty12 == nullptr || polarity == nullptr || enabled == nullptr) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_pwmStatesValid) return M5IOE1_FAIL;

    *duty12   = _pwmStates[channel].duty12;
    *polarity = _pwmStates[channel].polarity;
    *enabled  = _pwmStates[channel].enabled;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getCachedAdcState(uint8_t* activeChannel, bool* busy, uint16_t* lastValue)
{
    if (activeChannel == nullptr || busy == nullptr || lastValue == nullptr) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_adcStateValid) return M5IOE1_FAIL;

    *activeChannel = _adcState.activeChannel;
    *busy          = _adcState.busy;
    *lastValue     = _adcState.lastValue;
    return M5IOE1_OK;
}

m5ioe1_err_t M5IOE1::getCachedPinState(uint8_t pin, bool* isOutput, uint8_t* level, uint8_t* pull)
{
    if (!_isValidPin(pin) || isOutput == nullptr || level == nullptr || pull == nullptr) {
        return M5IOE1_ERR_INVALID_ARG;
    }
    if (!_pinStatesValid) return M5IOE1_FAIL;

    *isOutput = _pinStates[pin].isOutput;
    *level    = _pinStates[pin].isOutput ? _pinStates[pin].outputLevel : _pinStates[pin].inputLevel;
    *pull     = _pinStates[pin].pull;
    return M5IOE1_OK;
}

// ============================
// 平台特定函数
// Platform-Specific Functions
// ============================

#ifdef ARDUINO

void IRAM_ATTR M5IOE1::_arduinoIsrHandler(void* arg)
{
    M5IOE1* self = static_cast<M5IOE1*>(arg);
    if (self == nullptr || self->_pollTask == nullptr) {
        return;
    }
    BaseType_t hpw = pdFALSE;
    vTaskNotifyGiveFromISR(self->_pollTask, &hpw);
    if (hpw == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void M5IOE1::_pollTaskArduino(void* arg)
{
    M5IOE1* self = static_cast<M5IOE1*>(arg);

    while (true) {
        uint16_t status = 0;
        if (self->getInterruptStatus(&status) == M5IOE1_OK && status != 0) {
            self->_handleInterrupt();
        }
        vTaskDelay(pdMS_TO_TICKS(self->_pollingInterval));
    }
}

bool M5IOE1::_setupPollingArduino()
{
    BaseType_t ok = xTaskCreatePinnedToCore(_pollTaskArduino, "m5ioe1_poll", 4096, this, 5, &_pollTask, tskNO_AFFINITY);
    return ok == pdPASS;
}

void M5IOE1::_cleanupPollingArduino()
{
    if (_pollTask) {
        vTaskDelete(_pollTask);
        _pollTask = nullptr;
    }
}

void M5IOE1::_intrTaskArduino(void* arg)
{
    M5IOE1* self = static_cast<M5IOE1*>(arg);
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (self) {
            self->_handleInterrupt();
        }
    }
}

bool M5IOE1::_setupHardwareInterruptArduino()
{
    if (_intPin < 0) {
        M5IOE1_LOG_E(TAG_IRQ, "Hardware interrupt mode requires interrupt pin");
        return false;
    }

    pinMode(_intPin, INPUT_PULLUP);

    BaseType_t ok = xTaskCreatePinnedToCore(_intrTaskArduino, "m5ioe1_intr", 4096, this, 5, &_pollTask, tskNO_AFFINITY);
    if (ok != pdPASS) {
        _pollTask = nullptr;
        return false;
    }

    attachInterruptArg(digitalPinToInterrupt(_intPin), _arduinoIsrHandler, this, FALLING);
    return true;
}

void M5IOE1::_cleanupHardwareInterruptArduino()
{
    if (_intPin >= 0) {
        detachInterrupt(_intPin);
    }
    if (_pollTask) {
        vTaskDelete(_pollTask);
        _pollTask = nullptr;
    }
}

#else  // ESP-IDF

void M5IOE1::_pollTaskFunc(void* arg)
{
    M5IOE1* self = static_cast<M5IOE1*>(arg);

    while (true) {
        uint16_t status = 0;
        if (self->getInterruptStatus(&status) == M5IOE1_OK && status != 0) {
            self->_handleInterrupt();
        }
        vTaskDelay(pdMS_TO_TICKS(self->_pollingInterval));
    }
}

void IRAM_ATTR M5IOE1::_isrHandler(void* arg)
{
    M5IOE1* self = static_cast<M5IOE1*>(arg);
    if (self && self->_intrQueue) {
        // 立即禁用中断，防止电平触发造成 ISR 风暴
        // Disable interrupt immediately to prevent level-triggered ISR storm
        gpio_intr_disable((gpio_num_t)self->_intPin);
        uint32_t evt   = 1;
        BaseType_t hpw = pdFALSE;
        xQueueSendFromISR(self->_intrQueue, &evt, &hpw);
        if (hpw == pdTRUE) portYIELD_FROM_ISR();
    }
}

bool M5IOE1::_setupHardwareInterrupt()
{
    if (_intPin < 0) {
        M5IOE1_LOG_E(TAG_IRQ, "Hardware interrupt mode requires interrupt pin");
        return false;
    }

    _intrQueue = xQueueCreate(8, sizeof(uint32_t));
    if (_intrQueue == nullptr) return false;

    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << _intPin),
                             .mode         = GPIO_MODE_INPUT,
                             .pull_up_en   = GPIO_PULLUP_ENABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type    = GPIO_INTR_LOW_LEVEL};

    if (gpio_config(&io_conf) != ESP_OK) {
        vQueueDelete(_intrQueue);
        _intrQueue = nullptr;
        return false;
    }

    esp_err_t isr_err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        vQueueDelete(_intrQueue);
        _intrQueue = nullptr;
        return false;
    }

    if (gpio_isr_handler_add((gpio_num_t)_intPin, _isrHandler, this) != ESP_OK) {
        vQueueDelete(_intrQueue);
        _intrQueue = nullptr;
        return false;
    }

    // 创建任务以处理中断
    // Create task to handle interrupts
    BaseType_t ok = xTaskCreatePinnedToCore(
        [](void* arg) {
            M5IOE1* self = static_cast<M5IOE1*>(arg);
            uint32_t evt;
            while (true) {
                if (xQueueReceive(self->_intrQueue, &evt, portMAX_DELAY) == pdTRUE) {
                    gpio_intr_disable((gpio_num_t)self->_intPin);
                    self->_handleInterrupt();
                    gpio_intr_enable((gpio_num_t)self->_intPin);
                }
            }
        },
        "m5ioe1_intr", 4096, this, configMAX_PRIORITIES - 2, &_pollTask, tskNO_AFFINITY);

    if (ok != pdPASS) {
        gpio_isr_handler_remove((gpio_num_t)_intPin);
        vQueueDelete(_intrQueue);
        _intrQueue = nullptr;
        return false;
    }

    return true;
}

bool M5IOE1::_setupPolling()
{
    BaseType_t ok = xTaskCreatePinnedToCore(_pollTaskFunc, "m5ioe1_poll", 4096, this, 5, &_pollTask, tskNO_AFFINITY);
    return ok == pdPASS;
}

void M5IOE1::_cleanupPolling()
{
    if (_pollTask) {
        vTaskDelete(_pollTask);
        _pollTask = nullptr;
    }
}

void M5IOE1::_cleanupHardwareInterrupt()
{
    if (_intrQueue) {
        vQueueDelete(_intrQueue);
        _intrQueue = nullptr;
    }
    if (_intPin >= 0) {
        // 移除 GPIO ISR handler
        // Remove GPIO ISR handler
        // 如果返回错误（如"GPIO isr service is not installed"）可忽略
        // The returned error (e.g., "GPIO isr service is not installed") can be ignored
        // This error indicates that the ISR service is not currently installed and does not need to be removed, which
        // is expected behavior
        esp_err_t err = gpio_isr_handler_remove((gpio_num_t)_intPin);
        if (err != ESP_OK) {
            M5IOE1_LOG_W(TAG_IRQ,
                         "gpio_isr_handler_remove failed (can be ignored if ISR service was not installed): %s",
                         esp_err_to_name(err));
        }
    }
}

#endif
