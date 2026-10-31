import Foundation

/// Apply the requested AMap-building default once when upgrading, then preserve choices.
enum MapDefaults {
    /// Source IDs from every previous version resolve to the sole provider.
    static func source(_ defaults: UserDefaults = .standard) -> Int {
        defaults.set(3, forKey: "map_src")
        defaults.removeObject(forKey: "key_tdt")
        return 3
    }
    static let migrationKey = "amap_building_defaults_v013"
    static func buildings(_ defaults: UserDefaults = .standard) -> Bool {
        if !defaults.bool(forKey: migrationKey) {
            if let old = defaults.object(forKey: "map_src") { defaults.set(old, forKey: "map_src_before_building_default") }
            defaults.set(3, forKey: "map_src")
            defaults.set(true, forKey: "map_buildings")
            defaults.set(true, forKey: migrationKey)
        }
        return defaults.object(forKey: "map_buildings") as? Bool ?? true
    }
}
