/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <M5PM1.h>

/*
 * NeoPixel 彩虹渐变示例（使用 GPIO0 作为 LED_EN/数据相关功能）。
 * NeoPixel rainbow demo (uses GPIO0 for LED_EN/data-related function).
 *
 * 注意：NeoPixel 仅支持 GPIO0，LED 数量最大 32（固件限制）。
 * Note: NeoPixel is only supported on GPIO0, and LED count max is 32 (firmware limit).
 *
 * GPIO0 NeoPixel 初始化提供两种等效方案，按需选择其一：
 * Two equivalent GPIO0 NeoPixel init approaches — choose one:
 *
 *   方案1（分步配置）/ Approach 1 (step-by-step):
 *     pm1.gpioSetFunc(M5PM1_GPIO_NUM_0, M5PM1_GPIO_FUNC_OTHER);
 *     pm1.gpioSetDrive(M5PM1_GPIO_NUM_0, M5PM1_GPIO_DRIVE_PUSHPULL);
 *     pm1.gpioSetOutput(M5PM1_GPIO_NUM_0, true);
 *
 *   方案2（简化调用，库内部等效方案1）/ Approach 2 (simplified, internally equivalent):
 *     pm1.pinMode(M5PM1_GPIO_NUM_0, M5PM1_OTHER);
 *
 * 本示例默认使用方案2。修改 USE_INIT_APPROACH 为 1 可切换至方案1。
 * This demo defaults to Approach 2. Set USE_INIT_APPROACH to 1 for Approach 1.
 */

// 选择初始化方案：1=分步配置  2=简化调用
// Select init approach: 1=step-by-step  2=simplified
#define USE_INIT_APPROACH 2

M5PM1 pm1;

#define LOGI(fmt, ...) Serial.printf("[PM1][I] " fmt "\r\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[PM1][W] " fmt "\r\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) Serial.printf("[PM1][E] " fmt "\r\n", ##__VA_ARGS__)

#ifndef PM1_I2C_SDA
#define PM1_I2C_SDA 48
#endif
#ifndef PM1_I2C_SCL
#define PM1_I2C_SCL 47
#endif
#ifndef PM1_I2C_FREQ
#define PM1_I2C_FREQ M5PM1_I2C_FREQ_100K
#endif

static const uint8_t LED_COUNT  = 1;
static const uint8_t BRIGHTNESS = 64;

static void printDivider()
{
    Serial.println("--------------------------------------------------");
}

static uint8_t scale(uint8_t v, uint8_t scale)
{
    // 通过亮度系数缩放颜色，避免过亮。
    // Scale color by brightness to avoid excessive intensity.
    return static_cast<uint8_t>((static_cast<uint16_t>(v) * scale) / 255);
}

static m5pm1_rgb_t wheel(uint8_t pos)
{
    pos           = 255 - pos;
    m5pm1_rgb_t c = {0, 0, 0};
    if (pos < 85) {
        c.r = 255 - pos * 3;
        c.g = 0;
        c.b = pos * 3;
    } else if (pos < 170) {
        pos -= 85;
        c.r = 0;
        c.g = pos * 3;
        c.b = 255 - pos * 3;
    } else {
        pos -= 170;
        c.r = pos * 3;
        c.g = 255 - pos * 3;
        c.b = 0;
    }
    c.r = scale(c.r, BRIGHTNESS);
    c.g = scale(c.g, BRIGHTNESS);
    c.b = scale(c.b, BRIGHTNESS);
    return c;
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    printDivider();
    LOGI("NeoPixel demo start");

    m5pm1_err_t err = pm1.begin(&Wire, M5PM1_DEFAULT_ADDR, PM1_I2C_SDA, PM1_I2C_SCL, PM1_I2C_FREQ);
    if (err != M5PM1_OK) {
        LOGE("PM1 begin failed: %d", err);
        while (true) {
            delay(1000);
        }
    }

#if USE_INIT_APPROACH == 1
    // ---- 方案1：分步配置 ----
    // ---- Approach 1: step-by-step configuration ----
    // 将 GPIO0 功能切换为 OTHER（NeoPixel/LED_EN）
    // Switch GPIO0 function to OTHER (NeoPixel/LED_EN)
    pm1.gpioSetFunc(M5PM1_GPIO_NUM_0, M5PM1_GPIO_FUNC_OTHER);
    // 设置推挽驱动
    // Set push-pull drive
    pm1.gpioSetDrive(M5PM1_GPIO_NUM_0, M5PM1_GPIO_DRIVE_PUSHPULL);
    // 输出高电平使能
    // Output high to enable
    pm1.gpioSetOutput(M5PM1_GPIO_NUM_0, true);
    LOGI("GPIO0 NeoPixel init: Approach 1 (step-by-step)");
#else
    // ---- 方案2：简化调用（库内部等效方案1）----
    // ---- Approach 2: simplified (internally equivalent to Approach 1) ----
    pm1.pinMode(M5PM1_GPIO_NUM_0, M5PM1_OTHER);
    LOGI("GPIO0 NeoPixel init: Approach 2 (simplified)");
#endif

    // setLedEnLevel() 主要用于 Stamp-S3Bat 产品，该产品使用默认的指示灯引脚作为RGB的供电引脚。
    // 其他产品可根据硬件设计决定是否调用。默认保留以兼容 Stamp-S3Bat。
    // setLedEnLevel() is mainly for the Stamp-S3Bat product, which uses the default indicator LED pin as the power supply pin for RGB.
    // Other products may or may not need this depending on hardware design. Kept by default for Stamp-S3Bat compatibility.
    pm1.setLedEnLevel(true);

    // 设置灯珠数量（1-32，固件最大支持32）。
    // Set LED count (1-32, firmware max 32).
    pm1.setLedCount(LED_COUNT);

    LOGI("LED count: %u", LED_COUNT);
    printDivider();
}

void loop()
{
    static uint8_t offset = 0;

    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        // 计算彩虹色，并写入缓存。
        // Compute rainbow color and write to buffer.
        m5pm1_rgb_t c = wheel(static_cast<uint8_t>(i * 256 / LED_COUNT + offset));
        pm1.setLedColor(i, c);
    }
    // 刷新后才会显示到灯带。
    // Refresh is required to apply colors to LEDs.
    pm1.refreshLeds();

    offset++;
    delay(40);
}
