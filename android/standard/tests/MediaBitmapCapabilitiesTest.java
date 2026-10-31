package com.example.motonav;

/** Legacy-board safety and reconnect isolation for newly restored optional transports. */
public final class MediaBitmapCapabilitiesTest {
    static void check(boolean value) { if(!value) throw new AssertionError(); }
    public static void main(String[] args) {
        MediaBitmapCapabilities c=new MediaBitmapCapabilities();
        check(!c.supportsCover() && !c.supportsCross());
        for(String legacy:new String[]{"U,1","CAP,BLD,1","CAP,ZMAP,1","CAP,HID,1","CAP,LYR,1"})
            check(!c.accept(legacy,true) && !c.supportsCover() && !c.supportsCross());
        check(!c.accept("CAP,COVER,1",false) && !c.supportsCover());
        check(c.accept("CAP,COVER,1",true) && c.supportsCover() && !c.supportsCross());
        check(!c.accept("CAP,COVER,1",true));
        check(c.accept("CAP,CROSS,1",true) && c.supportsCross());
        c.reset(); check(!c.supportsCover() && !c.supportsCross());
        check(!c.accept("CAP,COVER,2",true) && !c.supportsCover());
        check(!c.accept("CAP,CROSS,1",false) && !c.supportsCross());
        System.out.println("PASS: optional bitmap channels require explicit authenticated capabilities; legacy and reconnect isolation");
    }
}
