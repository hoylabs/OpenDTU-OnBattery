import type { ValueObject } from '@/types/LiveDataStatus';
import type { StringValue } from '@/types/StringValue';

type BatteryData = (ValueObject | StringValue)[];

export interface Battery {
    manufacturer: string;
    serial: string;
    fwversion: string;
    hwversion: string;
    data_age: number;
    max_age: number;
    values: BatteryData[];
    showIssues: boolean;
    issues: number[];
}
