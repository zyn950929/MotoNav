#pragma once
// Shared code-native artwork for both logical 466px canvases (1.85B scales to 360px).
namespace DragonRadar {
static constexpr uint16_t DayBackground = 0x36C7; // RGB(48,216,56)
static constexpr uint16_t NightBackground = 0x1384;
// Keep an off-screen target near its bearing while avoiding the speed/turn HUD.
template<class Overlap> void placeEdgeBall(float &x, float &y, float center, float radius, Overlap overlap) {
  const float angle = atan2f(y - center, x - center);
  long best = 0x7fffffff;
  float bx = x, by = y;
  for (int step = 0; step <= 36; ++step) {
    for (int sign = -1; sign <= 1; sign += 2) {
      float a = angle + sign * step * 0.08726646f; // closest free position, 5 degrees at a time
      float px = center + cosf(a) * radius, py = center + sinf(a) * radius;
      long score = overlap(px - 23, py - 23, px + 23, py + 23);
      if (score < best) { best = score; bx = px; by = py; }
      if (score == 0) { x = px; y = py; return; }
    }
  }
  x = bx; y = by;
}
template<class Graphics> void drawGrid(Graphics *g, int width, int height, bool night) {
  const uint16_t minor = night ? 0x1263 : 0x25A5;
  const uint16_t major = night ? 0x0941 : 0x1383;
  const int step = 40, cx = width / 2, cy = height / 2;
  for (int k = -6; k <= 6; ++k) {
    const uint16_t color = k % 5 == 0 ? major : minor;
    int x = cx + k * step, y = cy + k * step;
    if (x >= 0 && x < width) g->drawFastVLine(x, 0, height, color);
    if (y >= 0 && y < height) g->drawFastHLine(0, y, width, color);
  }
  g->drawFastVLine(cx + 1, 0, height, major);
  g->drawFastHLine(0, cy + 1, width, major);
}
template<class Graphics> void drawStar(Graphics *g, float cx, float cy, float radius) {
  float x[10], y[10];
  for (int i = 0; i < 10; ++i) {
    float a = -1.57079633f + i * 0.62831853f;
    float r = i % 2 == 0 ? radius : radius * 0.43f;
    x[i] = cx + cosf(a) * r; y[i] = cy + sinf(a) * r;
  }
  for (int i = 0; i < 10; ++i) {
    int j = (i + 1) % 10;
    g->fillTriangle(cx, cy, x[i], y[i], x[j], y[j], 0xD8A1);
  }
}
template<class Graphics> void drawBall(Graphics *g, int x, int y) {
  g->fillCircle(x, y, 21, 0x0941);
  g->fillCircle(x, y, 19, 0xC340);
  g->fillCircle(x, y, 17, 0xFC60);
  g->fillCircle(x - 1, y - 2, 15, 0xFDC4);
  g->fillCircle(x - 7, y - 9, 3, 0xFF75);
  // Exactly four five-pointed red stars, laid out as a square.
  for (int row = 0; row < 2; ++row)
    for (int col = 0; col < 2; ++col)
      drawStar(g, x - 6 + col * 12, y - 5 + row * 11, 4.7f);
}
}
