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
    enableDischargeCurrentLimit: boolean;
    dischargeCurrentLimit: number;
    useBatteryReportedDischargeCurrentLimit: boolean;
    mqtt_discharge_current_topic: string;
    mqtt_discharge_current_json_path: string;
    mqtt_amperage_unit: number;
}
