#pragma once
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace AppleMediaData {
// Only values defined by AMS are accepted; malformed updates never imply PLAYING.
struct Playback { bool known = false; int state = 0; double rate = 0, elapsed = 0; };
inline Playback playback(const char *text) {
  Playback p; char *end;
  long state = strtol(text, &end, 10);
  if (end == text || *end++ != ',' || state < 0 || state > 3) return p;
  const char *start = end; double rate = strtod(start, &end);
  if (end == start || *end++ != ',' || !std::isfinite(rate)) return p;
  start = end; double elapsed = strtod(start, &end);
  if (end == start || *end || !std::isfinite(elapsed) || elapsed < 0) return p;
  p.known = true; p.state = state; p.rate = rate; p.elapsed = elapsed; return p;
}
inline double duration(const char *text) {
  char *end; double v = strtod(text, &end);
  return end != text && !*end && std::isfinite(v) && v >= 0 ? v : 0;
}
// Drop an incomplete UTF-8 code point when bounding a value or a long read.
inline size_t utf8Prefix(const uint8_t *p, size_t n) {
  if (!n) return 0;
  size_t start = n - 1;
  while (start && (p[start] & 0xc0) == 0x80) --start;
  unsigned lead = p[start]; size_t count = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
  return start + count <= n ? n : start;
}
inline int field(uint8_t entity, uint8_t attr) {
  if (entity == 2) { const int map[] = {1, 2, 0, 3}; return attr < 4 ? map[attr] : -1; }
  return entity == 0 && attr < 2 ? (attr == 0 ? 5 : 4) : -1;
}
inline uint8_t entity(int field) { return field < 4 ? 2 : 0; }
inline uint8_t attribute(int field) { const uint8_t map[] = {2, 0, 1, 3, 1, 0}; return map[field]; }
}
