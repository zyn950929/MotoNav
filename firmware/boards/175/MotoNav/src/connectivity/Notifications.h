#pragma once
// 配对、地图确认、媒体等蓝牙通知。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
void sendPendingAck() {
  if (!txChar || !bleConnected || !bleAuthed) return;    // v6.5 没通过验证的手机什么都不发
  // v6.5 告诉手机“验证通过了”：连上后 0.3 / 1.5 / 4 秒各发一次（手机订阅通知可能比加密晚一点）
  static const uint16_t uAt[3] = {300, 1500, 4000};
  if (authNotifies < 3 && millis() - bleAuthAt > uAt[authNotifies]) {
    authNotifies++;
    txChar->setValue((uint8_t *)"U,1", 3);
    txChar->notify();
    txChar->setValue((uint8_t *)"CAP,HID,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,LYR,1", 9); txChar->notify();
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    appleMedia.resend();
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    pendingMediaStatus = String("AMC,") + appleMedia.controlMask();
  }
  if (pendingLyricsStatus.length()) { txChar->setValue((uint8_t *)pendingLyricsStatus.c_str(), pendingLyricsStatus.length()); txChar->notify(); pendingLyricsStatus = ""; }
  if (pendingMediaStatus.length()) { txChar->setValue((uint8_t *)pendingMediaStatus.c_str(), pendingMediaStatus.length()); txChar->notify(); pendingMediaStatus = ""; }
  uint8_t amsFrame[139]; size_t amsSize = 0;
  if (appleMedia.nextFrame(amsFrame, amsSize, millis())) { txChar->setValue(amsFrame, amsSize); txChar->notify(); }
  if (pendingHidStatus.length()) {
    txChar->setValue((uint8_t *)pendingHidStatus.c_str(), pendingHidStatus.length());
    txChar->notify(); pendingHidStatus = "";
  }
  if (pendingBootAck.length()) {
    txChar->setValue((uint8_t *)pendingBootAck.c_str(), pendingBootAck.length());
    txChar->notify(); pendingBootAck = "";
  }
  if (pendingBuildingCapability) {
    txChar->setValue((uint8_t *)"CAP,BLD,1", 9); txChar->notify(); pendingBuildingCapability=false;
  }
  if (pendingAck.length()) {
    txChar->setValue((uint8_t *)pendingAck.c_str(), pendingAck.length());
    txChar->notify();
    pendingAck = "";
  }
  if (pendingStat2.length()) {
    txChar->setValue((uint8_t *)pendingStat2.c_str(), pendingStat2.length());
    txChar->notify();
    pendingStat2 = "";
  }
  if (pendingProf.length()) {                         // v5.8 分步用时
    txChar->setValue((uint8_t *)pendingProf.c_str(), pendingProf.length());
    txChar->notify();
    pendingProf = "";
  }
  if (pendingBat.length()) {                          // v6.8 电量
    txChar->setValue((uint8_t *)pendingBat.c_str(), pendingBat.length());
    txChar->notify();
    pendingBat = "";
  }
  if (pendingEvt.length()) {                          // v5.2 触摸事件
    txChar->setValue((uint8_t *)pendingEvt.c_str(), pendingEvt.length());
    txChar->notify();
    pendingEvt = "";
  }
}
