package com.yuminaga.motonav

import android.content.Context
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Looper
import android.os.SystemClock
import android.util.Log

/** Same granted GPS permission; independent of SDK navigation callbacks while stopped. */
object TrafficPositionMonitor {
    private var manager:LocationManager?=null
    private val listener=object:LocationListener {
        override fun onProviderEnabled(provider:String){}
        override fun onProviderDisabled(provider:String){}
        @Deprecated("Legacy LocationListener callback")
        override fun onStatusChanged(provider:String,status:Int,extras:android.os.Bundle?){}
        override fun onLocationChanged(fix:Location){
            val age=(SystemClock.elapsedRealtimeNanos()-fix.elapsedRealtimeNanos)/1_000_000
            if(age !in 0..5000)return
            val gcj=MapGeo.wgsToGcj(fix.latitude,fix.longitude)
            @Suppress("DEPRECATION")
            val mock=fix.isFromMockProvider
            SingleEngineSession.rawFix(gcj[0],gcj[1],if(fix.hasBearing())fix.bearing else Float.NaN,
                if(fix.hasSpeed())(fix.speed*3.6).toInt() else -1,
                if(fix.hasAccuracy())fix.accuracy.toDouble() else Double.NaN,fix.time,mock)
            TrafficLightOverlay.positionFix(gcj[0],gcj[1],if(fix.hasAccuracy())fix.accuracy.toDouble() else Double.NaN,
                if(fix.hasSpeed())fix.speed.toDouble() else Double.NaN,fix.time,System.currentTimeMillis(),SystemClock.elapsedRealtime(),true)
        }
    }
    fun start(c:Context){
        if(c.packageName!="com.motonav.app.fresh" || manager!=null)return
        try{
            val m=c.getSystemService(Context.LOCATION_SERVICE) as LocationManager
            // Keep stopped fixes fresh for the red-light hold gate; a 2m minimum
            // would suppress the very updates needed while waiting at a signal.
            m.requestLocationUpdates(LocationManager.GPS_PROVIDER,2000L,0f,listener,Looper.getMainLooper())
            manager=m
        }catch(e:RuntimeException){Log.w("MotoNavLightOverlay","GPS stop observer unavailable; SDK location fallback remains",e)}
    }
    fun stop(){manager?.let{runCatching{it.removeUpdates(listener)}};manager=null}
}
