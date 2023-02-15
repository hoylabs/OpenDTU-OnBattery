export interface PowerMeterConfig {
    enabled: boolean;
    source: number;
    mqtt_topic_powermeter_1: string;
    mqtt_topic_powermeter_2: string;
    mqtt_topic_powermeter_3: string;
    interval: number;
}
