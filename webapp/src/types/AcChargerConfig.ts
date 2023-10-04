export interface AcChargerConfig {
  enabled: boolean;
  verbose_logging: boolean;
  auto_power_enabled: boolean;
  auto_power_batterysoc_limits_enabled: boolean;
  voltage_limit: number;
  enable_voltage_limit: number;
  lower_power_limit: number;
  upper_power_limit: number;
  reduced_upper_power_limit: number;
  reduced_batterysoc_threshold: number;
  stop_batterysoc_threshold: number;
}
