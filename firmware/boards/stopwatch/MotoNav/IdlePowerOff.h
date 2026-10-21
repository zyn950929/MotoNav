#ifndef MOTONAV_IDLE_POWER_OFF_H
#define MOTONAV_IDLE_POWER_OFF_H

#include <stdint.h>
#include <stdbool.h>

/* Same timer is compiled into the firmware and the native C regression test. */
#define IDLE_POWER_OFF_MS UINT32_C(300000)

typedef struct {
  uint32_t since;
  bool armed;
} IdlePowerOffTimer;

static inline void idlePowerOffReset(IdlePowerOffTimer *timer, uint32_t now) {
  timer->since = now;
  timer->armed = true;
}

static inline bool idlePowerOffNavigationActive(bool connected, bool inNav,
                                                bool cruise, bool arrived) {
  return connected && (cruise || (inNav && !arrived));
}

static inline bool idlePowerOffDue(IdlePowerOffTimer *timer, uint32_t now,
                                   bool navigating, bool interacting) {
  if (!timer->armed || navigating || interacting) {
    idlePowerOffReset(timer, now);
    return false;
  }
  /* Unsigned subtraction also handles the approximately 49-day millis wrap. */
  return (uint32_t)(now - timer->since) >= IDLE_POWER_OFF_MS;
}

#endif
