from pathlib import Path
import os
import shutil,re,json,copy,subprocess,xml.etree.ElementTree as E
root=Path(__file__).resolve().parent
base=root/'car-resources'
code=root/'car-code'
if not code.exists():
    code.mkdir()
    for name in ['AndroidManifest.xml','resources.arsc','apktool.yml']:shutil.copy2(base/name,code/name)
    shutil.copytree(base/'smali',code/'smali')
    for p in list((code/'smali').rglob('*.smali')):
        text=p.read_text('utf8').replace('androidx/','carxlibx/').replace('androidx.','carxlibx.')
        text=text.replace('android/support/','carolds/support/').replace('android.support.','carolds.support.')
        p.write_text(text,'utf8')
    (code/'smali/androidx').rename(code/'smali/carxlibx')
    if(code/'smali/android/support').exists():
        (code/'smali/carolds').mkdir(exist_ok=True)
        (code/'smali/android/support').rename(code/'smali/carolds/support')

bt=Path(os.environ['ANDROID_SDK_ROOT'])/'build-tools/35.0.0'
moto=root/'app/build/outputs/apk/debug/app-debug.apk'
dump=subprocess.check_output([str(bt/'aapt2.exe'),'dump','resources',str(moto)]).decode('utf8')
mr={name:'@'+rid for rid,name in re.findall(r'resource (0x[0-9a-f]+) (\S+)',dump)}
cr={e.get('type')+'/'+e.get('name'):'@'+e.get('id') for e in E.parse(root/'car-resources/res/values/public.xml').getroot() if e.tag=='public'}
A='{http://schemas.android.com/apk/res/android}'
E.register_namespace('android',A[1:-1])
car=E.parse(root/'car-resources/AndroidManifest.xml').getroot()
for e in car.iter():
    for k,v in list(e.attrib.items()):
        e.set(k,v.replace('androidx.','carxlibx.').replace('android.support.','carolds.support.'))
motoxml=root/'app/build/intermediates/merged_manifests/debug/processDebugManifest/AndroidManifest.xml'
own=E.parse(motoxml).getroot()
host=car.get('package');original=own.get('package')
def rewrite_refs(node,resources):
    for e in node.iter():
        for k,v in list(e.attrib.items()):
            if v.startswith('@') and ':' not in v and not v.startswith('@0x'):
                key=v[1:];assert key in resources,(e.tag,k,v)
                e.set(k,resources[key])
def expand(e,pkg):
    name=e.get(A+'name')
    if name and (name.startswith('.') or '.' not in name):e.set(A+'name',pkg+('.' if not name.startswith('.') else '')+name)
rewrite_refs(car,cr);rewrite_refs(own,mr)
app=car.find('application');mapp=own.find('application')
assert app is not None and mapp is not None
app.set(A+'appComponentFactory','com.yuminaga.motonav.FusionComponentFactory')
app.set(A+'debuggable','false')
app.set(A+'label',mapp.get(A+'label'))
app.set(A+'icon',mapp.get(A+'icon'))
app.set(A+'extractNativeLibs','true')
for e in app:
    if e.tag in ['activity','activity-alias','provider','service','receiver']:expand(e,host)
    # Keep the car console as an internal activity, with one MotoNav launcher icon.
    for f in list(e.findall('intent-filter')):
        if any(x.get(A+'name')=='android.intent.category.LAUNCHER' for x in f.findall('category')):e.remove(f)
    if e.tag=='activity' and e.get(A+'name')==host+'.MainMapActivity':
        e.set(A+'theme',mr['style/FusionHiddenCarTheme'])
        e.set(A+'excludeFromRecents','true')
for e in list(mapp):
    if e.tag not in ['activity','activity-alias','provider','service','receiver']:continue
    item=copy.deepcopy(e);expand(item,original)
    item.set(A+'process',':motonav')
    # Explicitly preserve MotoNav's HWUI requirement in the merged manifest.
    # TextureMapView cannot draw in the software-rendered windows seen on device.
    if item.tag=='activity':
        item.set(A+'hardwareAccelerated','true')
        # Car MainMapActivity is singleTask. It must not clear MotoNav navigation
        # off the same task stack (nor be cleared when MotoNav returns).
        item.set(A+'taskAffinity',host+'.motonav')
    if item.tag in ['activity','activity-alias'] and A+'theme' not in item.attrib:item.set(A+'theme',mapp.get(A+'theme'))
    authority=item.get(A+'authorities')
    if authority:item.set(A+'authorities',authority.replace(original,host))
    app.append(item)
for e in own:
    if e.tag in ['application','uses-sdk','queries']:continue
    if e.tag.startswith('uses-permission') or e.tag in ['uses-feature','permission']:
        name=e.get(A+'name')
        # AndroidX derives this permission from the installed host package.
        # Never redeclare the independent formal MotoNav application's permission.
        if name==original+'.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION':
            name=host+'.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION'
            e.set(A+'name',name)
        if not any(x.tag==e.tag and x.get(A+'name')==name for x in car):car.append(copy.deepcopy(e))
queries=car.find('queries')
if queries is None:queries=E.SubElement(car,'queries')
oq=own.find('queries')
if oq is not None:
    for q in oq:queries.append(copy.deepcopy(q))
# Preserve original target behavior for the car's older receiver/service APIs.
sdk=car.find('uses-sdk')
if sdk is None:sdk=E.SubElement(car,'uses-sdk')
sdk.set(A+'minSdkVersion','28')
if A+'targetSdkVersion' not in sdk.attrib:sdk.set(A+'targetSdkVersion','32')
# The car engine parses this field as its own numeric protocol version.
# The MotoNav UI separately displays APP_VERSION=8.3.26-single.32.
car.set(A+'versionCode','26000032');car.set(A+'versionName','9.5.0.600013')
for e in car.iter():
    for k in list(e.attrib):
        if k.startswith('{http://schemas.android.com/tools}'):del e.attrib[k]
# Relocate only host identities, not the provider implementation class names.
car.set('package','com.motonav.app.fresh')
for node in car.iter():
    for attr,val in list(node.attrib.items()):
        if attr in [A+'authorities',A+'permission',A+'readPermission',A+'writePermission',A+'taskAffinity'] or (node.tag in ['permission','uses-permission'] and attr==A+'name'):
            val=val.replace('com.autonavi.amapauto','com.motonav.app.fresh').replace('com.yuminaga.motonav.permission.CAR_GUIDANCE','com.motonav.app.fresh.permission.CAR_GUIDANCE')
            node.set(attr,val)
E.ElementTree(car).write(root/'AndroidManifest.xml',encoding='utf-8',xml_declaration=True)
print('Merged manifest prepared; target SDK',sdk.get(A+'targetSdkVersion'))
