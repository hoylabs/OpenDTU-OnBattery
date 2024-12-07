import type { ValueObject } from '@/types/LiveDataStatus';

export interface DynamicPowerLimiter {
    PLSTATE: number;
    PLLIMIT: number;
}

export interface SolarCharger {
    full_update: boolean;
    instances: { [key: string]: SolarChargerInstance };
}

type MpptData = (ValueObject | string)[];

export interface SolarChargerInstance {
    data_age_ms: number;
    product_id: string;
    firmware_version: string;
    values: { [key: string]: MpptData };
}
