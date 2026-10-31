from pathlib import Path
import hashlib, tempfile, subprocess, tarfile, platform

source=Path(__file__).parent
expected={
    'arm64':('08a58829a3f14bb08f2e39f4cad48dcb14b7000384268e7f06689836332d6e25','fe300417054386e8c15f916216924accb05d0132411bfce3b223ad621f0de6d9'),
    'amd64':('23921776a7df75f2e6f3ca10a604d74db4661e1b8a0ebb89bdddcd2d26f37055','4dd42149be48880fd066fbf8a0f0ac16d38bfcf652451c6491e2b55bc5722c5e')
}
firmware={
    'MotoNav_universal_1.75_v7.12.10-pages.4_panel0_app_0x10000.bin':'6ee31c480882f3197ba620d4f9d155bf74ef1538709644af8fe25f172b6da0b3',
    'MotoNav_universal_1.75_v7.12.10-pages.4_panel0_full_0x0.bin':'fc8d55d57d8d3f3611e3849db7860472127243f393ffb5660fc7a263565e2210'
}
def download(url,path,digest):
    subprocess.run(['curl','--fail','--location','--retry','3','--silent','--show-error',url,'--output',str(path)],check=True)
    assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,path.name

assert platform.system()=='Darwin'
subprocess.run(['/bin/zsh','-n',str(source/'flash.command')],check=True)
with tempfile.TemporaryDirectory(prefix='MotoNav Mac 测试 ') as temp:
    root=Path(temp)
    arch='arm64' if platform.machine()=='arm64' else 'amd64'
    archive=root/'esptool.tar.gz'
    download(f'https://github.com/espressif/esptool/releases/download/v5.6.0/esptool-v5.6.0-macos-{arch}.tar.gz',archive,expected[arch][0])
    tool=root/'tools'/f'macos-{arch}'/'esptool';tool.parent.mkdir(parents=True)
    with tarfile.open(archive) as tar:
        tool.write_bytes(tar.extractfile(f'esptool-macos-{arch}/esptool').read())
    assert hashlib.sha256(tool.read_bytes()).hexdigest()==expected[arch][1]
    tool.chmod(0o755)
    (root/'firmware').mkdir()
    for name,digest in firmware.items():
        download(f'https://github.com/zyn950929/MotoNav/releases/download/firmware-v7.12.10-pages.4/{name}',root/'firmware'/name,digest)
    app=(root/'firmware'/next(iter(firmware))).read_bytes()
    full=(root/'firmware'/list(firmware)[1]).read_bytes()
    assert full[0x10000:]==app
    script=root/'一键烧录.command';script.write_bytes((source/'flash.command').read_bytes());script.chmod(0o755)
    checks=[f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(root).as_posix()}' for p in root.rglob('*') if p.is_file() and p!=archive]
    (root/'SHA256SUMS.txt').write_text('\n'.join(checks)+'\n')
    subprocess.run([str(script),'--check'],check=True,timeout=60)
    help_text=subprocess.run([str(tool),'write-flash','--help'],capture_output=True,text=True,check=True).stdout
    assert '--erase-all' in help_text
    # Tampering must stop before any tool can write a device.
    (root/'firmware'/next(iter(firmware))).write_bytes(b'corrupt')
    bad=subprocess.run([str(script),'--check'],capture_output=True,text=True,timeout=60)
    assert bad.returncode!=0 and '文件校验失败' in bad.stderr
    print(f'PASS: macOS {platform.mac_ver()[0]} {arch}, zsh syntax, native esptool v5.6.0, firmware hashes, Unicode/spaced path, no-device check, corrupt-file rejection. No device flashed.')
