/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <M5PM1.h>

/*
 * 基础电源/ADC示例：读取设备信息、电压、ADC/温度，并演示电源轨开关。
 * Basic power/ADC demo: read device info, voltages, ADC/temp, and toggle rails.
 *
 * 接线：PM1通过I2C连接；若PM1_I2C_SDA/SCL为-1则使用默认Wire引脚。
 * Wiring: PM1 via I2C; if PM1_I2C_SDA/SCL = -1, default Wire pins are used.
 *
 * ADC1=GPIO1、ADC2=GPIO2，使用前需将GPIO功能设为 OTHER。
 * ADC1=GPIO1, ADC2=GPIO2; set GPIO func to OTHER before ADC reads.
 *
 * 注意：切换DCDC/LDO/5VINOUT可能影响外接负载供电，请确认不会误断电。一般DCDC为芯片提供供电，若关闭DCDC则芯片有可能直接下电。
 * Note: toggling DCDC/LDO/5VINOUT may affect external loads; typically DCDC powers the chip, disabling it may cause
 * immediate power loss.
 */

M5PM1 pm1;

#define LOGI(fmt, ...) Serial.printf("[PM1][I] " fmt "\r\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[PM1][W] " fmt "\r\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) Serial.printf("[PM1][E] " fmt "\r\n", ##__VA_ARGS__)

#ifndef PM1_I2C_SDA
#define PM1_I2C_SDA 47
#endif
#ifndef PM1_I2C_SCL
#define PM1_I2C_SCL 48
#endif
#ifndef PM1_I2C_FREQ
#define PM1_I2C_FREQ M5PM1_I2C_FREQ_100K
#endif

static const uint8_t PWR_CFG_CHG  = 0x01;
static const uint8_t PWR_CFG_DCDC = 0x02;
static const uint8_t PWR_CFG_LDO  = 0x04;
static const uint8_t PWR_CFG_5V   = 0x08;

static void printDivider()
{
    Serial.println("--------------------------------------------------");
}

static void printDeviceInfo()
{
    uint8_t id    = 0;
    uint8_t model = 0;
    uint8_t hw    = 0;
    uint8_t sw    = 0;

    if (pm1.getDeviceId(&id) == M5PM1_OK) {
        LOGI("Device ID: 0x%02X", id);
    }
    if (pm1.getDeviceModel(&model) == M5PM1_OK) {
        LOGI("Device Model: 0x%02X", model);
    }
    if (pm1.getHwVersion(&hw) == M5PM1_OK) {
        LOGI("HW Version: 0x%02X", hw);
    }
    if (pm1.getSwVersion(&sw) == M5PM1_OK) {
        LOGI("SW Version: 0x%02X", sw);
    }
}

static void printPowerConfig(uint8_t cfg)
{
    LOGI("Power CFG: CHG=%s DCDC=%s LDO=%s 5V=%s", (cfg & PWR_CFG_CHG) ? "ON" : "OFF",
         (cfg & PWR_CFG_DCDC) ? "ON" : "OFF", (cfg & PWR_CFG_LDO) ? "ON" : "OFF", (cfg & PWR_CFG_5V) ? "ON" : "OFF");
}

static void demoPowerRails()
{
    uint8_t cfg = 0;
    if (pm1.getPowerConfig(&cfg) != M5PM1_OK) {
        LOGW("Get power config failed");
        return;
    }

    LOGI("Power rail demo start (only enable)");
    printPowerConfig(cfg);

    // 只开启不关闭。
    // Only enable, do not disable.

    pm1.setChargeEnable(true);
    LOGI("Charge -> ON");
    delay(300);

    pm1.setDcdcEnable(true);
    LOGI("DCDC -> ON");
    delay(300);

    pm1.setLdoEnable(true);
    LOGI("LDO -> ON");
    delay(300);

    pm1.setBoostEnable(true);
    LOGI("5V IN/OUT -> ON");
    delay(300);

    if (pm1.getPowerConfig(&cfg) == M5PM1_OK) {
        printPowerConfig(cfg);
    }
    LOGI("Power rail demo done");
}

static void printVoltages()
{
    uint16_t mv = 0;

    // 电压读数单位为 mV。
    // Voltage readings are in mV.

    if (pm1.readVref(&mv) == M5PM1_OK) {
        LOGI("Vref: %u mV (%.3f V)", mv, mv / 1000.0f);
    }
    if (pm1.readVbat(&mv) == M5PM1_OK) {
        LOGI("VBAT: %u mV (%.3f V)", mv, mv / 1000.0f);
    }
    if (pm1.readVin(&mv) == M5PM1_OK) {
        LOGI("VIN: %u mV (%.3f V)", mv, mv / 1000.0f);
    }
    if (pm1.read5VInOut(&mv) == M5PM1_OK) {
        LOGI("5V IN/OUT: %u mV (%.3f V)", mv, mv / 1000.0f);
    }
}

static void printAdcAndTemp()
{
    uint16_t adc  = 0;
    uint16_t temp = 0;

    // ADC1=GPIO1，ADC2=GPIO2
    // ADC1=GPIO1, ADC2=GPIO2

    if (pm1.analogRead(M5PM1_ADC_CH_1, &adc) == M5PM1_OK) {
        LOGI("ADC1 (GPIO1): %u", adc);
    }
    if (pm1.analogRead(M5PM1_ADC_CH_2, &adc) == M5PM1_OK) {
        LOGI("ADC2 (GPIO2): %u", adc);
    }
    if (pm1.readTemperature(&temp) == M5PM1_OK) {
        LOGI("Temp: %u", temp);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    printDivider();
    LOGI("Basic + ADC + Power demo start");

    m5pm1_err_t err = pm1.begin(&Wire, M5PM1_DEFAULT_ADDR, PM1_I2C_SDA, PM1_I2C_SCL, PM1_I2C_FREQ);
    if (err != M5PM1_OK) {
        LOGE("PM1 begin failed: %d", err);
        while (true) {
            delay(1000);
        }
    }

    printDeviceInfo();
    printDivider();

    demoPowerRails();
    printDivider();

    // ADC通道需要将GPIO1/2切到 OTHER 功能。
    // ADC channels require GPIO1/2 to be set as OTHER function.
    pm1.gpioSetFunc(M5PM1_GPIO_NUM_1, M5PM1_GPIO_FUNC_OTHER);
    pm1.gpioSetFunc(M5PM1_GPIO_NUM_2, M5PM1_GPIO_FUNC_OTHER);
}

void loop()
{
    static uint32_t lastMs    = 0;
    const uint32_t intervalMs = 2000;

    if (millis() - lastMs >= intervalMs) {
        lastMs = millis();
        // 每2秒输出一次电压、ADC、温度信息。
        // Print voltages, ADC, and temperature every 2 seconds.
        printVoltages();
        printAdcAndTemp();
        printDivider();
    }
}
