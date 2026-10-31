#pragma once
// 地图和路线大块数据分包接收。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// 'M' 类型 编号 总片数(2) 片序号(2) 总字节(4) 偏移(4) + 数据
void handleBulk(const uint8_t *d, int n) {
  if (n < 15) return;
  int type = d[1], id = d[2];
  int parts = d[3] | (d[4] << 8), seq = d[5] | (d[6] << 8);
  uint32_t total = (uint32_t)rd32(d + 7), off = (uint32_t)rd32(d + 11);
  if (type < 1 || type > 4) return;
  Bulk &b = bulks[type];
  if (!b.buf || total == 0 || total > b.cap || parts == 0 || parts > 4096 || seq >= parts) return;
  if (id != b.id || total != b.total || parts != b.parts) {     // 新的一份：从头开始收
    b.id = id; b.total = total; b.parts = parts; b.got = 0; b.done = false;
    memset(b.mask, 0, sizeof(b.mask));
  }
  if (b.done) { pendingAck = "K," + String(type) + "," + String(id); return; }   // 重复的：再确认一次
  uint32_t len = n - 15;
  if (off >= total) return;
  if (off + len > total) len = total - off;
  if (!(b.mask[seq >> 3] & (1 << (seq & 7)))) {
    memcpy(b.buf + off, d + 15, len);
    b.mask[seq >> 3] |= (1 << (seq & 7));
    b.got++;
  }
  if (b.got >= b.parts) {
    b.done = true;
    if (type == 1) {
      uint8_t *t = mapShow; mapShow = b.buf; b.buf = t;   // 交换：新图显示，旧缓冲留着收下一块
      parseMap(mapShow, total);
    } else if (type == 2) {
      parseRoute(b.buf, total);
    } else if (type == 4) {
      parseGhost(b.buf, total);                        // v4.4 原路线
    } else {
      if (!parseRaster(b.buf,total)) {
        // 没有成功解码就不回“已确认”，并允许相同编号完整重发。
        b.done=false; b.got=0; memset(b.mask,0,sizeof(b.mask));
        return;
      }
    }
    pendingAck = "K," + String(type) + "," + String(id);
    navDirty = true;
  }
}
