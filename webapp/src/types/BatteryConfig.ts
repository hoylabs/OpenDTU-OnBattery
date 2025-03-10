export interface BatteryZendureConfig {
    device_type: number;
    device_id: string;
    polling_interval: number;
    soc_min: number;
    soc_max: number;
    bypass_mode: number;
    max_output: number;
    auto_shutdown: boolean;
    output_limit: number;
    output_control: number;
    output_limit_day: number;
    output_limit_night: number;
    sunrise_offset: number;
    sunset_offset: number;
    charge_through_enable: boolean;
    charge_through_interval: number;
}

export interface BatteryConfig {
    enabled: boolean;
    verbose_logging: boolean;
    provider: number;
    jkbms_interface: number;
    jkbms_polling_interval: number;
    mqtt_soc_topic: string;
    mqtt_soc_json_path: string;
    mqtt_voltage_topic: string;
    mqtt_voltage_json_path: string;
    mqtt_voltage_unit: number;
    enable_discharge_current_limit: boolean;
    discharge_current_limit: number;
    discharge_current_limit_below_soc: number;
    discharge_current_limit_below_voltage: number;
    use_battery_reported_discharge_current_limit: boolean;
    mqtt_discharge_current_topic: string;
    mqtt_discharge_current_json_path: string;
    mqtt_amperage_unit: number;
    zendure: BatteryZendureConfig;
}
