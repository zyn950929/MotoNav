/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * M5IOE1 轮询中断示例
 * M5IOE1 Interrupt Polling Example
 *
 * 本示例演示如何在没有物理中断引脚的情况下，以轮询模式使用 M5IOE1 库
 * This example demonstrates how to use the M5IOE1 library in POLLING mode
 * without a physical interrupt pin. The library periodically polls the
 * interrupt status registers to detect GPIO changes.
 *
 * 硬件连接
 * Hardware Connections:
 * - M5IOE1 SDA -> GPIO 38 (默认，可配置) (default, configurable)
 * - M5IOE1 SCL -> GPIO 39 (默认，可配置) (default, configurable)
 * - IO1 (M5IOE1) -> 按钮或开关 (连接到 GND 为低电平有效) (Button or switch, connect to GND for active LOW)
 *
 * 演示功能
 * Features demonstrated:
 * - 以轮询模式初始化 M5IOE1 (无 INT 引脚) (Initializing M5IOE1 in polling mode, no INT pin)
 * - 为引脚 1 (IO1) 附加下降沿触发的中断回调 (Attaching interrupt callback to pin 1 with FALLING edge)
 * - 读取设备信息 (UID、版本) (Reading device information: UID, version)
 * - 使用 attachInterrupt() 回调进行事件处理 (Using attachInterrupt() callback for event handling)
 * - 轮询间隔配置 (Polling interval configuration)
 */

#include <M5IOE1.h>

// M5IOE1 设备实例
// M5IOE1 device instance
M5IOE1 ioe1;

// I2C 配置
// I2C configuration
#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39
#define I2C_FREQ    400000

// M5IOE1 I2C 地址 (默认 0x6F)
// M5IOE1 I2C address (default 0x6F)
#define I2C_ADDR M5IOE1_DEFAULT_ADDR

// 引脚定义
// Pin definitions
#define IOE1_PIN_1 M5IOE1_PIN_1  // M5IOE1 上的 IO1 (引脚索引 0) (IO1 on M5IOE1, pin index 0)

// 中断事件计数器
// Counter for interrupt events
volatile int interruptCounter = 0;

// 引脚 1 下降沿中断的回调函数
// Callback function for pin 1 falling edge interrupt
void IRAM_ATTR pin1FallingCallback()
{
    interruptCounter++;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("M5IOE1 Interrupt Polling Example");
    Serial.println("========================================\n");

    // 设置日志级别为 INFO (默认)
    // Set log level to INFO (default)
    M5IOE1::setLogLevel(M5IOE1_LOG_LEVEL_INFO);

    // 以轮询模式初始化 M5IOE1 (无物理 INT 引脚)
    // Initialize M5IOE1 in POLLING mode (no physical INT pin)
    // 当未提供 intPin (或设置为 -1) 时，仅支持轮询和禁用模式
    // When intPin is not provided (or set to -1), only POLLING and DISABLED modes are supported
    Serial.println("Initializing M5IOE1 in polling mode...");

    if (ioe1.begin(&Wire, I2C_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ, -1, M5IOE1_INT_MODE_POLLING) != M5IOE1_OK) {
        Serial.println("ERROR: Failed to initialize M5IOE1!");
        Serial.println("Please check:");
        Serial.println("  - I2C connections (SDA=" + String(I2C_SDA_PIN) + ", SCL=" + String(I2C_SCL_PIN) + ")");
        Serial.println("  - M5IOE1 I2C address (0x" + String(I2C_ADDR, HEX) + ")");
        Serial.println("  - Power supply to M5IOE1");
        while (1) {
            delay(1000);
        }
    }

    Serial.println("M5IOE1 initialized successfully!\n");

    // 读取并显示设备信息
    // Read and display device information
    uint16_t uid;
    uint8_t version;

    if (ioe1.getUID(&uid) == M5IOE1_OK) {
        Serial.println("Device UID: 0x" + String(uid, HEX));
    }

    if (ioe1.getVersion(&version) == M5IOE1_OK) {
        Serial.println("Firmware Version: " + String(version));
    }

    uint16_t refVoltage;
    if (ioe1.getRefVoltage(&refVoltage) == M5IOE1_OK) {
        Serial.println("Reference Voltage: " + String(refVoltage) + " mV");
    }

    Serial.println();

    // 将 IO1 配置为带上拉电阻的输入
    // Configure IO1 as input with pull-up
    Serial.println("Configuring IO1 as input with internal pull-up...");
    ioe1.pinMode(IOE1_PIN_1, INPUT_PULLUP);
    Serial.println("IO1 configured successfully!\n");

    // 为引脚 1 附加下降沿触发的中断
    // Attach interrupt to pin 1 with FALLING edge trigger
    // 在轮询模式下，库会定期检查中断状态，并在 IO1 上检测到下降沿时调用此回调
    // In polling mode, the library will periodically check the interrupt status
    // and call this callback when a falling edge is detected on IO1
    Serial.println("Attaching interrupt callback to IO1 (FALLING edge)...");
    ioe1.attachInterrupt(IOE1_PIN_1, pin1FallingCallback, FALLING);
    Serial.println("Interrupt attached successfully!\n");

    // 设置轮询间隔为 1 秒 (1000ms)
    // Set polling interval to 1 second (1000ms)
    // 默认为 5000ms。更短的间隔 = 更快的响应，但 CPU 使用率更高。
    // Default is 5000ms. Shorter intervals = faster response but more CPU usage.
    Serial.println("Setting polling interval to 1.0 second...");
    if (ioe1.setPollingInterval(1.0f) == M5IOE1_OK) {
        Serial.println("Polling interval configured successfully!\n");
    } else {
        Serial.println("WARNING: Failed to set polling interval\n");
    }

    Serial.println("========================================");
    Serial.println("Setup complete! Ready for interrupts.");
    Serial.println("========================================\n");

    Serial.println("Instructions:");
    Serial.println("  - Connect a button between IO1 and GND");
    Serial.println("  - Press the button to trigger FALLING edge interrupt");
    Serial.println("  - The library polls interrupt status every 1 second");
    Serial.println("  - Watch for interrupt events in the serial monitor\n");
}

void loop()
{
    // 存储当前计数器值以检测变化
    // Store current counter value to detect changes
    static int lastCounter = 0;

    // 检查中断计数器是否发生变化
    // Check if interrupt counter changed
    if (interruptCounter != lastCounter) {
        Serial.println(">>> INTERRUPT TRIGGERED! Count: " + String(interruptCounter) + " <<<");
        lastCounter = interruptCounter;

        // 读取 IO1 的当前状态
        // Read current state of IO1
        int pinState = ioe1.digitalRead(IOE1_PIN_1);
        Serial.println("    IO1 state: " + String(pinState == LOW ? "LOW (pressed)" : "HIGH (released)"));

        // 读取并显示中断状态寄存器
        // Read and display interrupt status register
        uint16_t status = 0;
        ioe1.getInterruptStatus(&status);
        Serial.println("    Interrupt status register: 0b" + String(status, BIN) + "\n");

        // 清除此引脚的中断
        // Clear the interrupt for this pin
        ioe1.clearInterrupt(IOE1_PIN_1);
    }

    // 可选：手动轮询检查 (库会在后台自动执行此操作)
    // Optional: Manual polling check (the library does this automatically in background)
    // 这仅用于演示 - 库会自动处理轮询
    // This is just for demonstration - the library handles polling automatically
    static unsigned long lastStatusCheck = 0;
    if (millis() - lastStatusCheck > 5000) {
        lastStatusCheck = millis();

        // 显示当前轮询模式信息
        // Display current polling mode info
        Serial.println("--- Status Update ---");
        Serial.println("    Total interrupts: " + String(interruptCounter));
        Serial.println("    IO1 current state: " + String(ioe1.digitalRead(IOE1_PIN_1) == LOW ? "LOW" : "HIGH"));
        Serial.println("    Polling mode: ACTIVE (background task)\n");
    }

    delay(100);  // 小延迟以防止过度的 CPU 使用 / Small delay to prevent excessive CPU usage
}
