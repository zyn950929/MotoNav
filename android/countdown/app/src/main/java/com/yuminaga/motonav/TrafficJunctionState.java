package com.yuminaga.motonav;
/** Never combine directions from different callback formats or old snapshots. */
public final class TrafficJunctionState {
    private final TrafficSignalState camera=new TrafficSignalState(),navigation=new TrafficSignalState();
    public void clear(){camera.clear();navigation.clear();}
    /** A malformed callback revokes only its source, not an independent fresh snapshot. */
    public void reject(boolean list){(list?camera:navigation).clear();}
    public boolean accept(boolean list,TrafficSignalState.Light[] lights,long now){
        return (list?camera:navigation).accept(lights,now);
    }
    public TrafficSignalPacket[] snapshots(long now){
        TrafficSignalPacket[] all=camera.snapshots(now);
        return all.length>0?all:navigation.snapshots(now);
    }
}
