package com.yuminaga.motonav;

import android.app.Application;
import android.content.Intent;
import android.os.SystemClock;
import android.util.Log;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import org.json.JSONArray;
import org.json.JSONObject;

/** Car-process adapter. Android/reflection only: never initializes the MotoNav SDK here. */
public final class CarEngineTap {
    public static final String ACTION="com.motonav.fusion.CAR_GUIDANCE";
    private static final Map<String,Method> methods=new HashMap<>();
    private static long errorAt;
    private static long stateAt, positionAt;
    private static String lastEngineState="";
    private static int routeInfoSequence;
    private static long syncLease(Application app,long now){
        File f=new File(app.getFilesDir(),"car-nav-sync-enabled");
        if(!f.isFile()||f.length()>96)return 0;
        try(java.io.FileInputStream in=new java.io.FileInputStream(f)){
            byte[] b=new byte[96];int n=in.read(b);
            String[] values=new String(b,0,n,StandardCharsets.UTF_8).split("\\n");
            long token=Long.parseLong(values[0]),until=Long.parseLong(values[1]);
            return token>0&&token<=now&&until>=now&&until-now<=15000?token:0;
        }catch(Exception e){return 0;}
    }
    private static boolean single(Application app){return new File(app.getFilesDir(),"single-engine-enabled").isFile();}
    private static Application app() throws Exception {
        return (Application)Class.forName("android.app.ActivityThread").getMethod("currentApplication").invoke(null);
    }
    private static Intent event(Application app,String kind){return new Intent(ACTION).setPackage(app.getPackageName())
        .putExtra("at",SystemClock.elapsedRealtime()).putExtra("kind",kind)
        .putExtra("providerProcess",Application.getProcessName());}
    private static void emit(Application app,Intent i){app.sendBroadcast(i,"com.motonav.app.fresh.permission.CAR_GUIDANCE");}
    private static Object call(Object o,String name) throws Exception {
        String key=o.getClass().getName()+"#"+name;
        Method m=methods.get(key);
        if(m==null){m=o.getClass().getMethod(name);methods.put(key,m);}
        return m.invoke(o);
    }
    private static double number(Object o,String name) throws Exception {
        return ((Number)call(o,name)).doubleValue();
    }
    public static synchronized void guide(Object g) {
        if(g==null)return;
        try {
            Application app=(Application)Class.forName("android.app.ActivityThread")
                .getMethod("currentApplication").invoke(null);
            if(app==null)return;
            Intent i=new Intent(ACTION).setPackage(app.getPackageName());
            i.putExtra("at",SystemClock.elapsedRealtime());
            i.putExtra("type",(int)number(g,"getType"));
            i.putExtra("lat",number(g,"getCarLatitude"));
            i.putExtra("lng",number(g,"getCarLongitude"));
            i.putExtra("bearing",number(g,"getCarDirection"));
            i.putExtra("speed",number(g,"getCurSpeed"));
            i.putExtra("icon",(int)number(g,"getIcon"));
            i.putExtra("turnMetres",(int)number(g,"getSegRemainDis"));
            i.putExtra("remaining",(int)number(g,"getRouteRemainDis"));
            i.putExtra("endLat",number(g,"getEndPOILatitude"));
            i.putExtra("endLng",number(g,"getEndPOILongitude"));
            i.putExtra("road",String.valueOf(call(g,"getCurRoadName")));
            i.putExtra("nextRoad",String.valueOf(call(g,"getNextRoadName")));
            if(single(app)) {
                i.putExtra("kind","guide");
                String[][] fields={{"remainingSeconds","getRouteRemainTime"},{"total","getRouteAllDis"},
                    {"totalSeconds","getRouteAllTime"},{"segment","getCurSegNum"},{"point","getCurPointNum"},
                    {"lights","getRouteRemainTrafficLightNum"},{"cameraDistance","getCameraDist"},
                    {"cameraType","getCameraType"},{"cameraSpeed","getCameraSpeed"},{"speedLimit","getLimitedSpeed"}};
                for(String[] f:fields)try{i.putExtra(f[0],(int)number(g,f[1]));}catch(Exception ignored){}
            }
            app.sendBroadcast(i,"com.motonav.app.fresh.permission.CAR_GUIDANCE");
        } catch(Throwable e) {
            long t=SystemClock.elapsedRealtime();
            if(t-errorAt>10000){errorAt=t;Log.w("MotoNavCarTap","guide adapter: "+e.getClass().getSimpleName());}
        }
    }
    /** Runs only in the original provider process; JNI is never loaded in :motonav. */
    public static synchronized void engineState(Application app){
        if(!app.getPackageName().equals(Application.getProcessName()))return;
        long now=SystemClock.elapsedRealtime();if(now-stateAt<1000)return;
        // Terminal commands must be consumed even after MotoNav releases ownership.
        NativeRouteBridge.applySimulationCommand(app);
        boolean owner=single(app),probe=false;
        long syncToken=syncLease(app,now);
        try{
            File f=new File(app.getFilesDir(),"single-engine-probe");
            if((app.getApplicationInfo().flags&2)!=0&&f.isFile()){
                byte[] b=new byte[40];int n;
                try(java.io.FileInputStream in=new java.io.FileInputStream(f)){n=in.read(b);}
                long until=Long.parseLong(new String(b,0,n,StandardCharsets.UTF_8));
                probe=until>=now&&until-now<=120000;
            }
        }catch(Exception ignored){}
        if(!owner&&!probe&&syncToken==0)return;stateAt=now;
        try {
            Class<?> c=Class.forName("com.autonavi.amapauto.jni.AndroidDriveNavigation");
            boolean real=Boolean.TRUE.equals(c.getMethod("isInNavi").invoke(null));
            boolean sim=Boolean.TRUE.equals(c.getMethod("isInSimulateNavi").invoke(null));
            String actual=real+":"+sim;
            if(!actual.equals(lastEngineState)){
                lastEngineState=actual;
                Log.i("MotoNavCarTap","native-engine process="+Application.getProcessName()+" real="+real+" simulation="+sim);
            }
            if(syncToken>0)emit(app,event(app,"engine").putExtra("syncToken",syncToken)
                .putExtra("real",real).putExtra("simulation",sim));
            if(probe){
                JSONObject state=new JSONObject().put("at",now).put("real",real).put("simulation",sim);
                try(FileOutputStream out=new FileOutputStream(new File(app.getFilesDir(),"single-engine-probe-state.json"))){out.write(state.toString().getBytes(StandardCharsets.UTF_8));}
                if(!owner&&(real||sim))NativeRouteBridge.poll(app);
            }
            if(owner){
                emit(app,event(app,"engine").putExtra("real",real).putExtra("simulation",sim));
                if(real||sim||new File(app.getFilesDir(),"single-engine-preview-enabled").isFile())NativeRouteBridge.poll(app);else NativeRouteBridge.idle();
            }
        }catch(Throwable ignored){}
    }
    /** Forward true terminal and reroute events; turn icons never complete a trip. */
    public static synchronized void nativeStatus(int code){
        if(code!=39&&code!=305&&code!=306&&code!=307&&code!=308&&code!=9)return;
        try{
            Application app=app();if(app==null||!single(app))return;
            emit(app,event(app,"nativeStatus").putExtra("code",code));
            // A failed replan can retain the same active path ID. Re-export it too,
            // otherwise clearing the old drawing at 307 leaves the map empty.
            if(code==305||code==306)NativeRouteBridge.idle();
            Log.i("MotoNavCarTap","native-status="+code);
        }catch(Throwable e){Log.w("MotoNavCarTap","native status adapter",e);}
    }
    public static synchronized void position(Object location){
        try {
            Application app=app();if(app==null||!single(app)||location==null)return;
            long now=SystemClock.elapsedRealtime();if(now-positionAt<250)return;positionAt=now;
            Class<?> c=location.getClass();
            Intent i=event(app,"position");
            for(String[] f:new String[][]{{"lat","lat"},{"lng","lon"},{"bearing","bearing"},{"speedMps","speed"},{"accuracy","accuracy"}})
                i.putExtra(f[0],((Number)c.getField(f[1]).get(location)).doubleValue());
            emit(app,i);
        }catch(Throwable ignored){}
    }
    public static synchronized void lane(Object lane){
        try {
            Application app=app();if(app==null||!single(app))return;
            Intent i=event(app,"lane");
            if(lane!=null){
                i.putExtra("back",(int[])lane.getClass().getField("backLane").get(lane));
                i.putExtra("front",(int[])lane.getClass().getField("frontLane").get(lane));
            }
            emit(app,i);
        }catch(Throwable ignored){}
    }
    /** A possible actual-route outlet. It is not replaced by an unrelated SDK/web path. */
    public static synchronized void route(Object route){
        try {
            Application app=app();if(app==null||!single(app)||route==null)return;
            JSONArray points=new JSONArray();
            for(Object link:(List<?>)call(route,"getRouteLinkInfos")){
                for(Object p:(List<?>)call(link,"getGeoPointArrayList")){
                    if(points.length()>=20000)throw new IllegalArgumentException("route too large");
                    double lat=number(p,"getLatitudeY"),lng=number(p,"getLongitudeX");
                    if(!Double.isFinite(lat)||!Double.isFinite(lng)||Math.abs(lat)>90||Math.abs(lng)>180)throw new IllegalArgumentException("route coordinate");
                    points.put(new JSONArray().put(lat).put(lng));
                }
            }
            if(points.length()<2)return;
            String id=String.valueOf(call(route,"getPathid"));
            JSONObject data=new JSONObject().put("at",SystemClock.elapsedRealtime()).put("id",id).put("points",points);
            File temp=new File(app.getFilesDir(),"single-engine-route.tmp"),target=new File(app.getFilesDir(),"single-engine-route.json");
            try(FileOutputStream out=new FileOutputStream(temp)){out.write(data.toString().getBytes(StandardCharsets.UTF_8));}
            if(!temp.renameTo(target))throw new IllegalStateException("route commit");
            emit(app,event(app,"route").putExtra("id",id).putExtra("count",points.length()));
            Log.i("MotoNavSingleEngine","native-route points="+points.length());
        }catch(Throwable e){Log.w("MotoNavSingleEngine","native-route unavailable: "+e.getClass().getSimpleName());}
    }
    public static synchronized void routeInfo(Object value){
        try {
            Application app=app();if(app==null||!single(app)||value==null)return;
            Object raw=value.getClass().getField("jsonString").get(value);
            JSONObject data=new JSONObject().put("at",SystemClock.elapsedRealtime())
                .put("class",value.getClass().getName())
                .put("json",raw==null?JSONObject.NULL:raw)
                .put("newJson",value.getClass().getField("isNewJsonResult").get(value))
                .put("success",value.getClass().getField("isSuccessed").get(value));
            byte[] bytes=data.toString().getBytes(StandardCharsets.UTF_8);if(bytes.length>4000000)return;
            try(FileOutputStream out=new FileOutputStream(new File(app.getFilesDir(),"single-engine-route-info.json"))){out.write(bytes);}
            try(FileOutputStream out=new FileOutputStream(new File(app.getFilesDir(),"single-engine-route-info-"+(routeInfoSequence++%8)+".json"))){out.write(bytes);}
            emit(app,event(app,"routeInfo").putExtra("bytes",bytes.length));
        }catch(Throwable e){Log.w("MotoNavSingleEngine","route-info capture: "+e.getClass().getSimpleName());}
    }
    public static synchronized void response(Object value){
        try {
            Application app=app();if(app==null||!single(app)||value==null)return;
            Object raw=value.getClass().getField("jsonString").get(value);
            if(!(raw instanceof String)||((String)raw).length()>4000000)return;
            JSONObject json=new JSONObject((String)raw);
            if(json.optInt("protocolId",-1)==30005)routeInfo(value);
        }catch(Throwable ignored){}
    }
}
