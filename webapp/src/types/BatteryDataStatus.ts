import type { ValueObject } from '@/types/LiveDataStatus';
import type { StringValue } from '@/types/StringValue';

type BatteryData = (ValueObject | StringValue)[];

export interface CellValue {
    v: number;
    u: string;
    d: number;
}

export interface CellStatus {
    cellMinVoltage: CellValue;
    cellMaxVoltage: CellValue;
    cellDiffVoltage: CellValue;
    cellMinTemperature: CellValue;
    cellMaxTemperature: CellValue;
}

export interface CellEntry {
    voltage: CellValue;
    temperature: CellValue;
}

export interface BatteryModule {
    moduleNumber: number;
    moduleName: string;
    moduleSerialNumber: string;
    hwversion?: string;
    swversion?: string;
    nCells?: number;
    values: { [key: string]: CellValue | StringValue };
    cellStatus?: CellStatus;
    cells?: CellEntry[];
    parameters?: { [key: string]: CellValue };
}

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
    numberOfModules?: number;
    modules?: BatteryModule[];
}
