#pragma once
#include <stdint.h>
#include <stddef.h>

// HID Consumer page: Previous Track, Play/Pause, Next Track. No keyboard collection.
namespace HidMediaKeys {
static constexpr uint8_t REPORT_ID = 1;
static constexpr uint16_t PREVIOUS = 0x00B6, PLAY_PAUSE = 0x00CD, NEXT = 0x00B5;
static const uint8_t REPORT_MAP[] = {
  0x05, 0x0C,        // Usage Page (Consumer)
  0x09, 0x01,        // Usage (Consumer Control)
  0xA1, 0x01,        // Collection (Application)
  0x85, REPORT_ID,   // Report ID (GATT Report Reference; not part of notification payload)
  0x15, 0x00,        // Logical Minimum (0 = released)
  0x26, 0xFF, 0x03,  // Logical Maximum (0x03FF)
  0x19, 0x00,        // Usage Minimum (0)
  0x2A, 0xFF, 0x03,  // Usage Maximum (0x03FF)
  0x75, 0x10,        // Report Size (16 bits)
  0x95, 0x01,        // Report Count (1)
  0x81, 0x00,        // Input (Data, Array, Absolute)
  0xC0
};
inline uint16_t usageForButton(int button) {
  return button == 0 ? PREVIOUS : button == 1 ? PLAY_PAUSE : button == 2 ? NEXT : 0;
}
inline void encode(uint16_t usage, uint8_t (&bytes)[2]) {
  bytes[0] = static_cast<uint8_t>(usage); bytes[1] = static_cast<uint8_t>(usage >> 8);
}
// A small nonblocking press/release queue. Only releases are retried after BLE errors;
// retrying an uncertain Play/Pause press could toggle twice. All calls on the UI loop.
class Pulses {
public:
  static constexpr size_t CAPACITY = 4;
  struct Report { uint16_t usage; bool release; };
  bool enqueue(int button) {
    uint16_t usage = usageForButton(button);
    if (!usage || count_ == CAPACITY) return false;
    queue_[(head_ + count_) % CAPACITY] = usage; ++count_; return true;
  }
  bool poll(uint32_t now, Report &report) const {
    if (waiting_ && static_cast<int32_t>(now - due_) < 0) return false;
    if (down_) { report = {0, true}; return true; }
    if (count_) { report = {queue_[head_], false}; return true; }
    return false;
  }
  void submitted(const Report &report, uint32_t now, bool success) {
    waiting_ = true;
    if (report.release) {
      if (success) { down_ = false; due_ = now + 40; }
      else { down_ = true; due_ = now + 50; }
    } else {
      if (count_) { head_ = (head_ + 1) % CAPACITY; --count_; }
      down_ = true; due_ = now + (success ? 60 : 0);
    }
  }
  void cancelQueued() { count_ = 0; head_ = 0; } // still release an in-flight key
  void reset() { cancelQueued(); down_ = false; waiting_ = false; due_ = 0; }
private:
  uint16_t queue_[CAPACITY] = {};
  size_t head_ = 0, count_ = 0;
  bool down_ = false, waiting_ = false;
  uint32_t due_ = 0;
};
}
