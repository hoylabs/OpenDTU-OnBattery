export interface AcChargerConfig {
  enabled: boolean;
  auto_power_enabled: boolean;
  auto_power_reduce_on_batterysoc_enabled: boolean;
  voltage_limit: number;
  enable_voltage_limit: number;
  lower_power_limit: number;
  upper_power_limit: number;
  batterysoc_threshold: number;
  reduced_upper_power_limit: number;
}
