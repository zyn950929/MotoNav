import Foundation
import Combine

/// Only user-selected planning inputs; never saves login state or substitutes an old geometry.
struct SavedMotorPlan: Codable,Identifiable {
    var id=UUID()
    let date: Date
    let request: MotorRequest
    let distance: Int
    let seconds: Int
    var valid: Bool {
        MotorRequest.valid(request.start.lat,request.start.lng) && MotorRequest.valid(request.end.lat,request.end.lng) &&
        request.vias.count<=3 && request.vias.allSatisfy{MotorRequest.valid($0.lat,$0.lng)} &&
        (!request.bicycle || request.vias.isEmpty) && distance>=0 && seconds>=0
    }
    var key: String {
        ([request.bicycle ? "bicycle":"motorcycle"]+([request.start]+request.vias+[request.end]).map{
            String(format:"%.6f,%.6f",$0.lat,$0.lng)
        }).joined(separator:"|")
    }
}

enum MotorHistoryPolicy {
    static func inserting(_ entry: SavedMotorPlan,into existing: [SavedMotorPlan]) -> [SavedMotorPlan] {
        guard entry.valid else{return existing}
        return Array(([entry]+existing.filter{$0.valid && $0.key != entry.key}).prefix(20))
    }
}

final class MotorPlanHistory: ObservableObject {
    static let shared=MotorPlanHistory()
    @Published private(set) var plans: [SavedMotorPlan]=[]
    private let defaults: UserDefaults
    private let key="motor_plan_history_v1"
    init(defaults: UserDefaults = .standard) {
        self.defaults=defaults
        if let bytes=defaults.data(forKey:key),bytes.count<=1024*1024,
           let saved=try? JSONDecoder().decode([SavedMotorPlan].self,from:bytes) {
            plans=Array(saved.filter{$0.valid}.prefix(20))
        }
    }
    func save(_ route: MotorRoute) {
        plans=MotorHistoryPolicy.inserting(SavedMotorPlan(date:Date(),request:route.request,distance:route.totalMeters,seconds:route.seconds),into:plans)
        persist()
    }
    func remove(_ id: UUID) {plans.removeAll{$0.id==id};persist()}
    func clear(bicycle: Bool) {plans.removeAll{$0.request.bicycle==bicycle};persist()}
    private func persist() {if let bytes=try? JSONEncoder().encode(plans){defaults.set(bytes,forKey:key)}}
}
