# M5IOE1

## Overview

**SKU:N/A**

M5IOE1 is a versatile I2C-based I/O expansion library designed for M5Stack devices, compatible with both Arduino and ESP-IDF frameworks. It significantly extends system capabilities by providing additional digital I/Os with interrupt support, 12-bit ADC channels, PWM outputs, and a built-in NeoPixel LED driver.

## Related Link

- N/A

## Required Libraries:

- N/A

## Notes

### Include Order (ESP-IDF)

If used alongside `M5Unified`, include it **before** `M5IOE1`:

```cpp
#include <M5Unified.h>   // ✓ must come first
#include <M5IOE1.h>
```

```cpp
#include <M5IOE1.h>       // ✗ causes i2c_config_t conflict
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

- [M5IOE1 - MIT](LICENSE)
