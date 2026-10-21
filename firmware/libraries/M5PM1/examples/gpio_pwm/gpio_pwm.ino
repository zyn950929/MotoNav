/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <M5PM1.h>

/*
 * GPIO 输入/输出 + PWM 呼吸灯示例。
 * GPIO input/output + PWM breathing LED demo.
 *
 * 接线建议：
 *      - GPIO0：接LED(串电阻)到GND，演示普通GPIO翻转。
 *      - GPIO1：接按键到GND（内部上拉），用于输入读取。
 *      - PWM_CH_0 对应 GPIO3：接LED(串电阻)到GND，演示PWM呼吸灯。
 * Wiring suggestion:
 *      - GPIO0: LED (with resistor) to GND for GPIO toggling.
 *      - GPIO1: Button to GND (internal pull-up) for input reading.
 *      - PWM_CH_0 maps to GPIO3: LED (with resistor) to GND for PWM breathing.
 *
 * 注意：PWM 使用 GPIO3/4，且需要将引脚功能设为 M5PM1_OTHER。
 * Note: PWM uses GPIO3/4 and requires GPIO func set to M5PM1_OTHER.
 *
 * 若使用 NeoPixel，请勿将 GPIO0 用作普通 GPIO。
 * If using NeoPixel, do not use GPIO0 as a normal GPIO.
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

static const m5pm1_gpio_num_t GPIO_OUT  = M5PM1_GPIO_NUM_0;
static const m5pm1_gpio_num_t GPIO_IN   = M5PM1_GPIO_NUM_1;
static const m5pm1_pwm_channel_t PWM_CH = M5PM1_PWM_CH_0;

static void printDivider()
{
    Serial.println("--------------------------------------------------");
}

void setup()
{
    Serial.begin(115200);
    delay(200);
    printDivider();
    LOGI("GPIO + PWM demo start");

    m5pm1_err_t err = pm1.begin(&Wire, M5PM1_DEFAULT_ADDR, PM1_I2C_SDA, PM1_I2C_SCL, PM1_I2C_FREQ);
    if (err != M5PM1_OK) {
        LOGE("PM1 begin failed: %d", err);
        while (true) {
            delay(1000);
        }
    }

    // GPIO0 作为普通输出脚。
    // GPIO0 works as a normal output pin.
    pm1.pinMode(GPIO_OUT, OUTPUT);
    pm1.digitalWrite(GPIO_OUT, LOW);

    // GPIO1 作为输入脚，内部上拉，按键接GND即可触发低电平。
    // GPIO1 as input with pull-up; button to GND pulls it low.
    pm1.pinMode(GPIO_IN, INPUT_PULLUP);

    // PWM_CH_0 对应 GPIO3，需要设为 M5PM1_OTHER 才能输出PWM。
    // PWM_CH_0 maps to GPIO3; set func to M5PM1_OTHER for PWM output.
    pm1.pinMode(M5PM1_GPIO_NUM_3, M5PM1_OTHER);
    // PWM 频率全通道共享。
    // PWM frequency is shared across channels.
    pm1.setPwmFrequency(1000);
    pm1.setPwmDuty(PWM_CH, 0, false, true);

    LOGI("GPIO OUT=%u, GPIO IN=%u, PWM CH=%u", GPIO_OUT, GPIO_IN, PWM_CH);
    printDivider();
}

void loop()
{
    static uint32_t lastBlinkMs = 0;
    static uint32_t lastLogMs   = 0;
    static uint32_t lastPwmMs   = 0;
    static bool ledOn           = false;
    static int duty             = 0;
    static int step             = 2;

    uint32_t now = millis();

    if (now - lastBlinkMs >= 500) {
        lastBlinkMs = now;
        ledOn       = !ledOn;
        pm1.digitalWrite(GPIO_OUT, ledOn ? HIGH : LOW);
    }

    if (now - lastPwmMs >= 20) {
        lastPwmMs = now;
        duty += step;
        if (duty >= 100) {
            duty = 100;
            step = -step;
        } else if (duty <= 0) {
            duty = 0;
            step = -step;
        }
        pm1.setPwmDuty(PWM_CH, static_cast<uint8_t>(duty), false, true);
    }

    if (now - lastLogMs >= 1000) {
        lastLogMs   = now;
        int inLevel = pm1.digitalRead(GPIO_IN);
        LOGI("GPIO_IN=%d PWM_DUTY=%d%%", inLevel, duty);
    }
}
