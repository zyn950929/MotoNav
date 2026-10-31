from pathlib import Path
from zipfile import ZipFile,ZipInfo,ZIP_DEFLATED,ZIP_STORED
import re,struct,json,hashlib
from resource_table import merge,parse,values,U32
from native_preview import manual_preview
root=Path(__file__).resolve().parent
carPath=root/'inputs/provider.apk'
motoPath=root/'app/build/outputs/apk/debug/app-debug.apk'
out=root/'fusion-unsigned.pending.apk'
old=b'libneonui_shared.so';new=b'libneonui_moto01.so';assert len(old)==len(new)
def car_names(b):
    for a,z in [(b'androidx.',b'carxlibx.'),(b'android/support/',b'carolds/support/'),(b'android.support.',b'carolds.support.')]:
        assert len(a)==len(z);b=b.replace(a,z).replace(a.decode().encode('utf-16le'),z.decode().encode('utf-16le'))
    return b
def classes(b):
    so=U32(b,60);to=U32(b,68);nc=U32(b,96);co=U32(b,100)
    def string(n):
        p=U32(b,so+4*n)
        while b[p]&128:p+=1
        p+=1
        return b[p:b.index(b'\0',p)].decode('utf8',errors='replace')
    return {string(U32(b,to+4*U32(b,co+32*i))) for i in range(nc)}
sign=re.compile(r'^META-INF/(MANIFEST\.MF|[^/]+\.(SF|RSA|DSA|EC))$',re.I)
native=[];defined=set();dexes=[]
with ZipFile(carPath) as car,ZipFile(motoPath) as moto,ZipFile(root/'car-code.apk') as cd,ZipFile(root/'moto-code.apk') as md:
    resource,resourceInfo=merge(car_names(car.read('resources.arsc')),moto.read('resources.arsc'))
    for source in [cd,md]:
        for n in sorted(source.namelist()):
            if not re.fullmatch(r'classes\d*\.dex',n):continue
            raw=source.read(n);definedNow=classes(raw);dup=defined&definedNow
            assert not dup,('duplicate definitions',sorted(dup)[:10])
            defined|=definedNow;dexes.append(raw)
    names=set()
    with ZipFile(out,'w',ZIP_DEFLATED,compresslevel=6) as z:
        def add(name,b,info=None):
            assert name not in names,('ZIP collision',name)
            names.add(name)
            if name=='resources.arsc':
                zi=ZipInfo(name);zi.compress_type=ZIP_STORED;z.writestr(zi,b)
            elif info is not None:
                zi=ZipInfo(name,info.date_time);zi.compress_type=info.compress_type
                z.writestr(zi,b)
            else:z.writestr(name,b)
        add('AndroidManifest.xml',(root/'AndroidManifest.binary.xml').read_bytes())
        add('resources.arsc',resource)
        for i,raw in enumerate(dexes):add('classes'+(str(i+1) if i else '')+'.dex',raw)
        for info in car.infolist():
            name=info.filename
            if name in names or sign.match(name) or re.fullmatch(r'classes\d*\.dex',name):continue
            if name.startswith('kotlin/') and name.endswith('.kotlin_builtins'):continue
            b=car.read(name)
            if name=='lib/arm64-v8a/libautonavi.so':b=manual_preview(b)
            if name=='lib/arm64-v8a/libhsl.so':
                original=b'/data/data/com.autonavi.amapauto'
                replacement=b'/data/data/com.motonav.app.fresh'
                assert len(original)==len(replacement) and b.count(original)==1
                b=b.replace(original,replacement)
            if name.startswith('res/') and name.endswith('.xml'):b=car_names(b)
            add(name,b,info)
        for info in moto.infolist():
            name=info.filename
            if name=='AndroidManifest.xml' or name=='resources.arsc' or name.startswith('META-INF/') or re.fullmatch(r'classes\d*\.dex',name):continue
            if name in ('assets/dexopt/baseline.prof','assets/dexopt/baseline.profm'):continue
            b=moto.read(name);dest='res/motonav/'+name[4:] if name.startswith('res/') else name
            if name.endswith('.so'):
                hits=b.count(old);b=b.replace(old,new)
                if name.endswith('/libneonui_shared.so'):dest=name.replace('libneonui_shared.so','libneonui_moto01.so')
                native.append({'file':name,'output':dest,'rewrittenLibraryNameReferences':hits})
            add(dest,b,info)
    # Every compiled file resource must resolve to an actual member of this single APK.
    (strings,_,_),packages=parse(resource)
    checked=0
    for package in packages:
        for pos in values(package):
            if package[pos+3]==3:
                value=strings[U32(package,pos+4)]
                if value.startswith('res/'):
                    assert value in names,('missing file resource',value);checked+=1
with ZipFile(out) as verify:
    assert verify.testzip() is None
out.replace(root/'fusion-unsigned.apk')
out=root/'fusion-unsigned.apk'
summary={'bytes':out.stat().st_size,'definedClasses':len(defined),'dexFiles':len(dexes),'duplicateClasses':0,
 'resourceMerge':resourceInfo,'verifiedFileResourceReferences':checked,'motoNativeIsolation':native,
 'installedPackage':'com.motonav.app.fresh','motoProcess':':motonav','packageCount':1,
 'testedOnDevice':False,'noRouteCruiseCountdownVerified':False,
 'routePreviewManualStartOnly':{'library':'libautonavi.so','offset':'0xd612c0','bytesChanged':4,'originalHashChecked':True}}
(root/'merge-checks.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),'utf8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
