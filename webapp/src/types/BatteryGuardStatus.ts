export interface BatteryGuardStatus {
    enabled: boolean;
    battery_data_sufficient: boolean;
    voltage_resolution: number;
    current_resolution: number;
    open_circuit_voltage: number;
    resistance_configured: number;
    resistance_calculated: number;
    resistance_calculation_count: number;
    resistance_calculation_state: string;
    measurement_time_period: number;
    v_i_time_stamp_delay: number;
}
