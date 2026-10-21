#pragma once
#include <stdint.h>
#include <stddef.h>
namespace AppleMediaCommands {
// Apple's RemoteCommand IDs: Toggle=2, Next=3, Previous=4.
inline int forButton(int button) { return button == 0 ? 4 : button == 1 ? 2 : button == 2 ? 3 : -1; }
class Queue {
public:
  void reset() { head = count = 0; known = false; mask = 0; }
  void supported(const uint8_t *ids, size_t n) {
    mask = 0; known = true;
    for (size_t i = 0; i < n; ++i) if (ids[i] < 32) mask |= uint32_t(1) << ids[i];
  }
  bool supports(int button) const { int id = forButton(button); return id >= 0 && (!known || (mask & (uint32_t(1) << id))); }
  bool enqueue(int button) {
    int id = forButton(button); if (!supports(button) || count == 4) return false;
    items[(head + count) % 4] = id; ++count; return true;
  }
  bool take(uint8_t &command) {
    // Recheck the latest host capability list before sending queued commands.
    while (count) {
      command = items[head]; head = (head + 1) % 4; --count;
      if (!known || (mask & (uint32_t(1) << command))) return true;
    }
    return false;
  }
  void cancel() { head = count = 0; }
private:
  uint8_t items[4] = {}; size_t head = 0, count = 0; bool known = false; uint32_t mask = 0;
};
}
