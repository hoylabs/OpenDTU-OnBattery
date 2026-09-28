import type { ValueObject } from '@/types/LiveDataStatus';
import type { StringValue } from '@/types/StringValue';

type BatteryData = (ValueObject | StringValue)[];

export interface CellValue {
    v: number;
    u: string;
    d: number;
}

export interface CellColumn {
    name: string;
    u: string;
    d: number; // -1: yes/no flag
}

// module selector data, part of the battery document
export interface BatteryModuleSummary {
    moduleNumber: number;
    moduleName: string;
    online?: boolean;
    SoC?: number;
    error?: string;
}

// details of one module, sent as {"module": ...} per module (websocket) or
// fetched with ?module=N. Plain arrays, positions described by moduleColumns
// in BatteryView.vue (keeps the JSON built on the ESP small)
export interface BatteryModuleRaw {
    moduleNumber: number;
    moduleSerialNumber?: string;
    swversion?: string;
    nCells?: number;
    balancing?: number; // bitmask, bit 0 = cell 1
    values?: (number | boolean | null)[];
    cellStatus?: number[];
    cells?: number[][]; // one row per cell
}

// summary and details, expanded by the web UI into the usual value cards
export interface BatteryModule extends BatteryModuleSummary, Omit<BatteryModuleRaw, 'values' | 'cellStatus'> {
    values: { [key: string]: CellValue | StringValue };
    cellStatus?: { [key: string]: CellValue };
    cellColumns?: CellColumn[];
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
    modules?: BatteryModuleSummary[];
}
