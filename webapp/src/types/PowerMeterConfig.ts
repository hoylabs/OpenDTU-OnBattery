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

export interface PowerMeterConfig {
    enabled: boolean;
    source: number;
    interval: number;
    mqtt: PowerMeterMqttConfig;
    serial_sdm: PowerMeterSerialSdmConfig;
    http_json: PowerMeterHttpJsonConfig;
    http_sml: PowerMeterHttpSmlConfig;
    udp_victron: PowerMeterUdpVictronConfig;
}
