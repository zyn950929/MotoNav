package com.yuminaga.motonav;

import android.app.*;
import android.content.*;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.*;
import android.util.Log;
import android.view.*;
import android.widget.Button;
import android.widget.FrameLayout;
import java.io.File;
import java.io.IOException;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.nio.charset.StandardCharsets;
import java.util.WeakHashMap;

/** Android-only adapter, safe in the provider process without loading MotoNav SDKs.
 * Native initialization may still use an Activity; it is transparent and returned
 * behind the user's task. Explicit first-time setup/diagnostics remains available.
 */
public final class FusionCarUi implements Application.ActivityLifecycleCallbacks {
    private static final String MAP="com.autonavi.amapauto.MainMapActivity";
    private static final String CHANNEL="motonav_junction_service";
    private static final String MARKER="fusion-show-provider-console";
    private final Handler handler=new Handler(Looper.getMainLooper());
    private final WeakHashMap<Activity,Boolean> resumed=new WeakHashMap<>();
    private boolean warmed;
    private static File marker(Context c){return new File(c.getFilesDir(),MARKER);}
    private static File firstSetup(Context c){return new File(c.getFilesDir(),"fusion-first-setup");}
    private static boolean console(Context c){return marker(c).isFile();}
    private static File warmFile(Context c){return new File(c.getFilesDir(),"fusion-engine-warm");}
    private static File readyFile(Context c){return new File(c.getFilesDir(),"fusion-engine-ready");}
    private static String read(File f){
        try(FileInputStream in=new FileInputStream(f)){
            byte[] b=new byte[80];int n=in.read(b);return n>0?new String(b,0,n,StandardCharsets.UTF_8):"";
        }catch(IOException e){return "";}
    }
    private static void write(File f,String value)throws IOException{
        try(FileOutputStream out=new FileOutputStream(f)){out.write(value.getBytes(StandardCharsets.UTF_8));}
    }
    private static boolean nativeReady(){
        try{return Boolean.TRUE.equals(Class.forName("com.autonavi.amapauto.jni.protocol.AndroidProtocolExe")
            .getMethod("getReadyStatus").invoke(null));}
        catch(ReflectiveOperationException|LinkageError e){return false;}
    }
    public static void install(Application app){
        // AppComponentFactory also instantiates :locationservice and :push.
        // Their JNI state/readiness is not the actual navigation engine's state.
        String process=Application.getProcessName();
        if(process==null||process.indexOf(':')>=0)return;
        FusionCarUi ui=new FusionCarUi();app.registerActivityLifecycleCallbacks(ui);
        // Run in the provider process: never load its JNI/Application in :motonav.
        ui.handler.post(new Runnable(){public void run(){
            CarEngineTap.engineState(app);
            String token=read(warmFile(app));
            if(!token.isEmpty()&&!token.equals(read(readyFile(app)))&&nativeReady()){
                try{write(readyFile(app),token);Log.i("MotoNavProviderUi","engine-ready token="+token);}
                catch(IOException e){Log.w("MotoNavProviderUi","ready acknowledgement",e);}
            }
            ui.handler.postDelayed(this,500);
        }});
    }
    public static void warm(Context c,long token){
        try{write(warmFile(c),Long.toString(token));readyFile(c).delete();}
        catch(IOException e){Log.w("MotoNavProviderUi","warm request",e);return;}
        prepare(c);
        try{c.startActivity(new Intent().setClassName(c.getPackageName(),MAP)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP|Intent.FLAG_ACTIVITY_NO_ANIMATION));}
        catch(RuntimeException e){Log.w("MotoNavProviderUi","hidden engine initialization",e);}
    }
    public static boolean prepared(Context c,long token){return Long.toString(token).equals(read(readyFile(c)));}
    public static void finishWarm(Context c,long token){
        if(Long.toString(token).equals(read(warmFile(c)))){warmFile(c).delete();readyFile(c).delete();}
    }
    /** Only gates the provider's floating WINDOW. Guidance/light callbacks stay enabled. */
    public static boolean allowProviderFloat(){
        try {
            Application app=(Application)Class.forName("android.app.ActivityThread")
                .getMethod("currentApplication").invoke(null);
            return app!=null && console(app);
        } catch(ReflectiveOperationException e){return false;}
    }
    public static void maskProviderFloat(View view,WindowManager.LayoutParams params){
        if(allowProviderFloat())return;
        params.alpha=0f;
        params.flags|=WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE|WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        view.setAlpha(0f);
        view.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        Log.i("MotoNavProviderUi","provider float masked; native initialization/subscriptions preserved");
    }
    public static void prepare(Context c){
        if(!"com.motonav.app.fresh".equals(c.getPackageName()))return;
        // Explicit inspection stays visible through background route retries.
        // The return button / MotoNav resume closes it, rather than an engine request.
        try {
            Intent i=new Intent().setClassName(c.getPackageName(),
                "com.autonavi.amapauto.background.AutoBackgroundService");
            if(Build.VERSION.SDK_INT>=26)c.startForegroundService(i);else c.startService(i);
            Log.i("MotoNavProviderUi","background service requested");
        } catch(RuntimeException e){Log.w("MotoNavProviderUi","background service unavailable",e);}
    }
    public static void openConsole(Context c){
        openConsole(c,"com.yuminaga.motonav.MainActivity");
    }
    private static String returnClass(String name){
        if("com.yuminaga.motonav.SingleEngineActivity".equals(name) ||
           "com.yuminaga.motonav.NavActivity".equals(name) ||
           "com.yuminaga.motonav.MotorWebNavActivity".equals(name))return name;
        return "com.yuminaga.motonav.MainActivity";
    }
    public static void openConsole(Context c,String destination){
        try(FileOutputStream out=new FileOutputStream(marker(c))){
            out.write(returnClass(destination).getBytes(StandardCharsets.UTF_8));
        } catch(IOException e){throw new IllegalStateException("Console marker unavailable",e);}
        c.startActivity(new Intent().setClassName(c.getPackageName(),MAP)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP));
    }
    public static void openFirstSetup(Context c){
        try {firstSetup(c).createNewFile();}
        catch(IOException e){throw new IllegalStateException("Setup marker unavailable",e);}
        openConsole(c);
    }
    public static void closeConsole(Context c){marker(c).delete();firstSetup(c).delete();}
    private void addSetupReturn(Activity a){
        if(!console(a))return;
        ViewGroup content=a.findViewById(android.R.id.content);
        if(content==null || content.findViewWithTag("motonav-setup-return")!=null)return;
        Button button=new Button(a);button.setTag("motonav-setup-return");
        button.setText("返回 MotoNav");button.setAllCaps(false);
        button.setTextColor(Color.WHITE);button.setBackgroundColor(0xff352b47);
        float density=a.getResources().getDisplayMetrics().density;
        FrameLayout.LayoutParams p=new FrameLayout.LayoutParams((int)(160*density),(int)(48*density),Gravity.TOP|Gravity.END);
        p.topMargin=(int)(8*density);p.rightMargin=(int)(8*density);
        content.addView(button,p);
        button.setOnClickListener(v->{
            String destination="com.yuminaga.motonav.MainActivity";
            try(FileInputStream in=new FileInputStream(marker(a))){
                byte[] bytes=new byte[160];int n=in.read(bytes);
                if(n>0)destination=returnClass(new String(bytes,0,n,StandardCharsets.UTF_8));
            }catch(IOException e){Log.w("MotoNavProviderUi","Return destination unavailable",e);}
            Intent back=new Intent().setClassName(a.getPackageName(),destination)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP);
            // Clearing to MainActivity would destroy the live SDK navigation page.
            if(!destination.endsWith(".MainActivity"))back.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            closeConsole(a);configure(a);
            a.startActivity(back);
            Log.i("MotoNavProviderUi","console-return target="+destination);
        });
    }
    /** Keep the mandatory foreground-service notification; tapping returns MotoNav. */
    public static Notification notification(Context c,Notification original){
        if(!"com.motonav.app.fresh".equals(c.getPackageName()))return original;
        NotificationManager manager=(NotificationManager)c.getSystemService(Context.NOTIFICATION_SERVICE);
        if(Build.VERSION.SDK_INT>=26)manager.createNotificationChannel(
            new NotificationChannel(CHANNEL,"MotoNav 路口信息",NotificationManager.IMPORTANCE_LOW));
        Intent open=new Intent().setClassName(c.getPackageName(),"com.yuminaga.motonav.MainActivity")
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent tap=PendingIntent.getActivity(c,7317,open,PendingIntent.FLAG_UPDATE_CURRENT|PendingIntent.FLAG_IMMUTABLE);
        Notification.Builder b=Build.VERSION.SDK_INT>=26?new Notification.Builder(c,CHANNEL):new Notification.Builder(c);
        return b.setSmallIcon(android.R.drawable.ic_dialog_map).setContentTitle("MotoNav 正在运行")
            .setContentText("路口信息服务运行中，点此返回 MotoNav").setContentIntent(tap)
            .setOngoing(true).setOnlyAlertOnce(true).setCategory(Notification.CATEGORY_SERVICE).build();
    }
    private boolean target(Activity a){return MAP.equals(a.getClass().getName());}
    private void configure(Activity a){
        if(!target(a))return;
        Window w=a.getWindow();
        if(console(a)){
            int originalTheme=a.getResources().getIdentifier("Theme.Background","style",a.getPackageName());
            if(originalTheme!=0)a.setTheme(originalTheme);
            w.clearFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE|WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
            WindowManager.LayoutParams p=w.getAttributes();p.alpha=1f;w.setAttributes(p);
            w.setBackgroundDrawable(new ColorDrawable(Color.BLACK));
            return;
        }
        int theme=a.getResources().getIdentifier("FusionHiddenCarTheme","style",a.getPackageName());
        if(theme!=0)a.setTheme(theme);
        w.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE|WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
        WindowManager.LayoutParams p=w.getAttributes();p.alpha=0f;p.windowAnimations=0;w.setAttributes(p);
        w.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        a.overridePendingTransition(0,0);
    }
    @Override public void onActivityPreCreated(Activity a,Bundle state){configure(a);}
    @Override public void onActivityCreated(Activity a,Bundle state){configure(a);}
    @Override public void onActivityResumed(Activity a){
        if(!target(a))return;
        resumed.put(a,true);configure(a);
        if(console(a)){addSetupReturn(a);Log.i("MotoNavProviderUi","explicit setup console visible");return;}
        Log.i("MotoNavProviderUi","provider resumed with transparent non-interactive window");
        final long deadline=SystemClock.elapsedRealtime()+12000;
        handler.postDelayed(new Runnable(){public void run(){
            if(a.isFinishing()||a.isDestroyed()||!Boolean.TRUE.equals(resumed.get(a))||console(a))return;
            // Hiding the task before HMI initialization completes can strand route
            // commands until the user manually opens it. Wait for the real ready flag.
            if(!nativeReady()&&SystemClock.elapsedRealtime()<deadline){handler.postDelayed(this,250);return;}
            configure(a);
            boolean moved=a.moveTaskToBack(true);
            a.overridePendingTransition(0,0);warmed=true;
            Log.i("MotoNavProviderUi","provider task backgrounded="+moved+" ready="+nativeReady());
        }},300);
    }
    @Override public void onActivityPaused(Activity a){resumed.put(a,false);}
    @Override public void onActivityStarted(Activity a){}
    @Override public void onActivityStopped(Activity a){}
    @Override public void onActivitySaveInstanceState(Activity a,Bundle state){}
    @Override public void onActivityDestroyed(Activity a){resumed.remove(a);}
}
