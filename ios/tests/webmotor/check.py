from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'MotoNav/WebMotor.swift').read_text()
core=s[s.index('struct MotorPoint'):s.index('enum MotorRoutes')]
start=s.index('    static func parse(_ bytes: Data')
end=s.index('\n}\n\n/// Independent',start)
core+='enum MotorRoutes {\n'+s[start:end]+'\n}\n'
nav=(root/'MotoNav/NaviBridge.swift').read_text()
start=nav.index('    static func distM('); end=nav.index('\n    }',start)+6
core+='enum NaviBridge {\n'+nav[start:end]+'\n}\n'
core+=(root/'MotoNav/RouteGeometry.swift').read_text()+'\n'
core+=(root/'MotoNav/MotorRouteTracker.swift').read_text()+'\n'
core+=(root/'MotoNav/BleHandshakeClock.swift').read_text()+'\n'
share=(root/'MotoNav/AmapShare.swift').read_text()
share=share[:share.index('final class ShareGuide')]
share=share[:share.rfind('}')+1]
# Expose only the parser to this command-line harness, leaving app visibility unchanged.
core+=share.replace('private static func parse(', 'static func parse(').replace('private struct Share {', 'struct Share {')+'\n'
tests=r'''
func expectFailure(_ body: () throws -> Void) { do { try body(); fatalError("invalid input accepted") } catch {} }
let url = URL(string:"https://www.amap.com/ssr/dir?type=4&flat=30&flon=104&dlat=30.01&dlon=104.01&vlat=30.005&vlon=104.005")!
let req = try MotorRequest.parse(url)
precondition(req.vias.count == 1)
let roundTrip = try MotorRequest.parse(req.page)
precondition(roundTrip.end.lng == 104.01)
expectFailure { _ = try MotorRequest.parse(URL(string:"http://www.amap.com/ssr/dir?type=4")!) }
expectFailure { _ = try MotorRequest.parse(URL(string:"https://evil.example/ssr/dir?type=4")!) }
expectFailure { _ = try MotorRequest.parse(URL(string:"https://www.amap.com/ssr/dir?type=0&flat=30&flon=104&dlat=31&dlon=105")!) }
expectFailure { _ = try MotorRequest.parse(URL(string:"https://www.amap.com/ssr/dir?type=4&flat=nan&flon=104&dlat=31&dlon=105")!) }
let text = """
{"code":1,"data":[{"cost":{"duration":20},"steps":[{"polyline":"104,30;104.001,30.001","instruction":"前方终点","navi":{"action":"到达"}}]}]}
"""
let route = try MotorRoutes.parse(Data(text.utf8),request:req)[0]
precondition(route.points.count == 2 && route.steps[0].dir == "S")
precondition(!MotorArrival.confirmed(remaining:800,offset:0,endpointDistance:5))
precondition(!MotorArrival.confirmed(remaining:0,offset:100,endpointDistance:5))
precondition(!MotorArrival.confirmed(remaining:0,offset:0,endpointDistance:800))
precondition(MotorArrival.confirmed(remaining:0,offset:0,endpointDistance:0))
expectFailure { _ = try MotorRoutes.parse(Data("{}".utf8),request:req) }
expectFailure { _ = try MotorRoutes.parse(Data(text.replacingOccurrences(of:"104.001,30.001",with:"120,40").utf8),request:req) }
print("PASS motor URL validation, parser, continuity and arrival regression")
let bike = "amapuri://route/plan?t=3&rideType=bike&dev=0&slat=30&slon=104&dlat=30.01&dlon=104.01"
precondition(AmapShare.parse(bike)?.mode == "ridemap")
precondition(AmapShare.parse(bike.replacingOccurrences(of:"rideType=bike",with:"rideType=elebike")) == nil)
precondition(AmapShare.parse(bike.replacingOccurrences(of:"dev=0",with:"dev=1")) == nil)
precondition(AmapShare.parse(bike + "&vian=2") == nil)
print("PASS bicycle native share parser and electric bicycle rejection")
'''
tests+='\n'+(root/'tests/parity-015/main.swift').read_text()
core+=(root/'MotoNav/PhoneClockSync.swift').read_text()+'\n'
core+=(root/'MotoNav/MotorPlanHistory.swift').read_text()+'\n'
tests+='\n'+(root/'tests/parity-016/main.swift').read_text()
folder=Path(tempfile.mkdtemp(prefix='motonav-core-'))
(folder/'main.swift').write_text('import Foundation\n'+core+tests)
subprocess.run(['swiftc',str(folder/'main.swift'),'-o',str(folder/'test')],check=True)
subprocess.run([str(folder/'test')],check=True)
