var clock=PhoneClockSync()
let epoch=1793433600.0
let firstClock=clock.message(wall:epoch,uptime:10,offset:28800)!
precondition(firstClock=="#TIME,1793433600,28800")
precondition(clock.message(wall:epoch+1,uptime:11,offset:28800)==nil)
let secondClock=clock.message(wall:epoch+2,uptime:12,offset:28800)!
precondition(secondClock=="#TIME,1793433602,28800")
precondition(clock.message(wall:epoch+3,uptime:13,offset:28800)==nil)
let thirdClock=clock.message(wall:epoch+4,uptime:14,offset:28800)!
precondition(thirdClock=="#TIME,1793433604,28800")
precondition(clock.message(wall:epoch+63,uptime:73,offset:28800)==nil)
precondition(clock.message(wall:epoch+64,uptime:74,offset:28800)=="#TIME,1793433664,28800")
precondition(clock.message(wall:epoch+65,uptime:75,offset:32400) != nil)
precondition(clock.message(wall:epoch+600,uptime:76,offset:32400) != nil)
precondition(clock.message(wall:epoch+601,uptime:77,offset:32400,force:true) != nil)
try ([firstClock,secondClock,thirdClock].joined(separator:"\n")+"\n").write(toFile:"/tmp/motonav-clock-wires.txt",atomically:true,encoding:.utf8)
precondition(clock.message(wall:Double.nan,uptime:100,offset:0)==nil)
precondition(clock.message(wall:946684800,uptime:100,offset:0)==nil)
precondition(clock.message(wall:epoch,uptime:100,offset:86400)==nil)
precondition(BleAuthenticatedReply.accepts("U,1") && BleAuthenticatedReply.accepts("CAP,HID,1"))
precondition(!BleAuthenticatedReply.accepts("U,0") && !BleAuthenticatedReply.accepts("CAP,HID,0"))
var reconnected=PhoneClockSync()
precondition(reconnected.message(wall:epoch+700,uptime:170,offset:28800) == "#TIME,1793434300,28800")
precondition(NavigationSpeed.values == [60,100,120])
precondition(NavigationSpeed.next(60)==100 && NavigationSpeed.next(100)==120 && NavigationSpeed.next(120)==60)
print("PASS initial clock retries, UTC wire, firmware ranges, lost-U capability evidence, periodic sync, timezone, reconnect and speeds")

let suite="MotoNavTests-"+UUID().uuidString
let defaults=UserDefaults(suiteName:suite)!
defer{defaults.removePersistentDomain(forName:suite)}
let history=MotorPlanHistory(defaults:defaults)
let s=MotorPoint(lat:30,lng:104,name:"起点"),e=MotorPoint(lat:30.1,lng:104.1,name:"终点")
let v1=MotorPoint(lat:30.02,lng:104.02,name:"加油站"),v2=MotorPoint(lat:30.06,lng:104.06,name:"途经二")
let savedRequest=MotorRequest(start:s,end:e,vias:[v1,v2])
let savedRoute=MotorRoute(request:savedRequest,points:[s,v1,v2,e],steps:[MotorStep(instruction:"直行",road:"A",action:"直行",end:3)],seconds:100)
history.save(savedRoute)
precondition(history.plans.count==1 && history.plans[0].request.vias.map{$0.name} == ["加油站","途经二"])
let reload=MotorPlanHistory(defaults:defaults)
precondition(reload.plans.count==1 && reload.plans[0].request.start.lat==30)
history.save(savedRoute);precondition(history.plans.count==1)
let reversedRequest=MotorRequest(start:s,end:e,vias:[v2,v1])
history.save(MotorRoute(request:reversedRequest,points:[s,v2,v1,e],steps:savedRoute.steps,seconds:120))
precondition(history.plans.count==2 && history.plans[0].request.vias[0].name=="途经二")
for i in 0..<25 {
    let dest=MotorPoint(lat:30.1+Double(i)/10000,lng:104.1)
    history.save(MotorRoute(request:MotorRequest(start:s,end:dest),points:[s,dest],steps:[MotorStep(instruction:"",road:"",action:"",end:1)],seconds:100))
}
precondition(history.plans.count==20)
history.clear(bicycle:false);precondition(history.plans.isEmpty)
defaults.set(Data("corrupted".utf8),forKey:"motor_plan_history_v1")
precondition(MotorPlanHistory(defaults:defaults).plans.isEmpty)
print("PASS history persistence, full ordered vias, deduplication, 20-plan limit, clear and corruption handling")

func cookie(_ domain:String,_ path:String="/",expires:Date?=nil)->HTTPCookie {
    var properties:[HTTPCookiePropertyKey:Any]=[.domain:domain,.path:path,.name:"session",.value:"test-only"]
    if let expires=expires{properties[.expires]=expires}
    return HTTPCookie(properties:properties)!
}
let scoped=[cookie(".amap.com"),cookie("evil.example")]
precondition(ShareCookiePolicy.header(scoped,url:URL(string:"https://www.amap.com/ssr/dir")!)=="session=test-only")
precondition(ShareCookiePolicy.header(scoped,url:URL(string:"https://evil.example/")!)==nil)
precondition(ShareCookiePolicy.header(scoped,url:URL(string:"https://amap.com.evil.example/")!)==nil)
precondition(ShareCookiePolicy.header(scoped,url:URL(string:"http://www.amap.com/")!)==nil)
precondition(ShareCookiePolicy.header([cookie("www.amap.com","/account")],url:URL(string:"https://www.amap.com/accounting")!)==nil)
precondition(ShareCookiePolicy.header([cookie("www.amap.com",expires:Date(timeIntervalSinceNow:-5))],url:URL(string:"https://www.amap.com/")!)==nil)
precondition(!ShareCookiePolicy.allowed(URL(string:"https://user:password@www.amap.com/")!))
print("PASS share cookies: domain/path/expiry/TLS isolation and malicious redirect targets")
