package com.yuminaga.motonav

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import org.json.JSONObject

/** Bind from MotoNav's foreground service, never from an Activity lifecycle.
 * The native process inherits the live client's importance while needed. A
 * notification posted by the provider alone does not establish this dependency.
 * No JNI, navigation command, additional wake lock or polling loop is involved.
 */
object ProviderConnection {
    private val main=Handler(Looper.getMainLooper())
    private var context:Context?=null
    private var connection:ServiceConnection?=null
    private var wanted=false
    private var connected=false
    private var reconnects=0
    private val retry=Runnable{context?.let{update(it,wanted)}}

    fun update(c:Context,needed:Boolean){
        wanted=needed && c.packageName=="com.motonav.app.fresh"
        context=c.applicationContext
        if(!wanted){release();return}
        if(connection!=null)return
        val app=context!!
        val next=object:ServiceConnection {
            override fun onServiceConnected(name:ComponentName,binder:IBinder){
                if(connection!==this)return
                connected=true
                Log.i("MotoNavProviderLink","native service bound to foreground navigation")
            }
            override fun onServiceDisconnected(name:ComponentName){
                if(connection!==this)return
                connected=false;reconnects++
                // Android retains this binding and reconnects after a process crash.
                Log.w("MotoNavProviderLink","native service disconnected; binding retained")
            }
            override fun onBindingDied(name:ComponentName){lost(this)}
            override fun onNullBinding(name:ComponentName){lost(this)}
        }
        connection=next
        val accepted=runCatching{app.bindService(Intent().setClassName(app.packageName,
            "com.autonavi.amapauto.background.AutoBackgroundService"),next,
            Context.BIND_AUTO_CREATE or Context.BIND_IMPORTANT)}
            .onFailure{Log.w("MotoNavProviderLink","native service binding failed",it)}.getOrDefault(false)
        if(!accepted){
            if(connection===next)connection=null
            runCatching{app.unbindService(next)}
            main.removeCallbacks(retry);main.postDelayed(retry,5000)
        }
    }
    private fun lost(previous:ServiceConnection){
        if(connection!==previous)return
        context?.let{runCatching{it.unbindService(previous)}}
        connection=null;connected=false;reconnects++
        main.removeCallbacks(retry)
        if(wanted)main.postDelayed(retry,2000)
    }
    fun release(){
        wanted=false;main.removeCallbacks(retry)
        val previous=connection;connection=null;connected=false
        if(previous!=null){
            context?.let{runCatching{it.unbindService(previous)}}
            Log.i("MotoNavProviderLink","navigation dependency released")
        }
    }
    fun debug()=JSONObject().put("wanted",wanted).put("binding",connection!=null)
        .put("connected",connected).put("reconnects",reconnects)
}
