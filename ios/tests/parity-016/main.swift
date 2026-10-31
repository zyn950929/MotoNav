var clock=PhoneClockSync()
let epoch=1793433600.0
precondition(clock.message(wall:epoch,uptime:10,offset:28800)=="#TIME,1793433600,28800")
precondition(clock.message(wall:epoch+59,uptime:69,offset:28800)==nil)
precondition(clock.message(wall:epoch+60,uptime:70,offset:28800)=="#TIME,1793433660,28800")
precondition(clock.message(wall:epoch+61,uptime:71,offset:32400) != nil)
precondition(clock.message(wall:epoch+600,uptime:72,offset:32400) != nil)
precondition(clock.message(wall:epoch+601,uptime:73,offset:32400,force:true) != nil)
precondition(clock.message(wall:Double.nan,uptime:100,offset:0)==nil)
var reconnected=PhoneClockSync()
precondition(reconnected.message(wall:epoch+700,uptime:170,offset:28800) == "#TIME,1793434300,28800")
precondition(NavigationSpeed.values == [60,100,120])
precondition(NavigationSpeed.next(60)==100 && NavigationSpeed.next(100)==120 && NavigationSpeed.next(120)==60)
print("PASS clock throttling, UTC wire, timezone change, manual clock change, reconnect and simulation speeds")

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
