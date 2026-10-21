# M5PM1

## Overview

**SKU:N/A**

M5PM1 is a dual-platform (ESP-IDF & Arduino) driver library for M5Stack PM1 Power Management IC. It provides comprehensive power management features including battery charging and monitoring, multiple power rails (DCDC 5V, LDO 3.3V), 5 GPIO pins with various functions (GPIO/IRQ/WAKE/PWM/ADC), built-in NeoPixel LED driver, Watchdog timer, RTC RAM, and I2C auto-sleep/wake feature.

## Related Link

- [README_FUNCTION_CN](README_FUNCTION_CN.md)
- [README_FUNCTION_EN](README_FUNCTION_EN.md)

## Required Libraries:

- N/A

## Examples

- examples/basic_power_adc/basic_power_adc.ino
- examples/gpio_pwm/gpio_pwm.ino
- examples/neopixel/neopixel.ino
- examples/usb_interrupt_sleep/usb_interrupt_sleep.ino

## Notes

### Include Order (ESP-IDF)

If used alongside `M5Unified`, include it **before** `M5PM1`:

```cpp
#include <M5Unified.h>   // ✓ must come first
#include <M5PM1.h>
```

```cpp
#include <M5PM1.h>       // ✗ causes i2c_config_t conflict
#include <M5Unified.h>
```

> **Why:** On ESP-IDF ≥ 5.3.0 without `CONFIG_I2C_BUS_BACKWARD_CONFIG`, `i2c_bus.h` defines
> its own `i2c_config_t`. Including it before `M5Unified` (which pulls in `driver/i2c.h`)
> creates a conflicting declaration error. Reversing the order avoids this.
>
> Alternatively, enable `CONFIG_I2C_BUS_BACKWARD_CONFIG` in menuconfig to remove the restriction.

### i2c_bus mode is not supported when M5GFX / M5Unified is present

Use `begin(&M5.In_I2C, addr, freq)` instead — M5GFX already owns the I2C bus and the two drivers cannot share it.

## License

- [M5PM1 - MIT](LICENSE)

