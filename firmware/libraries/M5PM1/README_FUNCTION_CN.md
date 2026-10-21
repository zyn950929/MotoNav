# M5PM1 功能总览（中文）

面向 Arduino 与 ESP-IDF 的 PM1 电源管理驱动库功能分类说明。

- **版本**: 1.0.6
- **默认 I2C 地址**: `0x6E`
- **I2C 速度**: 默认 100 kHz，支持切换至 400 kHz

## 项目结构与角色

- `src/M5PM1.h`: 公共 API、寄存器/枚举、缓存结构定义。
- `src/M5PM1.cpp`: 驱动实现、冲突校验、快照与缓存逻辑。
- `src/M5PM1_i2c_compat.h`: Arduino / ESP-IDF / M5Unified I2C 兼容封装。

## 示例

- `examples/basic_power_adc/basic_power_adc.ino`: 初始化、设备信息、VBAT/VIN/5V 读数、ADC/温度采样、电源轨控制示例。
- `examples/gpio_pwm/gpio_pwm.ino`: GPIO 输出/输入读取与 PWM 呼吸灯示例。
- `examples/usb_interrupt_sleep/usb_interrupt_sleep.ino`: USB 插拔中断、定时器唤醒与关机示例。
- `examples/neopixel/neopixel.ino`: NeoPixel 彩虹渐变示例。

## 功能分类

### 1) 初始化与日志

- **初始化**
  - Arduino: `begin(TwoWire*, addr, sda, scl, speed)`
  - Arduino / ESP-IDF（检测到 M5Unified 时）: `begin(m5::I2C_Class*, addr, speed)`
  - ESP-IDF: `begin(i2c_port_t, addr, sda, scl, speed)`
  - ESP-IDF（IDF >= 5.3）: `begin(i2c_master_bus_handle_t, addr, speed)`
  - ESP-IDF（兼容模式可用时）: `begin(i2c_bus_handle_t, addr, speed)`
- **日志**
  - `setLogLevel` / `getLogLevel`

### 2) 设备信息

- `getDeviceId` / `getDeviceModel` / `getHwVersion` / `getSwVersion`

### 3) GPIO 基础与高级控制

- **Arduino 风格**
  - `pinMode` / `digitalWrite` / `digitalRead`
- **带返回值版本**
  - `pinModeWithRes` / `digitalWriteWithRes` / `digitalReadWithRes`
- **高级配置**
  - `gpioSet` / `gpioSetFunc` / `gpioSetMode` / `gpioSetOutput` / `gpioGetInput`
  - `gpioSetPull` / `gpioSetDrive` / `ledEnSetDrive`
  - `gpioSetWakeEnable` / `gpioSetWakeEdge`
- **引脚状态与校验**
  - `dumpPinStatus` / `verifyPinConfig` / `getPinStatus` / `getPinStatusArray`

### 4) 电源保持

- `gpioSetPowerHold` / `gpioGetPowerHold`
- `ldoSetPowerHold` / `ldoGetPowerHold`
- `boostSetPowerHold` / `boostGetPowerHold`

### 5) ADC 与温度

- `analogRead` / `isAdcBusy` / `disableAdc` / `readTemperature`

### 6) PWM

- `setPwmFrequency` / `getPwmFrequency`
- `setPwmDuty` / `getPwmDuty`（0-100%）
- `setPwmDuty12bit` / `getPwmDuty12bit`（0-4095）
- `setPwmConfig`
- `analogWrite`（0-255）

### 7) 电压读取

- `readVref` / `getRefVoltage` / `readVbat` / `readVin` / `read5VInOut`

### 8) 电源管理与电池

- `getPowerSource`
- `getWakeSource` / `clearWakeSource`
- `setPowerConfig` / `getPowerConfig` / `clearPowerConfig`
- `setChargeEnable` / `setDcdcEnable` / `setLdoEnable` / `setBoostEnable` / `setLedEnLevel`
- `setBatteryLvp`

### 9) 看门狗与定时器

- `wdtSet` / `wdtFeed` / `wdtGetCount`
- `timerSet` / `timerClear`

### 10) 按钮

- `btnSetConfig` / `btnGetState` / `btnGetFlag`
- `setSingleResetDisable`（高危） / `getSingleResetDisable`
- `setDoubleOffDisable`（高危） / `getDoubleOffDisable`

### 11) 中断

- **状态读取与清除**
  - `irqGetGpioStatus` / `irqClearGpioAll`
  - `irqGetSysStatus` / `irqClearSysAll`
  - `irqGetBtnStatus` / `irqClearBtnAll`
- **枚举式读取**
  - `irqGetGpioStatusEnum` / `irqGetSysStatusEnum` / `irqGetBtnStatusEnum`
- **中断屏蔽**
  - GPIO: `irqSetGpioMask(m5pm1_irq_gpio_t, m5pm1_irq_mask_ctrl_t)` / `irqGetGpioMask` / `irqSetGpioMaskAll(m5pm1_irq_mask_ctrl_t)` / `irqGetGpioMaskBits`
  - 系统: `irqSetSysMask(m5pm1_irq_sys_t, m5pm1_irq_mask_ctrl_t)` / `irqGetSysMask` / `irqSetSysMaskAll(m5pm1_irq_mask_ctrl_t)` / `irqGetSysMaskBits`
  - 按钮: `irqSetBtnMask(m5pm1_irq_btn_t, m5pm1_irq_mask_ctrl_t)` / `irqGetBtnMask` / `irqSetBtnMaskAll(m5pm1_irq_mask_ctrl_t)` / `irqGetBtnMaskBits`
  - 注意: 单个设置接口不支持 `ALL` / `NONE` 枚举值，批量操作请使用 `xxxMaskAll()` 函数。

### 12) 系统命令

- `sysCmd` / `shutdown` / `reboot` / `enterDownloadMode`
- `setDownloadLock`（高危） / `getDownloadLock`

### 13) NeoPixel

- `setLeds` / `setLedCount` / `setLedColor` / `refreshLeds` / `disableLeds`

### 14) AW8737A 脉冲控制

- **脉冲模式**
  - `setAw8737aPulse(pin, pulseNum, refresh)`: 设置脉冲数量（0-3）
  - `refreshAw8737aPulse`: 触发脉冲刷新
- **增益模式**
  - `setAw8737aMode(pin, mode, refresh)`: 设置增益模式（MODE_1 到 MODE_4）
  - `refreshAw8737aMode`: 触发模式刷新
- **注意事项**
  - 如果引脚不是输出模式，会自动配置为推挽输出。
  - 如果使用开漏输出，需要外部上拉电阻。
  - `refresh = NOW` 时执行后会有约 20 ms 延迟。

### 15) RTC RAM

- `writeRtcRAM` / `readRtcRAM`

### 16) I2C 配置与自动唤醒

- `setI2cConfig` / `switchI2cSpeed` / `getI2cSpeed`
- `setI2cSleepTime` / `getI2cSleepTime`
- `setAutoWakeEnable` / `isAutoWakeEnabled` / `sendWakeSignal`
- 兼容层: `M5PM1_i2c_compat.h`

### 17) 状态快照 / 缓存 / 校验

- `setAutoSnapshot` / `isAutoSnapshotEnabled` / `updateSnapshot` / `verifySnapshot`
- `validateConfig`
- `getCachedPwmFrequency` / `getCachedPwmState` / `getCachedAdcState`
- `getCachedPowerConfig` / `getCachedButtonConfig`
- `getCachedIrqMasks` / `getCachedIrqStatus`

## 兼容性与使用注意

### 与 M5Unified / M5GFX 共用 I2C

- 如果项目中使用 `M5Unified` 或 `M5GFX`，优先使用 `begin(&M5.In_I2C, addr, speed)`。
- 此时 `M5PM1` 只借用现有 I2C 句柄，不负责驱动安装和释放。
- 在包含 `M5Unified` 的 ESP-IDF 工程中，建议先包含 `M5Unified.h`，再包含 `M5PM1.h`，以避免 `i2c_config_t` 头文件冲突。

### i2c_bus 模式限制

- 只要项目中存在 `M5Unified` 或 `M5GFX`，就不应使用 `i2c_bus` 模式。
- `i2c_bus_handle_t` 初始化入口仅在兼容条件满足时可用；若不可用，对应重载会返回 `M5PM1_ERR_NOT_SUPPORTED`。

### 高危接口

- `setDownloadLock`、`setSingleResetDisable`、`setDoubleOffDisable` 可能导致下载模式、单击复位或双击关断不可用。
- 如果误配置后设备无法进入预期状态，通常需要断电后重新上电恢复；对带电池设备请谨慎操作。

## 引脚与功能限制

| 功能区域 | 说明 | 备注 |
| :--- | :--- | :--- |
| **WAKE 互斥** | GPIO0 / 2 互斥；GPIO3 / 4 互斥 | 仅 WAKE 功能生效 |
| **唤醒 (WAKE)** | GPIO0 / 2 / 3 / 4 支持唤醒 | GPIO1 不支持 WAKE |
| **ADC 通道** | GPIO1 = ADC1，GPIO2 = ADC2 | 温度为内部通道 |
| **PWM 通道** | GPIO3 = PWM0，GPIO4 = PWM1 | 频率全通道共享 |
| **NeoPixel** | 仅 GPIO0 支持；LED 数量 1-32；数据区 0x60 - 0x9F | 刷新时 I2C 短暂不可中断 |
| **I2C 休眠** | 空闲超时后可进入低功耗 | PWM 使能或下载模式下休眠失效 |
| **寄存器区块** | 0x00 - 0x0C，0x10 - 0x19，0x20 - 0x2A，0x30 - 0x35，0x38 - 0x3D，0x40 - 0x45，0x48 - 0x4A，0x50，0x53，0x60 - 0x9F，0xA0 - 0xBF | 支持连续读写 |

## 版本与依赖

- **版本**: 1.0.6
- **平台**: Arduino / ESP-IDF
- **依赖**: 默认无强制外部库依赖；ESP-IDF 下的 `i2c_bus_handle_t` 路径仅在兼容配置满足时可用。

