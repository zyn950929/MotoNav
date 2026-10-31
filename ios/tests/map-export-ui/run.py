from pathlib import Path
import subprocess,json,plistlib,platform,os

def run(*args):
    print("+",*args,flush=True)
    result=subprocess.run(args,capture_output=True,text=True)
    if result.returncode:
        print(result.stdout,flush=True);print(result.stderr,flush=True)
        result.check_returncode()
    return result.stdout.strip()
out=Path("output/export-ui");out.mkdir(parents=True,exist_ok=True)
app=Path(os.environ["RUNNER_TEMP"])/"ExportProbe.app";app.mkdir(exist_ok=True)
sdk=run("xcrun","--sdk","iphonesimulator","--show-sdk-path")
arch="arm64" if platform.machine()=="arm64" else "x86_64"
run("xcrun","swiftc","-parse-as-library","-target",arch+"-apple-ios15.0-simulator","-sdk",sdk,"ios/MotoNav/MapReportExport.swift","ios/tests/map-export-ui/Probe.swift","-o",str(app/"ExportProbe"))
info={"CFBundleIdentifier":"com.example.motonav.exportprobe","CFBundleExecutable":"ExportProbe","CFBundleName":"ExportProbe","CFBundlePackageType":"APPL","CFBundleVersion":"1","CFBundleShortVersionString":"1.0","LSRequiresIPhoneOS":True,"MinimumOSVersion":"15.0","UIDeviceFamily":[1,2],"UILaunchScreen":{},"UISupportedInterfaceOrientations":["UIInterfaceOrientationPortrait"]}
(app/"Info.plist").write_bytes(plistlib.dumps(info))
run("codesign","--force","--sign","-",str(app))
devices=json.loads(run("xcrun","simctl","list","devices","available","--json"))["devices"]
choices=[d for runtime,items in devices.items() if "iOS" in runtime for d in items if d["name"].startswith("iPhone")]
assert choices,"No iOS simulator runtime installed"
device=choices[0];udid=device["udid"]
if device["state"]!="Booted":run("xcrun","simctl","boot",udid)
run("xcrun","simctl","bootstatus",udid,"-b")
run("xcrun","simctl","ui",udid,"appearance","light")
run("xcrun","simctl","install",udid,str(app))
for mode in ["share","save"]:
    run("xcrun","simctl","launch","--terminate-running-process",udid,"com.example.motonav.exportprobe","--"+mode,"-AppleLanguages","(en)","-AppleLocale","en_US")
    # Let the nested report sheet and native controller complete their first animations.
    subprocess.run(["sleep","18"],check=True)
    run("xcrun","simctl","io",udid,"screenshot",str(out/(mode+"-first-open.png")))
(out/"probe.json").write_text(json.dumps({"device":device["name"],"source":"production MapReportExportSheet","nesting":"root -> diagnostics sheet -> export sheet","file":"map-timing-probe.json","firstPresentationScreenshots":["share-first-open.png","save-first-open.png"]},indent=2))
print("PASS native export screenshots captured for visual inspection")
