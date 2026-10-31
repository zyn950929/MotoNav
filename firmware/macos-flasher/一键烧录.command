#!/bin/zsh
# Double-click in Finder. Firmware and the native macOS esptool are bundled.
set -u
task_root="${0:A:h}"
task_check=0
task_full=0
task_port=""
task_version="7.12.10-pages.5"
finish() {
  local task_code="$1"
  if [[ -t 0 && "$task_check" == 0 ]]; then
    read "?按回车关闭窗口…"
  fi
  exit "$task_code"
}
fail() {
  print -u2 -- "$1"
  finish 1
}
while (( $# )); do
  case "$1" in
    --check) task_check=1 ;;
    --full) task_full=1 ;;
    --port)
      (( $# >= 2 )) || fail "--port 后缺少串口名称。"
      shift; task_port="$1" ;;
    *) fail "无法识别的参数：$1" ;;
  esac
  shift
done
[[ "$(/usr/bin/uname -s)" == Darwin ]] || fail "本包用于 Mac 电脑，请不要在 Windows 或 iPhone 上运行。"
cd -- "$task_root" || fail "无法打开烧录包目录。"
print -- "MotoNav ${task_version} · 微雪 ESP32-S3-Touch-AMOLED-1.75"
print -- "只适用于 1.75 英寸型号；1.75C、1.85B 和 M5Stack 请使用各自固件。"
/usr/bin/shasum -a 256 -c SHA256SUMS.txt >/dev/null || fail "文件校验失败，请重新解压完整烧录包。"
task_arch="$(/usr/bin/uname -m)"
if [[ "$(/usr/sbin/sysctl -n hw.optional.arm64 2>/dev/null || true)" == 1 ]]; then task_arch=arm64; fi
case "$task_arch" in
  arm64) task_arch=arm64 ;;
  x86_64) task_arch=amd64 ;;
  *) fail "暂不支持此 Mac 芯片：$task_arch" ;;
esac
task_tool="$task_root/tools/macos-${task_arch}/esptool"
[[ -x "$task_tool" ]] || /bin/chmod u+x "$task_tool" || fail "无法设置烧录工具运行权限。"
"$task_tool" version || fail "烧录工具未能启动。若 macOS 拦截，请在系统设置的隐私与安全性中查看提示。"
task_ports=(/dev/cu.usbmodem*(N) /dev/cu.usbserial*(N) /dev/cu.SLAB_USBtoUART*(N) /dev/cu.wchusbserial*(N) /dev/cu.wchusbmodem*(N))
task_ports=( ${(u)task_ports} )
if (( task_check )); then
  print -- "固件、工具校验通过，未执行烧录。"
  if (( ${#task_ports} )); then
    print -l -- "检测到 USB 串口：" "${task_ports[@]}"
  else
    print -- "当前未检测到 USB 串口；连接圆屏后再运行即可。"
  fi
  finish 0
fi
if [[ -z "$task_port" ]]; then
  (( ${#task_ports} )) || fail "未检测到圆屏。请使用 USB 数据线连接，并关闭占用串口的软件。"
  if (( ${#task_ports} == 1 )); then
    task_port="${task_ports[1]}"
  else
    print -- "请选择圆屏串口："
    for (( task_i=1; task_i<=${#task_ports}; task_i++ )); do print -- "${task_i}. ${task_ports[$task_i]}"; done
    read "task_choice?输入串口编号：" || finish 1
    [[ "$task_choice" == <-> ]] || fail "无效的串口编号，未烧录。"
    (( task_choice>=1 && task_choice<=${#task_ports} )) || fail "串口编号超出范围，未烧录。"
    task_port="${task_ports[$task_choice]}"
  fi
else
  (( ${task_ports[(Ie)$task_port]} )) || fail "指定串口不存在或不是当前 USB 串口：$task_port"
fi
if (( ! task_full )); then
  print -- "1. 普通升级（默认）：更新现有 MotoNav，保留配对和设置。"
  print -- "2. 首次完整安装：刷出厂小智或其他旧固件，清除全部设置。"
  print -- "3. 取消"
  read "task_mode?输入 1/2/3，直接回车为普通升级：" || finish 1
  case "$task_mode" in
    ""|1) ;;
    2) task_full=1 ;;
    3) finish 0 ;;
    *) fail "选项无效，未烧录。" ;;
  esac
fi
task_image="$task_root/firmware/MotoNav_universal_1.75_v${task_version}_panel0_app_0x10000.bin"
task_offset=0x10000
task_erase=()
if (( task_full )); then
  print -- "首次完整安装会擦除整片闪存，包括原固件、配对和所有设置。"
  read "task_confirm?输入 INSTALL 确认，其他输入取消：" || finish 1
  [[ "$task_confirm" == INSTALL ]] || finish 0
  task_image="$task_root/firmware/MotoNav_universal_1.75_v${task_version}_panel0_full_0x0.bin"
  task_offset=0x0
  task_erase=(--erase-all)
else
  print -- "普通升级写入 0x10000。要求设备已有 MotoNav 的 16MB 分区布局。"
fi
print -- "串口：$task_port"
print -- "开始烧录，请保持 USB 连接…"
if "$task_tool" --chip esp32s3 --port "$task_port" --baud 460800 --before default-reset --after hard-reset write-flash --flash-size 16MB "${task_erase[@]}" "$task_offset" "$task_image"; then
  print -- "烧录成功，圆屏将重启。请核对屏幕版本 ${task_version}。"
  finish 0
else
  print -u2 -- "烧录未完成。若一直停在 Connecting，请按住 BOOT、短按 RESET，再松开 BOOT 后重试。"
  finish 1
fi
