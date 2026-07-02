export interface ModbusServerInverterConfig {
    serial: string;
    unit_id: number;
}

export interface ModbusServerConfig {
    enabled: boolean;
    port: number;
    inverter: Array<ModbusServerInverterConfig>;
}
