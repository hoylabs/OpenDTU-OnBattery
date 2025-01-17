export interface SolarChargerMqttConfig {
    calculate_output_power: boolean;
    power_topic: string;
    power_path: string;
    power_unit: number;
    voltage_topic: string;
    voltage_path: string;
    voltage_unit: number;
    current_topic: string;
    current_path: string;
    current_unit: number;
}

export interface SolarChargerConfig {
    enabled: boolean;
    verbose_logging: boolean;
    provider: number;
    publish_updates_only: boolean;
    mqtt: SolarChargerMqttConfig;
}
