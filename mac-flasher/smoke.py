from pathlib import Path
import hashlib,tempfile,subprocess,tarfile,platform,json,shutil,os
source=Path(__file__).parent
expected={
 'arm64':('08a58829a3f14bb08f2e39f4cad48dcb14b7000384268e7f06689836332d6e25','fe300417054386e8c15f916216924accb05d0132411bfce3b223ad621f0de6d9'),
 'amd64':('23921776a7df75f2e6f3ca10a604d74db4661e1b8a0ebb89bdddcd2d26f37055','4dd42149be48880fd066fbf8a0f0ac16d38bfcf652451c6491e2b55bc5722c5e')}
def check(ok,message):
 if not ok:raise AssertionError(message)
def checksums(root):
 paths=[p for p in root.rglob('*') if p.is_file() and p.name not in ['SHA256SUMS.txt','esptool.tar.gz','write.log']]
 (root/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(root).as_posix()+'\n' for p in sorted(paths)))
check(platform.system()=='Darwin','Mac runner required')
subprocess.run(['/bin/zsh','-n',str(source/'flash.command')],check=True)
manifest=json.loads((source/'firmware/manifest.json').read_text())
check(manifest['firmware']=='7.12.10-pages.5','wrong firmware version')
with tempfile.TemporaryDirectory(prefix='MotoNav Mac 圆屏测试 ') as temp:
 root=Path(temp);arch='arm64' if platform.machine()=='arm64' else 'amd64'
 archive=root/'esptool.tar.gz'
 subprocess.run(['curl','--fail','--location','--retry','3','--silent','--show-error',f'https://github.com/espressif/esptool/releases/download/v5.6.0/esptool-v5.6.0-macos-{arch}.tar.gz','--output',str(archive)],check=True)
 check(hashlib.sha256(archive.read_bytes()).hexdigest()==expected[arch][0],'official archive hash')
 tool=root/'tools'/f'macos-{arch}'/'esptool';tool.parent.mkdir(parents=True)
 with tarfile.open(archive) as tar:tool.write_bytes(tar.extractfile(f'esptool-macos-{arch}/esptool').read())
 check(hashlib.sha256(tool.read_bytes()).hexdigest()==expected[arch][1],'native tool hash');tool.chmod(0o755)
 shutil.copytree(source/'firmware',root/'firmware')
 for e in manifest['files']:
  b=(root/'firmware'/e['file']).read_bytes();check(len(b)==e['bytes'] and hashlib.sha256(b).hexdigest()==e['sha256'],'firmware integrity')
 app=next((root/'firmware').glob('*_app_0x10000.bin'));full=next((root/'firmware').glob('*_full_0x0.bin'))
 check(full.read_bytes()[0x10000:]==app.read_bytes(),'application offset')
 script=root/'一键烧录.command';script.write_bytes((source/'flash.command').read_bytes());script.chmod(0o755)
 checksums(root)
 native=subprocess.run([str(script),'--check'],capture_output=True,text=True,timeout=90)
 check(native.returncode==0 and '7.12.10-pages.5' in native.stdout and '未执行烧录' in native.stdout,'native check failed: '+native.stdout+native.stderr)
 help_text=subprocess.run([str(tool),'--chip','esp32s3','--before','default-reset','--after','hard-reset','write-flash','--help'],capture_output=True,text=True,check=True).stdout
 check('--erase-all' in help_text and '--flash-size' in help_text,'unsupported write flags')
 original=app.read_bytes();app.write_bytes(b'corrupt')
 bad=subprocess.run([str(script),'--check'],capture_output=True,text=True,timeout=30)
 check(bad.returncode!=0 and '文件校验失败' in bad.stderr,'corrupt firmware accepted');app.write_bytes(original)
 # Exercise the real launcher with a fake tool and paths in a separate copy.
 # Never run a hardware-write branch using the native esptool in CI.
 mock=root/'mock-flow';mock.mkdir();shutil.copytree(root/'firmware',mock/'firmware')
 dev=mock/'fake-dev';dev.mkdir();(dev/'cu.usbmodemTest').touch()
 launcher=mock/'一键烧录.command'
 launcher.write_text(script.read_text().replace('/dev/','"${task_root}/fake-dev/"'));launcher.chmod(0o755)
 fake='#!/bin/zsh\nif [[ "$1" == version ]]; then print "esptool v5.6.0 mock"; exit 0; fi\nprintf "%s\\n" "$@" > "$MAC_FLASH_TEST_LOG"\nexit "${MAC_FLASH_TEST_EXIT:-0}"\n'
 for cpu in ['arm64','amd64']:
  p=mock/'tools'/f'macos-{cpu}'/'esptool';p.parent.mkdir(parents=True);p.write_text(fake);p.chmod(0o755)
 checksums(mock)
 record=mock/'write.log';env={**os.environ,'MAC_FLASH_TEST_LOG':str(record)}
 def flow(args,answer,code=0,exit_code='0'):
  record.unlink(missing_ok=True)
  r=subprocess.run([str(launcher),*args],input=answer,capture_output=True,text=True,env={**env,'MAC_FLASH_TEST_EXIT':exit_code},timeout=30)
  check(r.returncode==code,'launcher flow failed: '+r.stdout+r.stderr)
  return record.read_text().splitlines() if record.exists() else [],r
 args,_=flow([], '\n');check(args[-2:]==['0x10000',str(mock/'firmware'/app.name)] and '--erase-all' not in args,'default upgrade erases or wrong offset')
 args,_=flow(['--full'],'INSTALL\n');check('--erase-all' in args and args[-2:]==['0x0',str(mock/'firmware'/full.name)],'full install flag/offset')
 args,_=flow(['--full'],'NO\n');check(not args,'cancelled full install wrote')
 args,_=flow([], '3\n');check(not args,'cancel wrote')
 args,_=flow(['--port','/dev/cu.NOT_A_DEVICE'],'',code=1);check(not args,'invalid port wrote')
 _,failed=flow([], '\n',code=1,exit_code='1');check('烧录未完成' in failed.stderr and '烧录成功' not in failed.stdout,'write error reported success')
 result={'system':platform.system(),'os':platform.mac_ver()[0],'arch':arch,'nativeEsptool':'5.6.0','firmware':'7.12.10-pages.5','nativeCheck':True,'unicodeAndSpacedPath':True,'corruptFileRejected':True,'mockDefaultUpgradePreservesSettings':True,'mockFullInstallNeedsConfirmation':True,'mockCancelAndInvalidPortDoNotWrite':True,'mockFailureNotReportedSuccess':True,'deviceFlashed':False}
 print('PASS: '+json.dumps(result,ensure_ascii=False))
