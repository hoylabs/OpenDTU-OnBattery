export interface PowerLimiterConfig {
    enabled: boolean;
    solar_passtrough_enabled: boolean;
    is_inverter_behind_powermeter: boolean;
    lower_power_limit: number;
    upper_power_limit: number;
    battery_soc_start_threshold: number;
    battery_soc_stop_threshold: number;
    voltage_start_threshold: number;
    voltage_stop_threshold: number;
    voltage_load_correction_factor: number;
}
