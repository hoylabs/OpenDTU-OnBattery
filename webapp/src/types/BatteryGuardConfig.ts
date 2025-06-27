export interface BatteryGuardConfig {
    enabled: boolean;
    internal_resistance: number;
}

// meta-data not directly part of the BatteryGuard settings,
// to control visibility of BatteryGuard settings
export interface BatteryGuardMetaData {
    battery_enabled: boolean;
}
