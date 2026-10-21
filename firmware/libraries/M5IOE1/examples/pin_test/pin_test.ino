/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * M5IOE1 引脚测试示例
 * M5IOE1 Pin Test Example
 *
 * 本示例展示 M5IOE1 库的核心功能
 * This example demonstrates the core features of M5IOE1 library
 *
 * 硬件连接 / Hardware Connections:
 * - M5IOE1 SDA -> GPIO 38, SCL -> GPIO 39
 * - IO1 -> 按钮 (Button, active LOW to GND)
 * - IO3 -> LED (through 220Ω to GND)
 * - IO2 -> 电位器 / Potentiometer (ADC Channel 1, 0-3.3V)
 * - IO9 -> LED (PWM Channel 0, through 220Ω to GND)
 */

#include <M5IOE1.h>

// ============================
// 配置 / Configuration
// ============================
#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39
#define I2C_FREQ    400000
#define I2C_ADDR    M5IOE1_DEFAULT_ADDR

// ============================
// 引脚定义 / Pin Definitions
// ============================
// IO1 (Pin 0) - 数字输入，按钮 / Digital input, button
#define PIN_BUTTON M5IOE1_PIN_1  // IO1
// IO3 (Pin 2) - 数字输出，LED / Digital output, LED
#define PIN_LED M5IOE1_PIN_3  // IO3
// IO2 (Pin 1) - ADC 输入，电位器 / ADC input, potentiometer (ADC Channel 1)
#define PIN_ADC M5IOE1_PIN_2  // IO2 -> ADC_CH1
// IO9 (Pin 8) - PWM 输出，LED / PWM output, LED (PWM Channel 0)
#define PIN_PWM M5IOE1_PIN_9  // IO9 -> PWM_CH1

// ============================
// 全局变量 / Global Variables
// ============================
M5IOE1 ioe1;

volatile int buttonPressCount = 0;
uint8_t pwmValue              = 0;
int pwmDirection              = 1;

// ============================
// 中断回调函数 / Interrupt Callback
// ============================
void IRAM_ATTR buttonPressed()
{
    buttonPressCount++;
}

// ============================
// 辅助函数 / Helper Functions
// ============================
void printSeparator(const char* title)
{
    Serial.println("\n========================================");
    Serial.println(title);
    Serial.println("========================================\n");
}

void testDigitalWrite()
{
    printSeparator("测试 digitalWrite() / Testing digitalWrite()");

    Serial.println("Blinking LED on IO3 (3 cycles)...\n");

    for (int i = 0; i < 3; i++) {
        Serial.print("Cycle ");
        Serial.print(i + 1);
        Serial.println(": HIGH -> LOW");
        ioe1.digitalWrite(PIN_LED, HIGH);
        delay(500);
        ioe1.digitalWrite(PIN_LED, LOW);
        delay(500);
    }

    Serial.println("digitalWrite() test completed!\n");
}

void testDigitalRead()
{
    printSeparator("测试 digitalRead() / Testing digitalRead()");

    Serial.println("Reading button on IO1 (active LOW) for 8 seconds...");
    Serial.println("Press the button to test!\n");

    unsigned long startTime = millis();
    int lastState           = -1;

    while (millis() - startTime < 8000) {
        int currentState = ioe1.digitalRead(PIN_BUTTON);

        if (currentState != lastState) {
            Serial.print("State: ");
            Serial.println(currentState == LOW ? "LOW (pressed)" : "HIGH (released)");
            lastState = currentState;
        }
        delay(50);
    }

    Serial.println("\ndigitalRead() test completed!\n");
}

void testPinMode()
{
    printSeparator("测试 pinMode() / Testing pinMode()");

    Serial.println("Testing pin modes on IO3:\n");

    // OUTPUT + 闪烁测试 / OUTPUT + Blink test
    Serial.println("1. OUTPUT mode");
    ioe1.pinMode(PIN_LED, OUTPUT);
    for (int i = 0; i < 2; i++) {
        ioe1.digitalWrite(PIN_LED, HIGH);
        delay(200);
        ioe1.digitalWrite(PIN_LED, LOW);
        delay(200);
    }
    Serial.println("   -> Blink test OK\n");

    // INPUT + 读取测试 / INPUT + Read test
    Serial.println("2. INPUT mode (floating)");
    ioe1.pinMode(PIN_LED, INPUT);
    int val = ioe1.digitalRead(PIN_LED);
    Serial.print("   -> Read: ");
    Serial.println(val == HIGH ? "HIGH" : "LOW");
    Serial.println("   -> OK\n");

    // INPUT_PULLUP + 读取测试 / INPUT_PULLUP + Read test
    Serial.println("3. INPUT_PULLUP mode");
    ioe1.pinMode(PIN_LED, INPUT_PULLUP);
    val = ioe1.digitalRead(PIN_LED);
    Serial.print("   -> Read: ");
    Serial.println(val == HIGH ? "HIGH (pulled up)" : "LOW");
    Serial.println("   -> OK\n");

    // 恢复输出模式 / Restore OUTPUT mode
    ioe1.pinMode(PIN_LED, OUTPUT);

    Serial.println("pinMode() test completed!\n");
}

void testAnalogRead()
{
    printSeparator("测试 analogRead() / Testing analogRead()");

    Serial.println("Reading ADC Channel 1 (IO2) for 8 seconds...");
    Serial.println("Rotate potentiometer to test!\n");

    unsigned long startTime = millis();
    unsigned long lastPrint = 0;

    while (millis() - startTime < 8000) {
        uint16_t adcValue;

        if (ioe1.analogRead(1, &adcValue) == M5IOE1_OK) {
            if (millis() - lastPrint > 500) {
                float voltage = (adcValue * 3300.0f) / 4095.0f;

                Serial.print("Value: ");
                Serial.print(adcValue);
                Serial.print(" (");
                Serial.print((int)voltage);
                Serial.println(" mV)");

                // 简单的条形图 / Simple bar graph
                int bars = map(adcValue, 0, 4095, 0, 20);
                Serial.print("[");
                for (int i = 0; i < 20; i++) {
                    Serial.print(i < bars ? "=" : " ");
                }
                Serial.println("]\n");

                lastPrint = millis();
            }
        }
        delay(50);
    }

    Serial.println("analogRead() test completed!\n");
}

void testAnalogWrite()
{
    printSeparator("测试 analogWrite() / Testing analogWrite()");

    Serial.println("PWM breathing on IO9 (Channel 0)...\n");

    if (ioe1.setPwmFrequency(5000) == M5IOE1_OK) {
        Serial.println("PWM frequency: 5000 Hz\n");
    } else {
        Serial.println("WARNING: Failed to set PWM frequency\n");
    }

    unsigned long startTime = millis();

    while (millis() - startTime < 8000) {
        if (ioe1.analogWrite(0, pwmValue) == M5IOE1_OK) {
            if (pwmValue % 25 == 0) {
                float duty = (pwmValue * 100.0f) / 255.0f;
                Serial.print("PWM value: ");
                Serial.print(pwmValue);
                Serial.print(" (");
                Serial.print((int)duty);
                Serial.println("%)");
            }
        }

        // 呼吸效果 / Breathing effect
        pwmValue += pwmDirection * 5;
        if (pwmValue >= 255) {
            pwmValue     = 255;
            pwmDirection = -1;
        } else if (pwmValue <= 0) {
            pwmValue     = 0;
            pwmDirection = 1;
        }

        delay(20);
    }

    if (ioe1.analogWrite(0, 0) != M5IOE1_OK) {
        Serial.println("WARNING: Failed to stop PWM output\n");
    }
    Serial.println("\nPWM turned off.\n");

    Serial.println("analogWrite() test completed!\n");
}

void testAttachInterrupt()
{
    printSeparator("测试 attachInterrupt() / Testing attachInterrupt()");

    Serial.println("Configuring button with interrupt (FALLING edge)...\n");

    ioe1.pinMode(PIN_BUTTON, INPUT_PULLUP);
    buttonPressCount = 0;
    ioe1.attachInterrupt(PIN_BUTTON, buttonPressed, FALLING);

    Serial.println("Interrupt attached! Press button for 8 seconds...\n");

    unsigned long startTime = millis();
    int lastCount           = 0;

    while (millis() - startTime < 8000) {
        if (buttonPressCount != lastCount) {
            Serial.print("Button press detected! Count: ");
            Serial.println(buttonPressCount);
            lastCount = buttonPressCount;
        }
        delay(50);
    }

    Serial.print("\nTotal presses: ");
    Serial.println(buttonPressCount);

    ioe1.detachInterrupt(PIN_BUTTON);
    Serial.println("Interrupt detached.\n");

    Serial.println("attachInterrupt() test completed!\n");
}

// ============================
// Arduino setup() / loop()
// ============================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n######################################");
    Serial.println("#     M5IOE1 Pin Test Example      #");
    Serial.println("######################################\n");

    // 设置日志级别 / Set log level
    M5IOE1::setLogLevel(M5IOE1_LOG_LEVEL_WARN);

    // 初始化 M5IOE1 / Initialize M5IOE1
    Serial.println("Initializing M5IOE1...");
    if (ioe1.begin(&Wire, I2C_ADDR, I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ) != M5IOE1_OK) {
        Serial.println("ERROR: Failed to initialize M5IOE1!");
        Serial.println("Please check I2C connections and power supply.");
        while (1) {
            delay(1000);
        }
    }
    Serial.println("M5IOE1 initialized successfully!\n");

    // 读取设备信息 / Read device info
    uint16_t uid;
    uint8_t version;
    if (ioe1.getUID(&uid) == M5IOE1_OK) {
        Serial.println("Device UID: 0x" + String(uid, HEX));
    }
    if (ioe1.getVersion(&version) == M5IOE1_OK) {
        Serial.println("Firmware Version: " + String(version));
    }
    Serial.println();

    // 初始化测试引脚 / Initialize test pins
    Serial.println("Initializing test pins...");
    ioe1.pinMode(PIN_BUTTON, INPUT_PULLUP);
    ioe1.pinMode(PIN_LED, OUTPUT);
    ioe1.digitalWrite(PIN_LED, LOW);
    Serial.println("Test pins ready!\n");

    delay(2000);
}

void loop()
{
    static int testStep = 0;

    switch (testStep) {
        case 0:
            testPinMode();
            break;
        case 1:
            testDigitalWrite();
            break;
        case 2:
            testDigitalRead();
            break;
        case 3:
            testAnalogRead();
            break;
        case 4:
            testAnalogWrite();
            break;
        case 5:
            testAttachInterrupt();
            break;
        default:
            Serial.println("\n\n######################################");
            Serial.println("#   All tests completed!             #");
            Serial.println("#   Restarting in 5 seconds...       #");
            Serial.println("######################################\n");
            delay(5000);
            testStep = -1;
            break;
    }

    testStep++;

    if (testStep != 0) {
        Serial.println("Next test starting in 2 seconds...\n");
        delay(2000);
    }
}
