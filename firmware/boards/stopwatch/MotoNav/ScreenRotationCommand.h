#pragma once
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>

namespace ScreenRotationCommand {
inline int normalize(int degrees) { return ((degrees % 360) + 360) % 360; }
inline bool integer(const char *&cursor, int &value) {
  // Reject malformed or overflowing commands instead of unexpectedly rotating to 0°.
  if (!cursor || !*cursor || (*cursor != '-' && *cursor != '+' && (*cursor < '0' || *cursor > '9'))) return false;
  char *end; errno = 0; long parsed = strtol(cursor, &end, 10);
  if (end == cursor || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX) return false;
  value = static_cast<int>(parsed); cursor = end; return true;
}
inline bool parse(const char *command, int &degrees) {
  if (!command) return false;
  if (strncmp(command, "#R,", 3) == 0) {
    const char *cursor = command + 3; int quarter;
    if (!integer(cursor, quarter) || quarter < 0 || quarter > 3) return false;
    int exact = quarter * 90;
    if (*cursor == ',') { ++cursor; if (!integer(cursor, exact)) return false; }
    if (*cursor) return false;
    degrees = normalize(exact); return true;
  }
  if (strncmp(command, "#D", 2) == 0) {
    const char *cursor = command + 2; int exact;
    if (!integer(cursor, exact) || *cursor) return false;
    degrees = normalize(exact); return true;
  }
  return false;
}
}
