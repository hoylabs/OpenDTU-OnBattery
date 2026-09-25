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

export interface CellColumn {
    name: string;
    u: string;
    d: number;
}

export interface BatteryModule {
    moduleNumber: number;
    moduleName: string;
    moduleSerialNumber: string;
    swversion?: string;
    nCells?: number;
    values: { [key: string]: CellValue | StringValue };
    limits?: { [key: string]: CellValue };
    capacities?: { [key: string]: CellValue };
    cellStatus?: CellStatus;
    error?: string;
    online?: boolean;
    cellColumns?: CellColumn[];
    // compact on purpose to keep the JSON (built on the ESP for every push) small:
    // one row per cell, values in cellColumns order, unit/decimals only in cellColumns
    cells?: number[][];
}

export interface Battery {
    manufacturer: string;
    serial: string;
    fwversion: string;
    hwversion: string;
    data_age: number;
    values: BatteryData[];
    showIssues: boolean;
    issues: number[];
    numberOfModules?: number;
    modules?: BatteryModule[];
}
