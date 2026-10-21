#pragma once
#include "AppleMediaData.h"
#include "AppleMediaCommands.h"
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include <host/ble_uuid.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// GATT client on the existing encrypted peripheral link. No scan, extra connection,
// blocking wait or change to the navigation/HID services. Optional Remote Command
// writes provide a fallback only when the caller selects it instead of HID.
class AppleMediaClient {
public:
  static constexpr size_t LIMIT = 512;
  struct Value { String text; uint16_t revision = 1; bool partial = false; };
  Value values[7]; // title, artist, album, duration, playback, player, service status
  bool available = false, changed = false;
  volatile uint32_t generation = 0;
  AppleMediaData::Playback playback;
  uint32_t playbackAt = 0;
  void begin() { events = xQueueCreate(24, sizeof(Event)); owner() = this; BLEDevice::setCustomGapHandler(gap); }
  void tick(bool enabled, uint16_t handle, uint32_t session, uint32_t now) {
    if (!enabled || handle == 0xffff) {
      if (connected) { connected = false; reset(now); }
      return;
    }
    if (!connected || conn != handle || session != linkSession) {
      connected = true; conn = handle; linkSession = session; reset(now); retryAt = now + 1000;
    }
    Event e;
    while (events && xQueueReceive(events, &e, 0) == pdTRUE) {
      if (e.generation != generation || e.conn != conn) continue;
      if (e.kind == Notify) {
        if (e.handle == serviceChanged && e.size == 4) { reset(now); retryAt = now + 500; continue; }
        if (e.handle == remote) { commands.supported(e.data, e.size); continue; }
        if (e.handle == update && e.size >= 3) {
          int f = AppleMediaData::field(e.data[0], e.data[1]);
          if (f >= 0) { set(f, e.data + 3, e.size - 3, (e.data[2] & 1) || e.overflow, now); if (values[f].partial && attribute) reads |= 1 << f; }
        }
        continue;
      }
      if (e.kind == Service) { if (e.tag == 1) { first = e.handle; last = e.other; } else if (e.tag == 2) { gattFirst = e.handle; gattLast = e.other; } }
      else if (e.kind == Characteristic) {
        if (update && e.other > update) updateEnd = std::min<uint16_t>(updateEnd, e.other - 1);
        if (remote && e.other > remote) remoteEnd = std::min<uint16_t>(remoteEnd, e.other - 1);
        if (serviceChanged && e.other > serviceChanged) changedEnd = std::min<uint16_t>(changedEnd, e.other - 1);
        if (e.tag == 1) { update = e.handle; updateEnd = last; }
        else if (e.tag == 2) attribute = e.handle;
        else if (e.tag == 4) { remote = e.handle; remoteEnd = last; }
        else if (e.tag == 3) { serviceChanged = e.handle; changedEnd = gattLast; }
      }
      else if (e.kind == Descriptor) { if (e.other == update) updateCCCD = e.handle; else if (e.other == serviceChanged) changedCCCD = e.handle; else if (e.other == remote) remoteCCCD = e.handle; }
      else if (e.kind == Data && step == Read && e.field == reading && e.revision == values[reading].revision) {
        if (e.other != readSize) { readOverflow = true; continue; }
        size_t count = std::min<size_t>(e.size, LIMIT - readSize);
        memcpy(readBuffer + readSize, e.data, count); readSize += count;
        readOverflow |= e.overflow || count < e.size;
      }
      else if (e.kind == Done) {
        if (!busy) continue;
        busy = false;
        if (e.status && e.status != BLE_HS_EDONE) {
          // Absent attributes are legal. Other metadata errors leave HID operational.
          if (step == Command) { controlAck = -1; step = Ready; }
          else if (step == ChangedSub) { step = remoteCCCD ? RemoteSub : UpdateSub; }
          else if (step == RemoteSub || step == RemoteDescs) { step = step == RemoteSub ? UpdateSub : (gattFirst ? GattChars : UpdateSub); } else if ((step == Select || step == Read) && (e.status == BLE_HS_ATT_ERR(0xa0) || e.status == BLE_HS_ATT_ERR(0x01) || e.status == BLE_HS_ENOTCONN)) {
            reset(now); retryAt = now + 20000;
          } else if (step == Select || step == Read) {
            if (e.status == BLE_HS_ATT_ERR(0xa2) && e.revision == values[reading].revision) set(reading, nullptr, 0, false, now);
            step = Ready;
          } else { reset(now); retryAt = now + 20000; }
          continue;
        }
        advance(now, e.revision);
      }
    }
    if (dropped) { dropped = false; reset(now); retryAt = now + 20000; }
    // Retire delayed callbacks by generation; the BLE stack owns outstanding contexts.
    if (busy && now - startedAt > 20000) { if (step == Command) controlAck = -1; reset(now); retryAt = now + 20000; }
    if (busy || (int32_t)(now - retryAt) < 0) return;
    start(now);
  }
  int controlAck = 0; // 1=AMS write accepted, -1=failed; never implies playback changed.
  bool controlReady(int button) const { return connected && available && remote && commands.supports(button); }
  bool control(int button) { return controlReady(button) && commands.enqueue(button); }
  int controlMask() const { int mask = 0; for (int i = 0; i < 3; ++i) if (controlReady(i)) mask |= 1 << i; return mask; }
  void resend() { pending = 0x7f; wireField = -1; }
  bool nextFrame(uint8_t *out, size_t &size, uint32_t now) {
    if (!connected || !pending || now - sentAt < 20) return false;
    size_t payload = std::min<size_t>(128, std::max<int>(1, ble_att_mtu(conn) - 3 - 11));
    if (wireField < 0) {
      for (int f = 6; f >= 0; --f) if (pending & (1 << f)) { wireField = f; break; }
      wire = values[wireField].text; wireRevision = values[wireField].revision; wirePartial = values[wireField].partial;
      part = 0; parts = std::max<size_t>(1, (wire.length() + payload - 1) / payload); wirePayload = payload;
    }
    out[0] = 0xa6;
    for (int i = 0; i < 4; ++i) out[1+i] = generation >> (i * 8);
    out[5] = wireRevision; out[6] = wireRevision >> 8; out[7] = wireField;
    out[8] = part; out[9] = parts; out[10] = wirePartial ? 1 : 0;
    size_t offset = part * wirePayload, n = std::min<size_t>(wirePayload, wire.length() - offset);
    memcpy(out + 11, wire.c_str() + offset, n); size = n + 11; sentAt = now;
    if (++part >= parts) { if (values[wireField].revision == wireRevision) pending &= ~(1 << wireField); wireField = -1; }
    return true;
  }
private:
  enum Kind : uint8_t { Service, Characteristic, Descriptor, Data, Done, Notify };
  enum Step : uint8_t { Idle, Services, Chars, Descs, RemoteDescs, GattChars, GattDescs, ChangedSub, RemoteSub, UpdateSub, PlayerSub, TrackSub, Ready, Select, Read, Command };
  struct Context { AppleMediaClient *self; uint32_t generation; uint16_t conn, revision; uint8_t field; };
  struct Event {
    uint32_t generation; uint16_t conn, handle, other, status, size, revision;
    uint8_t kind, tag, field; bool overflow; uint8_t data[LIMIT];
  };
  QueueHandle_t events = nullptr;
  volatile bool dropped = false;
  volatile bool connected = false; bool busy = false;
  volatile uint16_t conn = 0xffff; uint16_t first = 0, last = 0, gattFirst = 0, gattLast = 0;
  AppleMediaCommands::Queue commands; uint8_t command = 0;
  volatile uint16_t update = 0, serviceChanged = 0, remote = 0; uint16_t attribute = 0, updateCCCD = 0, changedCCCD = 0, updateEnd = 0, changedEnd = 0, remoteEnd = 0, remoteCCCD = 0;
  uint32_t linkSession = 0, startedAt = 0, retryAt = 0, sentAt = 0;
  Step step = Idle;
  uint8_t reads = 0, reading = 0, pending = 0x7f;
  uint8_t readBuffer[LIMIT]; size_t readSize = 0; bool readOverflow = false;
  int wireField = -1; String wire; uint16_t wireRevision; bool wirePartial; size_t wirePayload, part = 0, parts = 0;
  static AppleMediaClient *&owner() { static AppleMediaClient *p = nullptr; return p; }
  static bool uuid(const ble_uuid_any_t &u, const char *text) { ble_uuid_any_t expected; ble_uuid_from_str(&expected, text); return ble_uuid_cmp(&u.u, &expected.u) == 0; }
  static Event event(Context *c, Kind kind) { Event e{}; e.generation = c->generation; e.conn = c->conn; e.kind = kind; e.field = c->field; e.revision = c->revision; return e; }
  void push(const Event &e) { if (events && xQueueSend(events, &e, 0) != pdTRUE) dropped = true; }
  static void done(Context *c, int status) { Event e = event(c, Done); e.status = status; c->self->push(e); delete c; }
  static int services(uint16_t, const ble_gatt_error *error, const ble_gatt_svc *s, void *arg) {
    auto c = static_cast<Context *>(arg); if (error->status || !s) { done(c, error->status); return 0; }
    Event e = event(c, Service); e.handle = s->start_handle; e.other = s->end_handle;
    if (uuid(s->uuid, "89D3502B-0F36-433A-8EF4-C502AD55F8DC")) e.tag = 1;
    else if (ble_uuid_u16(&s->uuid.u) == 0x1801) e.tag = 2;
    if (e.tag) c->self->push(e); return 0;
  }
  static int chars(uint16_t, const ble_gatt_error *error, const ble_gatt_chr *ch, void *arg) {
    auto c = static_cast<Context *>(arg); if (error->status || !ch) { done(c, error->status); return 0; }
    Event e = event(c, Characteristic); e.handle = ch->val_handle; e.other = ch->def_handle;
    if (uuid(ch->uuid, "2F7CABCE-808D-411F-9A0C-BB92BA96C102") && (ch->properties & BLE_GATT_CHR_PROP_NOTIFY) && (ch->properties & BLE_GATT_CHR_PROP_WRITE)) e.tag = 1;
    else if (uuid(ch->uuid, "C6B2F38C-23AB-46D8-A6AB-A3A870BBD5D7") && (ch->properties & BLE_GATT_CHR_PROP_READ) && (ch->properties & BLE_GATT_CHR_PROP_WRITE)) e.tag = 2;
    else if (uuid(ch->uuid, "9B3C81D8-57B1-4A8A-B8DF-0E56F7CA51C2") && (ch->properties & BLE_GATT_CHR_PROP_WRITE)) e.tag = 4;
    else if (ble_uuid_u16(&ch->uuid.u) == 0x2a05 && (ch->properties & BLE_GATT_CHR_PROP_INDICATE)) e.tag = 3;
    c->self->push(e); return 0;
  }
  static int descs(uint16_t, const ble_gatt_error *error, uint16_t ch, const ble_gatt_dsc *d, void *arg) {
    auto c = static_cast<Context *>(arg); if (error->status || !d) { done(c, error->status); return 0; }
    if (ble_uuid_u16(&d->uuid.u) == 0x2902) { Event e = event(c, Descriptor); e.handle = d->handle; e.other = ch; c->self->push(e); } return 0;
  }
  static int written(uint16_t, const ble_gatt_error *error, ble_gatt_attr *, void *arg) { done(static_cast<Context *>(arg), error->status); return 0; }
  static int read(uint16_t, const ble_gatt_error *error, ble_gatt_attr *a, void *arg) {
    auto c = static_cast<Context *>(arg); if (error->status || !a) { done(c, error->status); return 0; }
    Event e = event(c, Data); size_t n = OS_MBUF_PKTLEN(a->om); e.size = std::min<size_t>(n, LIMIT); e.overflow = n > LIMIT; e.other = a->offset;
    os_mbuf_copydata(a->om, 0, e.size, e.data); c->self->push(e); return 0;
  }
  static int gap(ble_gap_event *ev, void *) {
    auto self = owner(); if (!self || !self->connected || ev->type != BLE_GAP_EVENT_NOTIFY_RX || ev->notify_rx.conn_handle != self->conn) return 0;
    // No UI/String mutation in the host callback. Ignore all unrelated peer notifications.
    uint16_t h = ev->notify_rx.attr_handle;
    if (h != self->update && h != self->serviceChanged && h != self->remote) return 0;
    Event e{}; e.kind = Notify; e.generation = self->generation; e.conn = self->conn; e.handle = h;
    size_t n = OS_MBUF_PKTLEN(ev->notify_rx.om); e.size = std::min<size_t>(n, LIMIT); e.overflow = n > LIMIT;
    os_mbuf_copydata(ev->notify_rx.om, 0, e.size, e.data); self->push(e); return 0;
  }
  void set(int f, const uint8_t *data, size_t n, bool partial, uint32_t now) {
    n = std::min(n, LIMIT); size_t complete = AppleMediaData::utf8Prefix(data, n); partial |= complete < n;
    String text; if (complete) text.concat(reinterpret_cast<const char *>(data), complete);
    if (values[f].text == text && values[f].partial == partial) return;
    values[f].text = text; values[f].partial = partial; ++values[f].revision; if (!values[f].revision) ++values[f].revision;
    pending |= 1 << f; changed = true;
    if (f == 4) { playback = AppleMediaData::playback(text.c_str()); playbackAt = now; }
  }
  void reset(uint32_t now) {
    ++generation; if (!generation) ++generation;
    busy = false; step = Idle; available = false; reads = 0; commands.reset(); remote = remoteEnd = remoteCCCD = 0;
    first = last = gattFirst = gattLast = update = attribute = updateCCCD = serviceChanged = changedCCCD = 0;
    for (int f = 0; f < 6; ++f) { values[f].text = ""; values[f].partial = false; ++values[f].revision; }
    values[6].text = connected ? "SEARCHING" : "OFFLINE"; ++values[6].revision;
    playback = {}; playbackAt = now; changed = true; retryAt = now; resend();
  }
  void status(const char *s, uint32_t now) { set(6, reinterpret_cast<const uint8_t *>(s), strlen(s), false, now); }
  void advance(uint32_t now, uint16_t revision) {
    switch (step) {
    case Services: step = first ? Chars : (gattFirst ? GattChars : Ready); break;
    case Chars: step = update ? Descs : (gattFirst ? GattChars : Ready); break;
    case Descs: step = remote ? RemoteDescs : (gattFirst ? GattChars : UpdateSub); break;
    case RemoteDescs: step = gattFirst ? GattChars : (remoteCCCD ? RemoteSub : UpdateSub); break;
    case GattChars: step = serviceChanged ? GattDescs : (remoteCCCD ? RemoteSub : UpdateSub); break;
    case GattDescs: step = changedCCCD ? ChangedSub : (remoteCCCD ? RemoteSub : UpdateSub); break;
    case ChangedSub: step = remoteCCCD ? RemoteSub : UpdateSub; break;
    case RemoteSub: step = UpdateSub; break;
    case Command: controlAck = 1; step = Ready; break;
    case UpdateSub: step = PlayerSub; break;
    case PlayerSub: step = TrackSub; break;
    case TrackSub: step = Ready; available = true; status("READY", now); break;
    case Select: step = Read; break;
    case Read:
      if (revision == values[reading].revision) set(reading, readBuffer, readSize, readOverflow, now);
      step = Ready; break;
    default: break;
    }
    if (step == Ready && !available) { status("UNAVAILABLE", now); retryAt = now + 20000; }
  }
  void start(uint32_t now) {
    if (step == Ready) {
      if (!available) { reset(now); }
      else if (commands.take(command)) { step = Command; }
      else if (reads) { for (reading = 0; reading < 6; ++reading) if (reads & (1 << reading)) break; reads &= ~(1 << reading); readSize = 0; readOverflow = false; step = Select; }
      else return;
    }
    if (step == UpdateSub && (!update || !updateCCCD)) { step = Ready; status("UNAVAILABLE", now); retryAt = now + 20000; return; }
    auto c = new Context{this, generation, conn, values[reading].revision, reading}; int rc = 0;
    busy = true; startedAt = now;
    const uint8_t notify[] = {1, 0}, indicate[] = {2, 0}, player[] = {0, 0, 1}, track[] = {2, 0, 1, 2, 3};
    const uint8_t select[] = {AppleMediaData::entity(reading), AppleMediaData::attribute(reading)};
    switch (step) {
    case Idle: step = Services; rc = ble_gattc_disc_all_svcs(conn, services, c); break;
    case Chars: rc = ble_gattc_disc_all_chrs(conn, first, last, chars, c); break;
    case Descs: rc = ble_gattc_disc_all_dscs(conn, update, updateEnd, descs, c); break;
    case RemoteDescs: rc = ble_gattc_disc_all_dscs(conn, remote, remoteEnd, descs, c); break;
    case RemoteSub: rc = ble_gattc_write_flat(conn, remoteCCCD, notify, sizeof(notify), written, c); break;
    case Command: rc = ble_gattc_write_flat(conn, remote, &command, 1, written, c); break;
    case GattChars: rc = ble_gattc_disc_all_chrs(conn, gattFirst, gattLast, chars, c); break;
    case GattDescs: rc = ble_gattc_disc_all_dscs(conn, serviceChanged, changedEnd, descs, c); break;
    case ChangedSub: rc = ble_gattc_write_flat(conn, changedCCCD, indicate, sizeof(indicate), written, c); break;
    case UpdateSub: rc = ble_gattc_write_flat(conn, updateCCCD, notify, sizeof(notify), written, c); break;
    case PlayerSub: rc = ble_gattc_write_flat(conn, update, player, sizeof(player), written, c); break;
    case TrackSub: rc = ble_gattc_write_flat(conn, update, track, sizeof(track), written, c); break;
    case Select: rc = ble_gattc_write_flat(conn, attribute, select, sizeof(select), written, c); break;
    case Read: rc = ble_gattc_read_long(conn, attribute, 0, read, c); break;
    default: delete c; busy = false; return;
    }
    if (rc) {
      delete c; busy = false;
      if (step == Command) { controlAck = -1; step = Ready; } // no ambiguous write retry
      else if (step == RemoteSub) { step = UpdateSub; }
      else { reset(now); retryAt = now + 20000; }
    }
  }
};
