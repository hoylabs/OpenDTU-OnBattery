export interface Values {
    voltage_resolution: number;
    current_resolution: number;
    open_circuit_voltage_calculated: boolean;
    open_circuit_voltage: number;
    uncompensated_voltage: number;
    internal_resistance_calculated: boolean;
    resistance_configured: number;
    resistance_calculated: number;
    resistance_calculation_count: number;
    resistance_calculation_state: string;
    measurement_time_period: number;
    v_i_time_stamp_delay: number;
}

export interface Limits {
    max_voltage_resolution: number;
    max_current_resolution: number;
    max_measurement_time_period: number;
    max_v_i_time_stamp_delay: number;
    min_resistance_calculation_count: number;
}

export interface BatteryGuardStatus {
    enabled: boolean;
    values: Values;
    limits: Limits;
}
