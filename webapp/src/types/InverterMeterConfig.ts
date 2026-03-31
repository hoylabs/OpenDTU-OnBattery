import type { HttpRequestConfig } from '@/types/HttpRequestConfig';

export interface InverterMeterMqttValue {
    topic: string;
    json_path: string;
    unit: number;
    sign_inverted: boolean;
}

export interface InverterMeterMqttConfig {
    values: Array<InverterMeterMqttValue>;
}

export interface InverterMeterSerialSdmConfig {
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

export interface InverterMeterHttpJsonConfig {
    polling_interval: number;
    individual_requests: boolean;
    values: Array<PowerMeterHttpJsonValue>;
}

export interface InverterMeterHttpSmlConfig {
    polling_interval: number;
    http_request: HttpRequestConfig;
}

export interface InverterMeterUdpVictronConfig {
    polling_interval_ms: number;
    ip_address: string;
}

export interface InverterMeterConfig {
    enabled: boolean;
    source: number;
    serial: string;
    interval: number;
    mqtt: InverterMeterMqttConfig;
    serial_sdm: InverterMeterSerialSdmConfig;
    http_json: InverterMeterHttpJsonConfig;
    http_sml: InverterMeterHttpSmlConfig;
    udp_victron: InverterMeterUdpVictronConfig;
}
