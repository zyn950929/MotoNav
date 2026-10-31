// Public source build: original signing keys are deliberately excluded.
import java.util.Properties
import java.io.File
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val signingDir = providers.gradleProperty("localSigningDir").orNull?.let { File(it) } ?: rootProject.file("keystore")
val ksProps = Properties().apply {
    val f = File(signingDir,"keystore.properties")
    if (f.exists()) f.inputStream().use { load(it) }
}

// Generate a new development key on the recipient's machine, including when
// building directly in Android Studio. It is excluded from the source archive.
val localDebugKey = rootProject.file(".local/debug.keystore")
val createLocalDebugKeystore = tasks.register<Exec>("createLocalDebugKeystore") {
    onlyIf { !localDebugKey.exists() }
    doFirst { localDebugKey.parentFile.mkdirs() }
    val executable = if (System.getProperty("os.name").startsWith("Windows")) "keytool.exe" else "keytool"
    commandLine(File(System.getProperty("java.home"), "bin/$executable").absolutePath,
        "-genkeypair", "-keystore", localDebugKey.absolutePath,
        "-storepass", "android", "-keypass", "android", "-alias", "androiddebugkey",
        "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
        "-dname", "CN=Android Debug,O=Android,C=US")
}
tasks.configureEach {
    if (name == "validateSigningDebug") dependsOn(createLocalDebugKeystore)
}

android {
    namespace = "com.yuminaga.motonav"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.motonav.app.fresh"
        minSdk = 28
        targetSdk = 35
        versionCode = 182
        versionName = "8.3.26-single.32-fresh.1"
        ndk { abiFilters += setOf("arm64-v8a") }
    }

    signingConfigs {
        getByName("debug") { storeFile = rootProject.file(".local/debug.keystore") }
        if (File(signingDir,"keystore.properties").exists()) {
            create("release") {
                storeFile = File(signingDir,ksProps.getProperty("storeFile"))
                storePassword = ksProps.getProperty("storePassword")
                keyAlias = ksProps.getProperty("keyAlias")
                keyPassword = ksProps.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("release")
        }
    }

    androidResources { additionalParameters += listOf("--package-id", "0x80") }

    packaging {
        jniLibs { useLegacyPackaging = true }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    lint {
        checkReleaseBuilds = false
        abortOnError = false
    }
}

kotlin {
    compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
}

dependencies {
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-ktx:1.9.3")
    implementation("com.amap.api:navi-3dmap-location-search:10.1.600_3dmap10.1.600_loc6.5.1_sea9.7.4")
}
