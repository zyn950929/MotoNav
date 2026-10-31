package com.yuminaga.motonav;

/** 离线包只接收完整的 1200×1200、2 米普通地图，不把损坏缓存发往圆屏。 */
public final class RasterPacket {
    public static boolean isBuilding(byte[] b) { return b.length>=16 && u16(b,14)==(2000|AmapBuildings.FLAG); }
    /** 连接旧固件时保留完整范围，将屋顶退回地面，绝不发送它不认识的标志。 */
    public static byte[] forLegacy(byte[] b) {
        if(!isBuilding(b))return b;
        byte[] out=b.clone();out[14]=(byte)2000;out[15]=(byte)(2000>>>8);
        for(int p=16;p<out.length;) {
            int v=out[p]&255;
            if((v&15)>3)out[p]=(byte)((v&240)|(v&2));
            p+=((v>>>4)==15)?2:1;
        }
        return out;
    }
    public static boolean hasPalette(byte[] b) { return b.length>=48 && (u16(b,14)&0x8000)!=0; }
    private static int u16(byte[] b,int p) { return (b[p]&255)|((b[p+1]&255)<<8); }
    private static int i32(byte[] b,int p) { return u16(b,p)|(u16(b,p+2)<<16); }
    public static boolean valid(byte[] b,int frame,int x,int y) {
        if (b.length<17 || b.length>480000 || u16(b,0)!=frame || i32(b,2)!=x || i32(b,6)!=y ||
            u16(b,10)!=1200 || u16(b,12)!=1200 || (u16(b,14)!=2000 && u16(b,14)!=(2000|AmapBuildings.FLAG) && u16(b,14)!=(2000|0x8000))) return false;
        if((u16(b,14)&0x8000)!=0 && !hasPalette(b))return false;
        int pos=hasPalette(b)?48:16, pixels=0;
        while (pos<b.length && pixels<1440000) {
            int v=b[pos++]&255;
            if ((v&15)>(hasPalette(b)?15:isBuilding(b)?7:3)) return false;
            int run=(v>>4)+1;
            if ((v>>4)==15) { if (pos==b.length) return false; run=16+(b[pos++]&255); }
            if (run>1440000-pixels) return false;
            pixels+=run;
        }
        return pixels==1440000 && pos==b.length;
    }
}
