import SwiftUI

struct RoutePointPicker: View {
    let title: String
    let near: MotorPoint?
    let choose: (MotorPoint)->Void
    @StateObject private var search=SearchService()
    @ObservedObject private var favorites=Favorites.shared
    @State private var query=""
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        NavigationView {
            List {
                Section {
                    HStack {
                        TextField("输入地点名称",text:$query).submitLabel(.search).onSubmit{runSearch()}
                        Button("搜索"){runSearch()}
                    }
                    Button("搜索附近加油站"){query="加油站";search.searchFuel(near:near.map{($0.lat,$0.lng)})}
                    if !search.status.isEmpty{Text(search.status).font(.caption)}
                }
                if !search.results.isEmpty {
                    Section("搜索结果") {ForEach(search.results){p in pointRow(p)}}
                } else if query.isEmpty {
                    if !favorites.places.isEmpty {Section("常用地点"){ForEach(favorites.places){p in pointRow(p.searchPlace)}}}
                    Section("最近目的地"){ForEach(History.shared.places){p in pointRow(p)}}
                }
            }.navigationTitle(title).toolbar{ToolbarItem(placement:.cancellationAction){Button("取消"){dismiss()}}}
        }.preferredColorScheme(.dark)
    }
    private func runSearch(){search.search(query,near:near.map{($0.lat,$0.lng)})}
    private func pointRow(_ p: SearchService.Place)->some View {
        HStack {
            Button {choose(MotorPoint(lat:p.lat,lng:p.lng,name:p.name));dismiss()} label:{
                VStack(alignment:.leading,spacing:4){Text(p.name);Text(p.address).font(.caption).foregroundColor(.secondary)}
            }
            Spacer()
            Button{favorites.add(p)}label:{Image(systemName:"star")}.buttonStyle(.borderless).accessibilityLabel("收藏\(p.name)")
        }
    }
}

struct RouteWaypointEditor: View {
    private struct Pick: Identifiable {let id=UUID();let index:Int}
    let near: MotorPoint?
    let apply: ([MotorPoint])->Void
    @State private var draft: [MotorPoint]
    @State private var pick: Pick?
    @Environment(\.dismiss) private var dismiss
    init(points:[MotorPoint],near:MotorPoint?,apply:@escaping([MotorPoint])->Void){
        self.near=near;self.apply=apply;_draft=State(initialValue:points)
    }
    var body:some View {
        NavigationView {
            List {
                Section("按顺序经过，最多三个") {
                    ForEach(draft.indices,id:\.self){i in
                        VStack(alignment:.leading,spacing:10){
                            Button("途经 \(i+1)：\(draft[i].name.isEmpty ? "已选地点":draft[i].name)"){pick=Pick(index:i)}
                            HStack {
                                Button("上移"){move(i,-1)}.disabled(i==0)
                                Button("下移"){move(i,1)}.disabled(i==draft.count-1)
                                Spacer()
                                Button("删除",role:.destructive){draft.remove(at:i)}
                            }.buttonStyle(.borderless).font(.caption)
                        }.padding(.vertical,5)
                    }
                    if draft.count<3{Button("＋ 添加途经点"){pick=Pick(index:draft.count)}}
                }
                Text("点击地点可替换；只有点“应用”才会改变路线，取消保留原设置。").font(.caption).foregroundColor(.secondary)
            }.navigationTitle("编辑途经点").toolbar {
                ToolbarItem(placement:.cancellationAction){Button("取消"){dismiss()}}
                ToolbarItem(placement:.confirmationAction){Button("应用"){apply(draft);dismiss()}}
            }
        }.sheet(item:$pick){target in
            RoutePointPicker(title:"选择途经点",near:near){point in
                if target.index<draft.count{draft[target.index]=point}else if draft.count<3{draft.append(point)}
                pick=nil
            }
        }.preferredColorScheme(.dark)
    }
    private func move(_ i:Int,_ delta:Int){let j=i+delta;if draft.indices.contains(i) && draft.indices.contains(j){draft.swapAt(i,j)}}
}

struct MotorHistoryView:View {
    let bicycle:Bool
    let choose:(SavedMotorPlan)->Void
    @ObservedObject private var history=MotorPlanHistory.shared
    @State private var confirmClear=false
    @Environment(\.dismiss) private var dismiss
    private var plans:[SavedMotorPlan]{history.plans.filter{$0.request.bicycle==bicycle}}
    var body:some View {
        NavigationView {
            List {
                if plans.isEmpty{Text("成功规划后会保存起点、终点和途经点")}
                ForEach(plans){entry in
                    Button{choose(entry);dismiss()}label:{
                        VStack(alignment:.leading,spacing:5){
                            Text("\(entry.request.start.name.isEmpty ? "起点":entry.request.start.name) → \(entry.request.end.name.isEmpty ? "目的地":entry.request.end.name)")
                            if !entry.request.vias.isEmpty{Text("途经："+entry.request.vias.map{$0.name}.joined(separator:" → ")).font(.caption)}
                            Text("\(NaviBridge.distText(entry.distance)) · \(NaviBridge.durText(entry.seconds))").font(.caption).foregroundColor(.secondary)
                        }
                    }.swipeActions{Button("删除",role:.destructive){history.remove(entry.id)}}
                }
            }.navigationTitle(bicycle ? "自行车路线历史":"摩托车路线历史").toolbar {
                ToolbarItem(placement:.cancellationAction){Button("关闭"){dismiss()}}
                ToolbarItem(placement:.confirmationAction){Button("清空"){confirmClear=true}.disabled(plans.isEmpty)}
            }
        }.confirmationDialog("清空当前类型的路线历史？",isPresented:$confirmClear,titleVisibility:.visible){
            Button("清空路线历史",role:.destructive){history.clear(bicycle:bicycle)}
            Button("取消",role:.cancel){}
        }.preferredColorScheme(.dark)
    }
}
