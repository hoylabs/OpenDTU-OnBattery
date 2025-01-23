import type { ValueObject } from '@/types/LiveDataStatus';

// Huawei
export interface Huawei {
    data_age: 0;
    input_voltage: ValueObject;
    input_current: ValueObject;
    input_power: ValueObject;
    input_temp: ValueObject;
    input_frequency: ValueObject;
    efficiency: ValueObject;
    output_voltage: ValueObject;
    output_current: ValueObject;
    max_output_current: ValueObject;
    output_power: ValueObject;
    output_temp: ValueObject;
    board_type?: string;
    serial?: string;
    manufactured?: string;
    vendor_name?: string;
    product_name?: string;
    product_description?: string;
}
