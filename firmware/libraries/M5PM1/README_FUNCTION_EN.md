# M5PM1 Function Overview (English)

Feature overview of the PM1 power management driver library for Arduino and ESP-IDF.

- **Version**: 1.0.6
- **Default I2C address**: `0x6E`
- **I2C speed**: Default 100 kHz, supports switching to 400 kHz

## Project Structure and Roles

- `src/M5PM1.h`: Public APIs, registers/enums, cache structure definitions.
- `src/M5PM1.cpp`: Driver implementation, conflict checks, snapshot and cache logic.
- `src/M5PM1_i2c_compat.h`: Arduino / ESP-IDF / M5Unified I2C compatibility wrapper.

## Examples

- `examples/basic_power_adc/basic_power_adc.ino`: Initialization, device info, VBAT/VIN/5V readings, ADC/temperature sampling, and power rail control.
- `examples/gpio_pwm/gpio_pwm.ino`: GPIO output/input reads and PWM breathing LED demo.
- `examples/usb_interrupt_sleep/usb_interrupt_sleep.ino`: USB plug/unplug interrupt, timer wake, and shutdown demo.
- `examples/neopixel/neopixel.ino`: NeoPixel rainbow demo.

## Feature Categories

### 1) Initialization and Logging

- **Initialization**
  - Arduino: `begin(TwoWire*, addr, sda, scl, speed)`
  - Arduino / ESP-IDF (when M5Unified is detected): `begin(m5::I2C_Class*, addr, speed)`
  - ESP-IDF: `begin(i2c_port_t, addr, sda, scl, speed)`
  - ESP-IDF (IDF >= 5.3): `begin(i2c_master_bus_handle_t, addr, speed)`
  - ESP-IDF (when compatibility mode is available): `begin(i2c_bus_handle_t, addr, speed)`
- **Logging**
  - `setLogLevel` / `getLogLevel`

### 2) Device Information

- `getDeviceId` / `getDeviceModel` / `getHwVersion` / `getSwVersion`

### 3) GPIO Basics and Advanced Control

- **Arduino style**
  - `pinMode` / `digitalWrite` / `digitalRead`
- **With return status**
  - `pinModeWithRes` / `digitalWriteWithRes` / `digitalReadWithRes`
- **Advanced configuration**
  - `gpioSet` / `gpioSetFunc` / `gpioSetMode` / `gpioSetOutput` / `gpioGetInput`
  - `gpioSetPull` / `gpioSetDrive` / `ledEnSetDrive`
  - `gpioSetWakeEnable` / `gpioSetWakeEdge`
- **Pin status and validation**
  - `dumpPinStatus` / `verifyPinConfig` / `getPinStatus` / `getPinStatusArray`

### 4) Power Hold

- `gpioSetPowerHold` / `gpioGetPowerHold`
- `ldoSetPowerHold` / `ldoGetPowerHold`
- `boostSetPowerHold` / `boostGetPowerHold`

### 5) ADC and Temperature

- `analogRead` / `isAdcBusy` / `disableAdc` / `readTemperature`

### 6) PWM

- `setPwmFrequency` / `getPwmFrequency`
- `setPwmDuty` / `getPwmDuty` (0-100%)
- `setPwmDuty12bit` / `getPwmDuty12bit` (0-4095)
- `setPwmConfig`
- `analogWrite` (0-255)

### 7) Voltage Reading

- `readVref` / `getRefVoltage` / `readVbat` / `readVin` / `read5VInOut`

### 8) Power Management and Battery

- `getPowerSource`
- `getWakeSource` / `clearWakeSource`
- `setPowerConfig` / `getPowerConfig` / `clearPowerConfig`
- `setChargeEnable` / `setDcdcEnable` / `setLdoEnable` / `setBoostEnable` / `setLedEnLevel`
- `setBatteryLvp`

### 9) Watchdog and Timer

- `wdtSet` / `wdtFeed` / `wdtGetCount`
- `timerSet` / `timerClear`

### 10) Button

- `btnSetConfig` / `btnGetState` / `btnGetFlag`
- `setSingleResetDisable` (high risk) / `getSingleResetDisable`
- `setDoubleOffDisable` (high risk) / `getDoubleOffDisable`

### 11) Interrupts

- **Status read and clear**
  - `irqGetGpioStatus` / `irqClearGpioAll`
  - `irqGetSysStatus` / `irqClearSysAll`
  - `irqGetBtnStatus` / `irqClearBtnAll`
- **Enum-style read**
  - `irqGetGpioStatusEnum` / `irqGetSysStatusEnum` / `irqGetBtnStatusEnum`
- **Interrupt masks**
  - GPIO: `irqSetGpioMask(m5pm1_irq_gpio_t, m5pm1_irq_mask_ctrl_t)` / `irqGetGpioMask` / `irqSetGpioMaskAll(m5pm1_irq_mask_ctrl_t)` / `irqGetGpioMaskBits`
  - System: `irqSetSysMask(m5pm1_irq_sys_t, m5pm1_irq_mask_ctrl_t)` / `irqGetSysMask` / `irqSetSysMaskAll(m5pm1_irq_mask_ctrl_t)` / `irqGetSysMaskBits`
  - Button: `irqSetBtnMask(m5pm1_irq_btn_t, m5pm1_irq_mask_ctrl_t)` / `irqGetBtnMask` / `irqSetBtnMaskAll(m5pm1_irq_mask_ctrl_t)` / `irqGetBtnMaskBits`
  - Note: Single-item setters do not support the `ALL` / `NONE` enum values. Use `xxxMaskAll()` for batch operations.

### 12) System Commands

- `sysCmd` / `shutdown` / `reboot` / `enterDownloadMode`
- `setDownloadLock` (high risk) / `getDownloadLock`

### 13) NeoPixel

- `setLeds` / `setLedCount` / `setLedColor` / `refreshLeds` / `disableLeds`

### 14) AW8737A Pulse Control

- **Pulse mode**
  - `setAw8737aPulse(pin, pulseNum, refresh)`: Set pulse count (0-3)
  - `refreshAw8737aPulse`: Trigger pulse refresh
- **Gain mode**
  - `setAw8737aMode(pin, mode, refresh)`: Set gain mode (MODE_1 to MODE_4)
  - `refreshAw8737aMode`: Trigger mode refresh
- **Notes**
  - If the pin is not already an output, it will be auto-configured as push-pull output.
  - If open-drain output is used, an external pull-up is required.
  - When `refresh = NOW`, execution includes an approximately 20 ms delay.

### 15) RTC RAM

- `writeRtcRAM` / `readRtcRAM`

### 16) I2C Configuration and Auto Wake

- `setI2cConfig` / `switchI2cSpeed` / `getI2cSpeed`
- `setI2cSleepTime` / `getI2cSleepTime`
- `setAutoWakeEnable` / `isAutoWakeEnabled` / `sendWakeSignal`
- Compatibility layer: `M5PM1_i2c_compat.h`

### 17) State Snapshot / Cache / Validation

- `setAutoSnapshot` / `isAutoSnapshotEnabled` / `updateSnapshot` / `verifySnapshot`
- `validateConfig`
- `getCachedPwmFrequency` / `getCachedPwmState` / `getCachedAdcState`
- `getCachedPowerConfig` / `getCachedButtonConfig`
- `getCachedIrqMasks` / `getCachedIrqStatus`

## Compatibility and Usage Notes

### Sharing I2C with M5Unified / M5GFX

- If the project uses `M5Unified` or `M5GFX`, prefer `begin(&M5.In_I2C, addr, speed)`.
- In this path, `M5PM1` only borrows the existing I2C handle and does not manage driver install or teardown.
- In ESP-IDF projects that include `M5Unified`, include `M5Unified.h` before `M5PM1.h` to avoid `i2c_config_t` header conflicts.

### i2c_bus mode limits

- `i2c_bus` mode should not be used when `M5Unified` or `M5GFX` is present in the project.
- The `i2c_bus_handle_t` initialization overload is only usable when compatibility conditions are met; otherwise that overload returns `M5PM1_ERR_NOT_SUPPORTED`.

### High-risk APIs

- `setDownloadLock`, `setSingleResetDisable`, and `setDoubleOffDisable` can make download mode, single-click reset, or double-click shutdown unavailable.
- If the device becomes unreachable after a bad configuration, a full power cycle is usually required; be careful on battery-powered hardware.

## Pin and Function Limits

| Area | Description | Notes |
| :--- | :--- | :--- |
| **WAKE mutual exclusion** | GPIO0 / 2 are mutually exclusive; GPIO3 / 4 are mutually exclusive | Applies only to WAKE function |
| **Wake (WAKE)** | GPIO0 / 2 / 3 / 4 support wake | GPIO1 does not support WAKE |
| **ADC channels** | GPIO1 = ADC1, GPIO2 = ADC2 | Temperature is an internal channel |
| **PWM channels** | GPIO3 = PWM0, GPIO4 = PWM1 | Frequency is shared across channels |
| **NeoPixel** | GPIO0 only; LED count 1-32; data area 0x60 - 0x9F | I2C is briefly non-interruptible during refresh |
| **I2C sleep** | Can enter low power after idle timeout | Sleep is ineffective when PWM is enabled or in download mode |
| **Register blocks** | 0x00 - 0x0C, 0x10 - 0x19, 0x20 - 0x2A, 0x30 - 0x35, 0x38 - 0x3D, 0x40 - 0x45, 0x48 - 0x4A, 0x50, 0x53, 0x60 - 0x9F, 0xA0 - 0xBF | Supports continuous read/write |

## Version and Dependencies

- **Version**: 1.0.6
- **Platforms**: Arduino / ESP-IDF
- **Dependencies**: No mandatory external library dependency by default; the `i2c_bus_handle_t` path on ESP-IDF is only available when compatibility configuration is satisfied.
