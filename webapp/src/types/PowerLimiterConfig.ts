export interface PowerLimiterConfig {
    enabled: boolean;
    mqtt_topic_powermeter_1: string;
    mqtt_topic_powermeter_2: string;
    mqtt_topic_powermeter_3: string;
    powermeter_measures_inverter: boolean;
}
