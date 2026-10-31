import SwiftUI
import CoreLocation

enum IndependentMode:String,CaseIterable,Identifiable {
    case motorcycle="摩托车",bicycle="自行车"
    var id:String{rawValue}
}

final class IndependentPlanner:ObservableObject {
    @Published var start:MotorPoint?
    @Published var end:MotorPoint?
    @Published var vias:[MotorPoint]=[]
    @Published var routes:[MotorRoute]=[]
    @Published var selected=0
    @Published var busy=false
    @Published var message=""
    private var generation=UUID()
    private var task:Task<Void,Never>?
    func invalidate(){generation=UUID();task?.cancel();task=nil;routes=[];selected=0;busy=false}
    func load(_ entry:SavedMotorPlan){
        guard entry.valid else{return}
        invalidate();start=entry.request.start;end=entry.request.end;vias=entry.request.vias
        message="已恢复起终点和途经点，点击规划获取最新路线"
    }
    func plan(_ mode:IndependentMode){
        guard !WebMotorSession.shared.running,!NaviBridge.shared.navigating,!NaviBridge.shared.cruising else{message="请先结束当前导航或巡航";return}
        guard !Settings.shared.keysMissing else{message="请先填写自己的高德 iOS Key";return}
        let origin:MotorPoint
        if let start=start{origin=start}else{
            guard let fix=LocationService.shared.location,fix.horizontalAccuracy>=0,fix.horizontalAccuracy<=100,abs(fix.timestamp.timeIntervalSinceNow)<15 else{
                message="当前位置尚未就绪，请等待定位或手动选择起点";LocationService.shared.start();return
            }
            let g=MapGeo.wgsToGcj(fix.coordinate.latitude,fix.coordinate.longitude)
            origin=MotorPoint(lat:g.lat,lng:g.lng,name:"我的位置")
        }
        guard let end=end else{message="请选择目的地";return}
        invalidate();let token=generation
        let request=MotorRequest(start:origin,end:end,vias:mode == .bicycle ? []:vias,bicycle:mode == .bicycle)
        busy=true;message="正在规划\(mode.rawValue)路线…"
        task=Task { @MainActor [weak self] in
            do{
                let result=try await MotorRoutes.load(request)
                guard let self=self,self.generation==token,!Task.isCancelled else{return}
                self.routes=result;self.selected=0;self.busy=false
                self.message="已找到 \(result.count) 条路线，请选择方案"
                if let first=result.first{MotorPlanHistory.shared.save(first)}
            }catch{
                guard let self=self,self.generation==token,!Task.isCancelled else{return}
                self.busy=false;self.message=error.localizedDescription
            }
        }
    }
}

struct MotorLoginView:View {
    @StateObject private var browser=MotorBrowser()
    let page:URL?
    @Environment(\.dismiss) private var dismiss
    var body:some View {
        NavigationView{
            VStack{
                Text("仅在登录或验证时打开网页。登录保存在本机，正常规划可直接在 App 完成。").font(.caption).padding()
                MotorBrowserView(browser:browser)
                Text(ContentView.userMessage(browser.status)).font(.caption)
            }.navigationTitle("高德登录／验证").toolbar{ToolbarItem(placement:.confirmationAction){Button("完成"){dismiss()}}}
        }.onAppear{if let page=page{browser.web.load(URLRequest(url:page))}}
    }
}

struct IndependentPlannerView:View {
    @StateObject private var model=IndependentPlanner()
    @ObservedObject var guide=WebMotorSession.shared
    @State private var mode:IndependentMode
    @State private var pick:Int?
    @State private var login=false
    @State private var loaded=false
    @State private var waypoints=false
    @State private var showHistory=false
    @State private var simulationSpeed=60
    @Environment(\.dismiss) private var dismiss
    init(mode:IndependentMode){_mode=State(initialValue:mode)}
    var body:some View {
        NavigationView{
            ScrollView{
                VStack(alignment:.leading,spacing:16){
                    Picker("出行方式",selection:$mode){ForEach(IndependentMode.allCases){Text($0.rawValue).tag($0)}}.pickerStyle(.segmented)
                        .onChange(of:mode){_ in model.invalidate();model.message=""}
                    inputCard
                    HStack{
                        Button("路线历史"){showHistory=true};Spacer()
                        if mode == .motorcycle{Button("高德登录／验证"){login=true}}
                    }.font(.subheadline)
                    if !model.message.isEmpty{Text(ContentView.userMessage(model.message)).font(.footnote).foregroundColor(ContentView.sub)}
                    if let route=selectedRoute{routeOptions(route)}
                    Text(mode == .motorcycle ? "账号过期时，可在这里重新登录高德。":"自行车路线将按骑行方式规划。").font(.caption).foregroundColor(ContentView.sub)
                    Text("锁屏前请等待沿线底图准备；后台继续定位、语音和蓝牙，尚未准备的地图需回前台下载。").font(.caption).foregroundColor(ContentView.sub)
                }.padding(16)
            }.background(ContentView.bg).navigationTitle("\(mode.rawValue)路线")
                .toolbar{ToolbarItem(placement:.cancellationAction){Button("返回首页"){dismiss()}}}
        }.foregroundColor(.white).tint(ContentView.pink)
        .sheet(isPresented:Binding(get:{pick != nil},set:{if !$0{pick=nil}})){
            RoutePointPicker(title:pick==0 ? "选择起点":"选择目的地",near:model.start){point in
                if pick==0{model.start=point}else{model.end=point};model.invalidate();pick=nil
            }
        }
        .sheet(isPresented:$waypoints){RouteWaypointEditor(points:model.vias,near:model.start){points in model.vias=points;model.invalidate()}}
        .sheet(isPresented:$showHistory){MotorHistoryView(bicycle:mode == .bicycle){entry in model.load(entry)}}
        .sheet(isPresented:$login){MotorLoginView(page:nil)}
        .onAppear{
            guard !loaded else{return};loaded=true
            LocationService.shared.start()
            if let route=guide.route,route.request.bicycle == (mode == .bicycle),!guide.running{
                model.start=route.request.start;model.end=route.request.end;model.vias=route.request.vias;model.routes=[route]
            }
        }
    }
    private var selectedRoute:MotorRoute?{model.routes.indices.contains(model.selected) ? model.routes[model.selected]:nil}
    private var inputCard:some View {
        CardView{
            Button{pick=0}label:{Label(model.start?.name.isEmpty == false ? model.start!.name:"我的位置（点此更改起点）",systemImage:"location")}
            if model.start != nil{Button("使用当前位置"){model.start=nil;model.invalidate()}.font(.caption)}
            Divider()
            Button{pick=1}label:{Label(model.end?.name.isEmpty == false ? model.end!.name:"输入目的地",systemImage:"mappin.and.ellipse")}
            if let point=model.end{Button("收藏目的地"){Favorites.shared.add(SearchService.Place(name:point.name,address:"",lat:point.lat,lng:point.lng))}.font(.caption)}
            if mode == .motorcycle{
                Button{waypoints=true}label:{Label(model.vias.isEmpty ? "添加途经点（最多三个）":"途经："+model.vias.map{$0.name}.joined(separator:" → "),systemImage:"point.topleft.down.curvedto.point.bottomright.up")}
            }
            PillButton(title:model.busy ? "正在规划…":"规划\(mode.rawValue)路线",color:ContentView.pink,enabled:!model.busy && model.end != nil){model.plan(mode)}
        }
    }
    private func routeOptions(_ route:MotorRoute)->some View {
        VStack(alignment:.leading,spacing:14){
            MotorMap(route:route,position:nil,overview:true).frame(height:260).clipShape(RoundedRectangle(cornerRadius:18))
            ForEach(Array(model.routes.enumerated()),id:\.offset){i,r in
                Button{model.selected=i}label:{
                    HStack{
                        Image(systemName:model.selected==i ? "checkmark.circle.fill":"circle")
                        VStack(alignment:.leading,spacing:4){Text("方案 \(i+1) · \(NaviBridge.durText(r.seconds))").bold();Text(NaviBridge.distText(r.totalMeters)).font(.subheadline)}
                        Spacer()
                    }.padding(14).background(RoundedRectangle(cornerRadius:14).fill(ContentView.card))
                }
            }
            Picker("模拟速度",selection:$simulationSpeed){ForEach(NavigationSpeed.values,id:\.self){Text("\($0) km/h").tag($0)}}.pickerStyle(.segmented)
            HStack{
                PillButton(title:"模拟导航",color:.purple,enabled:!model.busy){begin(route,true)}
                PillButton(title:"开始导航",color:ContentView.pink,enabled:!model.busy){begin(route,false)}
            }
        }
    }
    private func begin(_ route:MotorRoute,_ simulate:Bool){
        guard !NaviBridge.shared.navigating,!NaviBridge.shared.cruising,!guide.running else{model.message="请先结束当前导航或巡航";return}
        History.shared.add(SearchService.Place(name:route.request.end.name,address:"",lat:route.request.end.lat,lng:route.request.end.lng))
        MotorPlanHistory.shared.save(route)
        guide.start(route,simulate:simulate,simulationSpeed:simulationSpeed)
    }
}

struct IndependentNavigationView: View {
    @ObservedObject var guide=WebMotorSession.shared
    @ObservedObject var settings=Settings.shared
    @ObservedObject var metadata=WebRouteMetadata.shared
    @Environment(\.dismiss) private var dismiss
    @State private var confirmExit=false
    var body: some View {
        ZStack(alignment:.top){
            if let route=guide.route {MotorMap(route:route,position:guide.position,overview:guide.overview).ignoresSafeArea()}
            VStack(spacing:12){
                HStack(alignment:.center,spacing:18){
                    Image(systemName:arrow).font(.system(size:48,weight:.bold)).foregroundColor(ContentView.pink).frame(width:58)
                    VStack(alignment:.leading,spacing:6){
                        Text(NaviBridge.distText(guide.turnMetres)).font(.system(size:30,weight:.heavy))
                        Text(guide.instruction).font(.headline).lineLimit(2)
                        if !navigationNotice.isEmpty {Text(navigationNotice).font(.caption).foregroundColor(ContentView.sub)}
                    };Spacer(minLength:0)
                }.padding(18).background(RoundedRectangle(cornerRadius:22).fill(ContentView.bg.opacity(0.94)))
                Spacer()
                VStack(spacing:12){
                    HStack{
                        Text("\(guide.speed)").font(.system(size:32,weight:.bold))+Text(" km/h").font(.caption)
                        Spacer()
                        VStack(alignment:.trailing){Text("剩余 \(NaviBridge.distText(guide.remaining))").bold();Text("约 \(NaviBridge.durText(guide.remainingSeconds))").font(.caption)}
                    }
                    HStack{
                        Button("全览／跟随"){guide.overview.toggle()};Spacer()
                        Button(settings.voiceMuted ? "取消静音" : "静音"){settings.voiceMuted.toggle();if settings.voiceMuted{Speech.shared.stopNow()}}
                        Spacer();Button("返回设置"){dismiss()}
                    }.font(.subheadline)
                    if guide.simulation {
                        HStack{Button(guide.paused ? "继续" : "暂停"){guide.paused.toggle()};Spacer();Button("\(guide.speed) km/h"){guide.speed=NavigationSpeed.next(guide.speed)}}
                    }
                    Button("结束导航"){confirmExit=true}.foregroundColor(.red)
                }.padding(18).background(RoundedRectangle(cornerRadius:22).fill(ContentView.bg.opacity(0.94)))
            }.padding(16)
            if guide.arrived {
                VStack(spacing:18){Text("已到达目的地").font(.title.bold());Button("返回首页"){guide.arrived=false;dismiss()}}
                    .padding(30).background(RoundedRectangle(cornerRadius:22).fill(ContentView.bg)).frame(maxHeight:.infinity)
            }
        }.foregroundColor(.white).tint(ContentView.pink)
        .alert("结束导航？",isPresented:$confirmExit){Button("结束",role:.destructive){guide.stop();guide.arrived=false;dismiss()};Button("继续导航",role:.cancel){}}
    }
    private var navigationNotice:String {
        if guide.status.contains("重算失败") {return "重新规划暂未完成，将保留当前路线"}
        if guide.status.contains("正在重新规划") {return "偏离路线，正在重新规划…"}
        if guide.status.contains("已偏离路线") {return "已偏离路线"}
        if guide.status.contains("GPS") {return "定位信号较弱"}
        return guide.simulation ? "模拟导航":""
    }
    private var arrow:String {
        switch guide.turnDirection {case "L":return "arrow.turn.up.left";case "R":return "arrow.turn.up.right";case "SL":return "arrow.up.left";case "SR":return "arrow.up.right";case "U":return "arrow.uturn.down";default:return "arrow.up"}
    }
}
