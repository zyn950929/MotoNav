package com.yuminaga.motonav;

/** Optional image channels are independent of maps, HID and link authentication. */
final class MediaBitmapCapabilities {
    private volatile boolean cover, cross;
    static boolean isMessage(String value) {
        return "CAP,COVER,1".equals(value) || "CAP,CROSS,1".equals(value);
    }
    boolean accept(String value, boolean authenticated) {
        if (!authenticated) return false;
        if ("CAP,COVER,1".equals(value)) { boolean changed=!cover; cover=true; return changed; }
        if ("CAP,CROSS,1".equals(value)) { boolean changed=!cross; cross=true; return changed; }
        return false;
    }
    boolean supportsCover() { return cover; }
    boolean supportsCross() { return cross; }
    void reset() { cover=false; cross=false; }
}
