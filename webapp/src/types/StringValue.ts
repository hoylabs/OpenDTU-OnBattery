export interface StringValue {
    value: string;
    translate: boolean;
}

export function isStringValue(value: unknown): value is StringValue {
    return value !== null && typeof value === 'object' && 'value' in value && 'translate' in value;
}
