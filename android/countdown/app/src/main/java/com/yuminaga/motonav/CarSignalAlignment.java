package com.yuminaga.motonav;

/** Independent evidence checks; a matching arrow alone never identifies a junction. */
public final class CarSignalAlignment {
    private CarSignalAlignment(){}
    public static boolean coordinate(double lat,double lng){
        return Double.isFinite(lat)&&Double.isFinite(lng)&&lat>=-90&&lat<=90&&lng>=-180&&lng<=180&&
            !(lat==0&&lng==0);
    }
    public static double metres(double a,double b,double c,double d){
        if(!coordinate(a,b)||!coordinate(c,d))return Double.POSITIVE_INFINITY;
        double x=Math.toRadians(c-a),y=Math.toRadians(d-b);
        double h=Math.sin(x/2)*Math.sin(x/2)+Math.cos(Math.toRadians(a))*Math.cos(Math.toRadians(c))*Math.sin(y/2)*Math.sin(y/2);
        return 12742000*Math.asin(Math.sqrt(Math.min(1,h)));
    }
    public static String turn(int icon){
        switch(icon){case 2:return "L";case 3:return "R";case 4:return "SL";case 5:return "SR";case 9:return "S";default:return "";}
    }
    public static boolean guideAgreement(String road,String next,int icon,int segment,int remaining,
        String sdkRoad,String sdkNext,String sdkTurn,int sdkSegment,int sdkRemaining){
        return guideDisagreement(road,next,icon,segment,remaining,sdkRoad,sdkNext,sdkTurn,sdkSegment,sdkRemaining).isEmpty();
    }
    /** Only after an independently GPS-proven stop at the same previously matched node. */
    public static boolean stoppedGuideAgreement(String road,String next,int icon,int segment,int remaining,
        String sdkRoad,String sdkNext,String sdkTurn,int sdkSegment,int sdkRemaining){
        String reason=guideDisagreement(road,next,icon,segment,remaining,sdkRoad,sdkNext,sdkTurn,sdkSegment,sdkRemaining);
        return (reason.isEmpty()||reason.equals("segment-distance")) && segment>=0 && sdkSegment>=0 &&
            remaining>0&&sdkRemaining>0&&Math.abs((long)segment-sdkSegment)<=100&&Math.abs((long)remaining-sdkRemaining)<=100;
    }
    public static String guideDisagreement(String road,String next,int icon,int segment,int remaining,
        String sdkRoad,String sdkNext,String sdkTurn,int sdkSegment,int sdkRemaining){
        if(road==null||next==null||sdkRoad==null||sdkNext==null)return "missing-roads";
        road=road.trim();next=next.trim();sdkRoad=sdkRoad.trim();sdkNext=sdkNext.trim();
        if(road.isEmpty()||next.isEmpty()||road.equals("无名道路")||next.equals("无名道路"))return "unnamed-roads";
        if(!road.equals(sdkRoad))return "current-road";
        if(!next.equals(sdkNext))return "next-road";
        if(turn(icon).isEmpty())return "unsupported-icon";
        if(!turn(icon).equals(sdkTurn))return "maneuver";
        if(segment<0||sdkSegment<0||remaining<=0||sdkRemaining<=0)return "missing-distances";
        if(Math.abs((long)segment-sdkSegment)>35)return "segment-distance";
        if(Math.abs((long)remaining-sdkRemaining)>100)return "route-distance";
        return "";
    }
    /** Translate the SDK light's offset from its next maneuver to the car's
     * distance reference. The caller must independently establish alignment. */
    public static int lightInCarReference(int light,int sdkSegment,int carSegment){
        if(light<0||sdkSegment<0||carSegment<0)return -1;
        long translated=(long)light+carSegment-sdkSegment;
        return translated<0||translated>Integer.MAX_VALUE ? -1 : (int)translated;
    }
    /** Caller has already matched the junction/route. A proven stationary hold
     * also checks fresh GPS, unchanged road pair, maneuver and route distances.
     * Near a stop line, one engine can predict past the marker: a negative
     * translated offset must not revoke that independently established match.
     * This authorizes a fresh upstream snapshot, never a cached phase.
     */
    public static boolean movementAgreement(int wanted,int light,int sdkSegment,int carSegment,
                                            String carTurn,boolean stoppedMatched){
        if((wanted!=1&&wanted!=2&&wanted!=4)||light<0||light>300||sdkSegment<0||carSegment<0)return false;
        if(stoppedMatched)return true;
        int translated=lightInCarReference(light,sdkSegment,carSegment);
        return wanted==TrafficSignalPolicy.desiredMovementAtOffset(carTurn,carSegment,translated);
    }
    public static boolean matches(boolean requested,boolean sdkSimulation,int carType,long requestedAt,
        long carAt,long now,double expectedLat,double expectedLng,double endLat,double endLng,
        double sdkLat,double sdkLng,double carLat,double carLng){
        return requested && carAt>=requestedAt && now>=carAt && now-carAt<3000 &&
            carType==(sdkSimulation?1:0) &&
            metres(expectedLat,expectedLng,endLat,endLng)<=100 &&
            metres(sdkLat,sdkLng,carLat,carLng)<=65;
    }
}
