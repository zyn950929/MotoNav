from pathlib import Path
import os,subprocess,json,sys,time

out=Path('ui-review-output');out.mkdir(exist_ok=True)
def run(*args,timeout=60,capture=False,check=True):
    return subprocess.run(list(args),check=check,timeout=timeout,capture_output=capture,text=True)
def sim(*args,**kwargs):return run('xcrun','simctl',*args,**kwargs)
if sys.argv[1]=='boot':
    data=json.loads(sim('list','devices','available','--json',capture=True).stdout)
    devices=[d for runtime,items in data['devices'].items() if '.iOS-' in runtime for d in items if d['name'].startswith('iPhone')]
    chosen=next((d for d in devices if 'SE' in d['name']),devices[0])
    (out/'device.json').write_text(json.dumps(chosen))
    if chosen['state']!='Booted':sim('boot',chosen['udid'])
    print('Simulator boot requested:',chosen['name'],flush=True)
else:
    device=json.loads((out/'device.json').read_text())['udid']
    sim('bootstatus',device,'-b',timeout=240)
    app=str(Path(os.environ['RUNNER_TEMP'])/'UIBuild/Build/Products/Debug-iphonesimulator/MotoNav.app')
    sim('install',device,app,timeout=60)
    sim('privacy',device,'grant','location-always','com.example.motonav')
    sim('status_bar',device,'override','--time','09:41','--batteryState','charged','--batteryLevel','100')
    for mode,page,label in [(1,0,'navigation-light'),(1,1,'rides-light'),(1,2,'device-light'),(1,3,'settings-light'),(2,0,'navigation-dark'),(2,2,'device-dark')]:
        sim('terminate',device,'com.example.motonav',check=False)
        sim('launch',device,'com.example.motonav','-key_amap','UI_REVIEW_PLACEHOLDER','-ui_selected_tab',str(page),'-ui_appearance',str(mode),timeout=45)
        time.sleep(6)
        sim('io',device,'screenshot',str(out/(label+'.png')))
    print('Captured six actual app screens; no device navigation or road test performed.')
