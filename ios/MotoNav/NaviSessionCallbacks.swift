import Foundation
import ObjectiveC

/// SDK callbacks belong to the navigation/planning/cruise session that registered them.
/// Retained old delegates and callbacks already queued on the main thread are ignored.
final class NaviSessionCallbacks: NSObject {
    private weak var bridge: NaviBridge?
    private let token: UInt64

    init(bridge: NaviBridge, token: UInt64) {
        self.bridge = bridge
        self.token = token
        super.init()
        class_addProtocol(NaviSessionCallbacks.self, AMapNaviDriveManagerDelegate.self)
        class_addProtocol(NaviSessionCallbacks.self, AMapNaviDriveDataRepresentable.self)
    }
    var asDelegate: AMapNaviDriveManagerDelegate? { (self as AnyObject) as? AMapNaviDriveManagerDelegate }
    var asRepresentative: AMapNaviDriveDataRepresentable? { (self as AnyObject) as? AMapNaviDriveDataRepresentable }

    private func deliver(_ body: @escaping (NaviBridge) -> Void) {
        let action = { [weak bridge = bridge, token = token] in
            bridge?.performCallback(token, body)
        }
        if Thread.isMainThread { action() }
        else { DispatchQueue.main.async { action() } }
    }

    @objc(driveManagerOnCalculateRouteSuccess:)
    func mnRouteSuccessOld(_ m: AMapNaviDriveManager) {
        deliver { bridge in bridge.mnRouteSuccessOld(m) }
    }

    @objc(driveManager:onCalculateRouteSuccessWithType:)
    func mnRouteSuccessNew(_ m: AMapNaviDriveManager, type: Int) {
        deliver { bridge in bridge.mnRouteSuccessNew(m, type: type) }
    }

    @objc(driveManager:onCalculateRouteFailure:)
    func mnRouteFail(_ m: AMapNaviDriveManager, error: NSError?) {
        deliver { bridge in bridge.mnRouteFail(m, error: error) }
    }

    @objc(driveManager:playNaviSoundString:soundStringType:)
    func mnSound(_ m: AMapNaviDriveManager, text: String?, type: Int) {
        deliver { bridge in bridge.mnSound(m, text: text, type: type) }
    }

    @objc(driveManagerIsNaviSoundPlaying:)
    func mnSoundPlaying(_ m: AMapNaviDriveManager) -> Bool {
        guard Thread.isMainThread, let bridge = bridge, bridge.acceptsCallback(token) else { return false }
        return bridge.mnSoundPlaying(m)
    }

    @objc(driveManagerOnArrivedDestination:)
    func mnArrived(_ m: AMapNaviDriveManager) {
        deliver { bridge in bridge.mnArrived(m) }
    }

    @objc(driveManagerDidEndEmulatorNavi:)
    func mnEmuEnd(_ m: AMapNaviDriveManager) {
        deliver { bridge in bridge.mnEmuEnd(m) }
    }

    @objc(driveManagerNeedRecalculateRouteForYaw:)
    func mnYaw(_ m: AMapNaviDriveManager) {
        deliver { bridge in bridge.mnYaw(m) }
    }

    @objc(driveManagerNeedRecalculateRouteForTrafficJam:)
    func mnJam(_ m: AMapNaviDriveManager) {
        deliver { bridge in bridge.mnJam(m) }
    }

    @objc(driveManager:updateGPSSignalStrength:)
    func mnGps(_ m: AMapNaviDriveManager, strength: Int) {
        deliver { bridge in bridge.mnGps(m, strength: strength) }
    }

    @objc(driveManager:onSuggestChangeMainNaviRoute:)
    func mnSuggest(_ m: AMapNaviDriveManager, info s: AMapNaviSuggestChangeMainNaviRouteInfo?) {
        deliver { bridge in bridge.mnSuggest(m, info: s) }
    }

    @objc(driveManager:updateNaviInfo:)
    func mnNaviInfo(_ m: AMapNaviDriveManager, info n: AMapNaviInfo?) {
        deliver { bridge in bridge.mnNaviInfo(m, info: n) }
    }

    @objc(driveManager:updateNaviLocation:)
    func mnLocation(_ m: AMapNaviDriveManager, location l: AMapNaviLocation?) {
        deliver { bridge in bridge.mnLocation(m, location: l) }
    }

    @objc(driveManager:updateCameraInfos:)
    func mnCameras(_ m: AMapNaviDriveManager, cams: [AMapNaviCameraInfo]?) {
        deliver { bridge in bridge.mnCameras(m, cams: cams) }
    }

    @objc(driveManager:showLaneBackInfo:laneSelectInfo:)
    func mnLaneShow(_ m: AMapNaviDriveManager, back: String?, select: String?) {
        deliver { bridge in bridge.mnLaneShow(m, back: back, select: select) }
    }

    @objc(driveManagerHideLaneInfo:)
    func mnLaneHide(_ m: AMapNaviDriveManager) {
        deliver { bridge in bridge.mnLaneHide(m) }
    }

    @objc(driveManager:updateTrafficFacilities:)
    func mnFacilities(_ m: AMapNaviDriveManager, list: [AMapNaviTrafficFacilityInfo]?) {
        deliver { bridge in bridge.mnFacilities(m, list: list) }
    }

    @objc(driveManager:updateCruiseElecCameraInfos:)
    func mnCruiseCams(_ m: AMapNaviDriveManager, list: [AMapNaviTrafficFacilityInfo]?) {
        deliver { bridge in bridge.mnCruiseCams(m, list: list) }
    }

    @objc(driveManager:updateCruiseInfo:)
    func mnCruiseInfo(_ m: AMapNaviDriveManager, info c: AMapNaviCruiseInfo?) {
        deliver { bridge in bridge.mnCruiseInfo(m, info: c) }
    }

    @objc(driveManager:updateTrafficStatus:)
    func mnTraffic(_ m: AMapNaviDriveManager, status: [AMapNaviTrafficStatus]?) {
        deliver { bridge in bridge.mnTraffic(m, status: status) }
    }

}
