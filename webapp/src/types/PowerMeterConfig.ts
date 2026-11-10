import type { HttpRequestConfig } from '@/types/HttpRequestConfig';

export interface PowerMeterMqttValue {
    topic: string;
    json_path: string;
    unit: number;
    sign_inverted: boolean;
}

export interface PowerMeterMqttConfig {
    values: Array<PowerMeterMqttValue>;
}

export interface PowerMeterSerialSdmConfig {
    polling_interval: number;
    address: number;
}

export interface PowerMeterHttpJsonValue {
    http_request: HttpRequestConfig;
    enabled: boolean;
    json_path: string;
    unit: number;
    sign_inverted: boolean;
}

export interface PowerMeterHttpJsonConfig {
    polling_interval: number;
    individual_requests: boolean;
    values: Array<PowerMeterHttpJsonValue>;
}

export interface PowerMeterHttpSmlConfig {
    polling_interval: number;
    http_request: HttpRequestConfig;
}

export interface PowerMeterUdpVictronConfig {
    polling_interval_ms: number;
    ip_address: string;
}

export interface PowerMeterModbusTcpRegisterConfig {
    address: number;
    scaling_factor: number;
    data_type: number;
}

export interface PowerMeterModbusTcpConfig {
    polling_interval: number;
    ip_address: string;
    port: number;
    device_id: number;
    power_register: PowerMeterModbusTcpRegisterConfig;
    power_l1_register: PowerMeterModbusTcpRegisterConfig;
    power_l2_register: PowerMeterModbusTcpRegisterConfig;
    power_l3_register: PowerMeterModbusTcpRegisterConfig;
    voltage_l1_register: PowerMeterModbusTcpRegisterConfig;
    voltage_l2_register: PowerMeterModbusTcpRegisterConfig;
    voltage_l3_register: PowerMeterModbusTcpRegisterConfig;
    import_register: PowerMeterModbusTcpRegisterConfig;
    export_register: PowerMeterModbusTcpRegisterConfig;
}

export interface PowerMeterConfig {
    enabled: boolean;
    source: number;
    interval: number;
    mqtt: PowerMeterMqttConfig;
    serial_sdm: PowerMeterSerialSdmConfig;
    http_json: PowerMeterHttpJsonConfig;
    http_sml: PowerMeterHttpSmlConfig;
    udp_victron: PowerMeterUdpVictronConfig;
    modbus_tcp: PowerMeterModbusTcpConfig;
}
