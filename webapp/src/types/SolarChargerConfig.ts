export interface SolarChargerMqttConfig {
    calculate_output_power: boolean;
    mqtt_output_power_topic: string;
    mqtt_output_power_path: string;
    mqtt_output_power_unit: number;
    mqtt_output_voltage_topic: string;
    mqtt_output_voltage_path: string;
    mqtt_output_voltage_unit: number;
    mqtt_output_current_topic: string;
    mqtt_output_current_path: string;
    mqtt_output_current_unit: number;
}

export interface SolarChargerConfig {
    enabled: boolean;
    verbose_logging: boolean;
    provider: number;
    publish_updates_only: boolean;
    mqtt: SolarChargerMqttConfig;
}
