package com.yuminaga.motonav

import android.content.Context

/** Only records the user's setup choice; never accepts terms or grants permissions. */
object FusionFirstUse {
    private fun prefs(c:Context):android.content.SharedPreferences {
        val p=c.getSharedPreferences("motonav_provider_setup",Context.MODE_PRIVATE)
        val installed=c.packageManager.getPackageInfo(c.packageName,0).firstInstallTime
        // A restored backup on another installation is not an authorization.
        if(p.getLong("installation",-1)!=installed)p.edit().clear().putLong("installation",installed).commit()
        return p
    }
    private fun supported(c:Context)=c.packageName=="com.motonav.app.fresh" && !APP_VERSION.contains("-replay.")
    fun complete(c:Context)=!supported(c) || prefs(c).getBoolean("completed",false)
    fun shouldOpen(c:Context)=supported(c) && !prefs(c).getBoolean("presented",false)
    fun returning(c:Context)=supported(c) && prefs(c).getBoolean("return_pending",false)
    fun open(c:Context) {
        if(!supported(c))return
        // Persist before changing tasks so recreation/process death cannot loop.
        prefs(c).edit().putBoolean("presented",true).putBoolean("return_pending",true).commit()
        try { FusionCarUi.openFirstSetup(c) }
        catch(e:RuntimeException) {
            prefs(c).edit().putBoolean("return_pending",false).apply()
            throw e
        }
    }
    fun consumeReturn(c:Context):Boolean {
        if(!supported(c) || !prefs(c).getBoolean("return_pending",false))return false
        prefs(c).edit().putBoolean("return_pending",false).commit()
        return true
    }
    fun finish(c:Context) {
        prefs(c).edit().putBoolean("completed",true).putBoolean("return_pending",false).apply()
        FusionCarUi.closeConsole(c)
    }
    fun later(c:Context) { FusionCarUi.closeConsole(c) }
}
