export interface AcChargerConfig {
    enabled: boolean;
    verbose_logging: boolean;
    hardware_interface: number;
    can_controller_frequency: number;
    auto_power_enabled: boolean;
    auto_power_batterysoc_limits_enabled: boolean;
    offline_voltage: number;
    offline_current: number;
    input_current_limit: number;
    fan_online_full_speed: boolean;
    fan_offline_full_speed: boolean;
    voltage_limit: number;
    enable_voltage_limit: number;
    lower_power_limit: number;
    upper_power_limit: number;
    emergency_charge_enabled: boolean;
    stop_batterysoc_threshold: number;
    target_power_consumption: number;
}
