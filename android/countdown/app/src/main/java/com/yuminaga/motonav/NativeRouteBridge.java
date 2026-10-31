package com.yuminaga.motonav;

import android.app.Application;
import android.content.Intent;
import android.os.SystemClock;
import android.util.Log;
import java.io.*;
import java.security.MessageDigest;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;
import org.json.JSONObject;

/** Provider process only. Pinned path copies and an explicit selected-path simulation command. */
public final class NativeRouteBridge {
    private static final AtomicBoolean busy=new AtomicBoolean();
    private static final java.util.concurrent.Executor worker=Executors.newSingleThreadExecutor();
    private static volatile boolean loaded,checked;
    private static String lastSimulationSequence="";
    private static String lastToken="",lastError="";
    private static long lastId=-1;
    private static native String readNative(long previousId,boolean preview);
    private static native String startSimulationNative(long expectedPathId,int speed);
    private static native String setSimulationSpeedNative(int speed);
    private static native String stopSimulationNative();
    /** Called only by the provider's main-thread state loop, after preview hash checks. */
    public static void applySimulationCommand(Application app){
        File stop=new File(app.getFilesDir(),"single-engine-simulation-stop.json");
        if(loaded&&checked&&stop.isFile())try{
            byte[] bytes=new byte[2048];int size;
            try(InputStream in=new FileInputStream(stop)){size=in.read(bytes);}
            JSONObject command=new JSONObject(new String(bytes,0,size,StandardCharsets.UTF_8));
            long age=SystemClock.elapsedRealtime()-command.getLong("at");
            if(age>=0&&age<=15000&&command.getString("sequence").equals(lastSimulationSequence)){
                boolean sim=Boolean.TRUE.equals(Class.forName("com.autonavi.amapauto.jni.AndroidDriveNavigation").getMethod("isInSimulateNavi").invoke(null));
                if(sim)Log.i("MotoNavSingleEngine","selected-path simulation stop="+stopSimulationNative());
                lastSimulationSequence="";
            }
            stop.delete();
        }catch(Exception|LinkageError e){stop.delete();Log.w("MotoNavSingleEngine","simulation-stop refused",e);}
        applySpeedCommand(app);
        File file=new File(app.getFilesDir(),"single-engine-simulation-command.json");
        if(!loaded||!checked||!file.isFile()||file.length()>2048)return;
        try{
            byte[] b=new byte[2048];int n;
            try(InputStream in=new FileInputStream(file)){n=in.read(b);}
            JSONObject o=new JSONObject(new String(b,0,n,StandardCharsets.UTF_8));
            String owner=o.getString("owner"),sequence=owner+":"+o.getString("sequence");
            File enabled=new File(app.getFilesDir(),"single-engine-enabled");
            try(InputStream in=new FileInputStream(enabled)){n=in.read(b);}
            long age=SystemClock.elapsedRealtime()-o.getLong("at");
            if(!owner.equals(new String(b,0,n,StandardCharsets.UTF_8))||age<0||age>5000||sequence.equals(lastSimulationSequence)){
                file.delete();return;
            }
            int speed=o.optInt("speed",60);
            if(!SimulationSpeedPolicy.allowed(speed)){file.delete();return;}
            String response=startSimulationNative(o.getLong("pathId"),speed);
            boolean started=response!=null&&response.startsWith("started");
            if(started)lastSimulationSequence=sequence;
            speedAck(app,o,sequence,started,speed,response);
            file.delete();
            Log.i("MotoNavSingleEngine","selected-path simulation start="+response+" path="+o.getLong("pathId"));
        }catch(Exception|LinkageError e){file.delete();Log.w("MotoNavSingleEngine","simulation-start refused",e);}
    }
    private static void speedAck(Application app,JSONObject command,String sequence,boolean applied,int speed,String detail)throws Exception{
        JSONObject ack=new JSONObject().put("sequence",sequence).put("request",command.getString("request"))
            .put("applied",applied).put("speed",speed).put("detail",detail).put("at",SystemClock.elapsedRealtime());
        File tmp=new File(app.getFilesDir(),"single-engine-simulation-speed-ack.tmp");
        try(OutputStream out=new FileOutputStream(tmp)){out.write(ack.toString().getBytes(StandardCharsets.UTF_8));}
        if(!tmp.renameTo(new File(app.getFilesDir(),"single-engine-simulation-speed-ack.json")))throw new IOException("speed-ack-commit");
    }
    private static void applySpeedCommand(Application app){
        File file=new File(app.getFilesDir(),"single-engine-simulation-speed.json");
        if(!loaded||!checked||!file.isFile())return;
        try{
            if(file.length()>2048){file.delete();return;}
            byte[] b=new byte[2048];int n;
            try(InputStream in=new FileInputStream(file)){n=in.read(b);}
            JSONObject command=new JSONObject(new String(b,0,n,StandardCharsets.UTF_8));
            String owner=command.getString("owner"),sequence=command.getString("sequence");
            try(InputStream in=new FileInputStream(new File(app.getFilesDir(),"single-engine-enabled"))){n=in.read(b);}
            long age=SystemClock.elapsedRealtime()-command.getLong("at");
            int speed=command.getInt("speed");
            if(age<0||age>5000||!SimulationSpeedPolicy.allowed(speed)||!sequence.equals(lastSimulationSequence)||
                !owner.equals(new String(b,0,n,StandardCharsets.UTF_8))){file.delete();return;}
            boolean sim=Boolean.TRUE.equals(Class.forName("com.autonavi.amapauto.jni.AndroidDriveNavigation").getMethod("isInSimulateNavi").invoke(null));
            String response=sim?setSimulationSpeedNative(speed):"simulation-not-running";
            boolean applied=("speed="+speed).equals(response);
            speedAck(app,command,sequence,applied,speed,response);
            file.delete();Log.i("MotoNavSingleEngine","simulation-speed "+response);
        }catch(Exception|LinkageError e){file.delete();Log.w("MotoNavSingleEngine","simulation-speed refused",e);}
    }
    public static void idle(){worker.execute(()->lastId=-1);}
    private static String hash(File file)throws Exception{
        MessageDigest d=MessageDigest.getInstance("SHA-256");byte[] bytes=new byte[65536];
        try(InputStream in=new FileInputStream(file)){int n;while((n=in.read(bytes))>0)d.update(bytes,0,n);}
        StringBuilder s=new StringBuilder();for(byte b:d.digest())s.append(String.format(java.util.Locale.ROOT,"%02x",b&255));return s.toString();
    }
    private static String token(Application app)throws IOException{
        File f=new File(app.getFilesDir(),"single-engine-enabled");boolean probe=!f.isFile();
        if(probe)f=new File(app.getFilesDir(),"single-engine-probe");
        try(InputStream in=new FileInputStream(f)){
            byte[] b=new byte[64];int n=in.read(b);String value=n>0?new String(b,0,n,StandardCharsets.UTF_8):"";
            if(probe){long remaining=Long.parseLong(value)-SystemClock.elapsedRealtime();if(remaining<0||remaining>120000)return "";}
            File preview=new File(app.getFilesDir(),"single-engine-preview-enabled");
            if(!probe&&preview.isFile())try(InputStream pin=new FileInputStream(preview)){
                byte[] p=new byte[128];int size=pin.read(p);if(size>0)value+=":"+new String(p,0,size,StandardCharsets.UTF_8);
            }
            return value;
        }
    }
    private static void signal(Application app,String state){
        if(state.equals(lastError))return;lastError=state;
        Log.i("MotoNavNativeRoute",state);
        app.sendBroadcast(new Intent(CarEngineTap.ACTION).setPackage(app.getPackageName())
            .putExtra("at",SystemClock.elapsedRealtime()).putExtra("kind","nativeRouteStatus").putExtra("state",state),
            "com.motonav.app.fresh.permission.CAR_GUIDANCE");
    }
    public static void poll(Application app){
        if(!busy.compareAndSet(false,true))return;
        worker.execute(()->{
            try{
                String current=token(app);if(current.isEmpty())return;
                if(!current.equals(lastToken)){lastToken=current;lastId=-1;}
                if(!checked){
                    File dir=new File(app.getApplicationInfo().nativeLibraryDir);
                    if(!hash(new File(dir,"libhsl.so")).equals("6c67ff15d76b54d808e50da64bb87fc23b8f8f6a86bbdfd92cf31fead9996a18")||
                       !hash(new File(dir,"libGbl.so")).equals("934396f73adf760af02e77ae8bd650c4e659522acd28db35f501d7633675a419")){
                        signal(app,"engine-binary-version-mismatch");return;
                    }
                    checked=true;
                }
                if(!loaded){System.loadLibrary("motonav_route");loaded=true;}
                boolean preview=new File(app.getFilesDir(),"single-engine-preview-enabled").isFile();
                String raw=readNative(lastId,preview);JSONObject o=new JSONObject(raw);
                if(!current.equals(token(app)))return;
                if(o.has("error")){signal(app,o.optString("error"));return;}
                if(o.optBoolean("unchanged"))return;
                o.put("at",SystemClock.elapsedRealtime());o.put("source",preview?"planned-native-path":"active-native-path");
                String file=preview?"single-engine-preview":"single-engine-route";
                File tmp=new File(app.getFilesDir(),file+".tmp"),target=new File(app.getFilesDir(),file+".json");
                try(OutputStream out=new FileOutputStream(tmp)){out.write(o.toString().getBytes(StandardCharsets.UTF_8));}
                if(!tmp.renameTo(target))throw new IOException("route-file-commit");
                lastId=o.getLong("id");signal(app,"active-route-exported points="+o.getInt("count"));
                app.sendBroadcast(new Intent(CarEngineTap.ACTION).setPackage(app.getPackageName())
                    .putExtra("at",SystemClock.elapsedRealtime()).putExtra("kind",preview?"previewRoute":"route").putExtra("id",Long.toString(lastId))
                    .putExtra("count",o.getInt("count")),"com.motonav.app.fresh.permission.CAR_GUIDANCE");
            }catch(Throwable e){signal(app,"reader-"+e.getClass().getSimpleName()+": "+String.valueOf(e.getMessage()));}
            finally{busy.set(false);}
        });
    }
}
