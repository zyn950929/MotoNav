"""Run the signal/route policy regressions with JDK 17 (no APK or device needed)."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parents[1]
source = root / 'app/src/main/java/com/yuminaga/motonav'
output = root / '.local/policy-tests'
output.mkdir(parents=True, exist_ok=True)
suffix = '.exe' if os.name == 'nt' else ''
java_home = os.environ.get('JAVA_HOME')
def java_tool(name):
    return str(Path(java_home) / 'bin' / (name + suffix)) if java_home else name
names = ['TrafficCountdown', 'TrafficSignalState', 'TrafficJunctionState',
         'TrafficSignalPacket', 'TrafficSignalBatchPacket', 'TrafficSignalPolicy',
         'TrafficSignalWrite', 'TrafficSignalOutputDelay', 'CarSignalAlignment',
         'RouteAnchorSelector', 'MotorTrack', 'RouteCameraIndex', 'RouteRoadIndex', 'RouteSignalIndex', 'WebRouteAnchors', 'PhoneClockWire', 'MapPoseUpdates', 'DestinationRequestLedger', 'SimulationSpeedPolicy', 'NativeSimulationDisplay', 'SingleEngineLifecycle', 'NativeSignalGeometry', 'SharedRouteComparison', 'ProviderStartup', 'ProviderEngineState', 'ArrivalPolicy', 'AmapBuildings', 'RasterPacket', 'BleSession', 'CoverPacket', 'CrossPacket', 'MediaBitmapCapabilities']
tests = sorted(set((root / 'tests').glob('Test*.java')) | set((root / 'tests').glob('*Test.java')))
subprocess.run([java_tool('javac'), '-encoding', 'UTF-8', '-d', str(output),
                *[str(source / (name + '.java')) for name in names],
                *[str(test) for test in tests]], check=True)
for test in tests:
    subprocess.run([java_tool('java'), '-cp', str(output), ('com.yuminaga.motonav.' if 'package com.yuminaga.motonav;' in test.read_text('utf8') else '')+test.stem], check=True)
