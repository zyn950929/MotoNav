#!/usr/bin/env python3
"""Build the countdown edition, automatically fetching its pinned dependency if needed."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from provider_download import resolve_provider

ROOT=Path(__file__).resolve().parent
VERSION='8.3.26-single.32-fresh.1'
sha=lambda b:hashlib.sha256(b).hexdigest()

def patch_dependency():
    spec=json.loads((ROOT/'provider-integration-deltas.json').read_text('utf8'))
    for item in spec['files']:
        path=ROOT/'car-code/smali'/item['path']
        text=path.read_text('utf8')
        if sha(text.encode())==item['afterSHA256']:continue
        if sha(text.encode())!=item['beforeSHA256']:
            raise ValueError('Unsupported dependency preimage: '+item['path'])
        lines=text.splitlines(keepends=True)
        for edit in reversed(item['edits']):
            if lines[edit['start']:edit['end']]!=edit['old']:
                raise ValueError('Dependency patch context mismatch: '+item['path'])
            lines[edit['start']:edit['end']]=edit['new']
        result=''.join(lines)
        if sha(result.encode())!=item['afterSHA256']:raise ValueError('Dependency patch result mismatch')
        path.write_text(result,encoding='utf8',newline='\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider',type=Path,help='Optional existing APK; otherwise download to inputs/provider.apk')
    parser.add_argument('--apktool',required=True,type=Path)
    parser.add_argument('--gradle',required=True,type=Path)
    parser.add_argument('--sdk',required=True,type=Path)
    parser.add_argument('--java-home',required=True,type=Path)
    parser.add_argument('--zig',required=True,type=Path,help='Zig 0.13.0 executable')
    parser.add_argument('--offline',action='store_true')
    args=parser.parse_args()
    if os.name!='nt':parser.error('The complete countdown builder currently supports Windows only.')
    spec=json.loads((ROOT/'provider-integration-deltas.json').read_text('utf8'))
    try:
        args.provider=resolve_provider(args.provider,offline=args.offline)
    except (ValueError,OSError) as error:
        parser.error(str(error))
    os.environ['JAVA_HOME']=str(args.java_home.resolve())
    os.environ['ANDROID_SDK_ROOT']=str(args.sdk.resolve())
    os.environ['ANDROID_HOME']=os.environ['ANDROID_SDK_ROOT']
    java=args.java_home.resolve()/'bin/java.exe'
    bt=args.sdk.resolve()/'build-tools/35.0.0'
    def run(argv,cwd=ROOT):subprocess.run([str(v) for v in argv],cwd=cwd,check=True)
    def tool(*argv):run([java,'-Xmx1024m','-jar',args.apktool.resolve(),*argv])
    inputs=ROOT/'inputs';inputs.mkdir(exist_ok=True)
    provider=inputs/'provider.apk'
    if args.provider.resolve()!=provider.resolve():shutil.copy2(args.provider,provider)
    tool('d','-f',provider,'-o',ROOT/'car-resources','-p',ROOT/'framework')
    tool('d','-r','-f',provider,'-o',ROOT/'car-code','-p',ROOT/'framework')
    smali=ROOT/'car-code/smali'
    for path in smali.rglob('*.smali'):
        text=path.read_text('utf8').replace('androidx/','carxlibx/').replace('androidx.','carxlibx.')
        text=text.replace('android/support/','carolds/support/').replace('android.support.','carolds.support.')
        path.write_text(text,encoding='utf8',newline='\n')
    (smali/'androidx').rename(smali/'carxlibx')
    if (smali/'android/support').exists():
        (smali/'carolds').mkdir(exist_ok=True)
        (smali/'android/support').rename(smali/'carolds/support')
    patch_dependency()
    # Match the supplied Fresh edition's package without renaming engine classes.
    for path in smali.rglob('*.smali'):
        text=path.read_text('utf8')
        text=text.replace('"com.autonavi.amapauto"','"com.motonav.app.fresh"')
        text=text.replace('"com.autonavi.amapauto.provider.','"com.motonav.app.fresh.provider.')
        text=text.replace('"content://com.autonavi.amapauto.provider.','"content://com.motonav.app.fresh.provider.')
        path.write_text(text,encoding='utf8',newline='\n')
    run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',ROOT/'native-route/build.ps1',
         '-ZigPath',args.zig.resolve(),'-JavaHome',args.java_home.resolve()])
    # Local SDK paths and generated identities are ignored by Git.
    (ROOT/'local.properties').write_text('sdk.dir='+str(args.sdk.resolve()).replace('\\','/').replace(':','\\:')+'\n','utf8')
    flags=['--no-daemon','--max-workers=2','-Dorg.gradle.jvmargs=-Xmx1536m -Dfile.encoding=UTF-8','-Pkotlin.compiler.execution.strategy=in-process']
    if args.offline:flags.append('--offline')
    run([args.gradle.resolve(),*flags,'assembleDebug'])
    moto=ROOT/'app/build/outputs/apk/debug/app-debug.apk'
    tool('d','-r','-f',moto,'-o',ROOT/'moto-code','-p',ROOT/'framework')
    for name in ['moto-code','car-code']:tool('b',ROOT/name,'-o',ROOT/(name+'.apk'),'-p',ROOT/'framework')
    for name in ['prepare-merge.py','compile-manifest.py','pack.py']:run([sys.executable,ROOT/name])
    key=ROOT/'.local/debug.keystore';out=ROOT/'output';out.mkdir(exist_ok=True)
    aligned=out/'countdown-aligned.apk';final=out/f'MotoNav-{VERSION}-release.apk'
    run([bt/'zipalign.exe','-f','4',ROOT/'fusion-unsigned.apk',aligned])
    run([bt/'apksigner.bat','sign','--ks',key,'--ks-pass','pass:android','--key-pass','pass:android',
         '--ks-key-alias','androiddebugkey','--out',final,aligned])
    result=subprocess.check_output([str(bt/'apksigner.bat'),'verify','--print-certs',str(final)],cwd=ROOT).decode('utf8','replace')
    print(result)
    run([bt/'zipalign.exe','-c','4',final])
    (out/'Application-info.txt').write_text('Package: com.motonav.app.fresh\n'+result,'utf8')
    (out/'build-result.json').write_text(json.dumps({'version':VERSION,'package':'com.motonav.app.fresh',
        'apk':final.name,'sha256':sha(final.read_bytes()),'providerInputSHA256':spec['providerSHA256'],
        'privateKeysIncludedInSource':False,'deviceInstalled':False},indent=2)+'\n','utf8')
    print('Package: com.motonav.app.fresh; apply for your own AMap Android Key using the certificate SHA1 above.')
    print('Keep .local/debug.keystore private and reuse it for future updates. Do not share inputs/ or generated dependency code.')
    print(final)

if __name__=='__main__':main()
