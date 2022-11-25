<template>
    <BasePage :title="'Power limiter Settings'" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="savePowerLimiterConfig">
            <div class="card">
                <div class="card-header text-bg-primary">Power limiter - Zero export - Configuration</div>
                <div class="card-body">
                    <div class="row mb-3">
                        <label class="col-sm-2 form-check-label" for="inputPowerlimiter">Enable</label>
                        <div class="col-sm-10">
                            <div class="form-check form-switch">
                                <input class="form-check-input" type="checkbox" id="inputPowerlimiter"
                                    v-model="powerLimiterConfigList.enabled" />
                            </div>
                        </div>
                    </div>

                    <div class="row mb-3" v-show="powerLimiterConfigList.enabled">
                        <label for="inputMqttTopicPowerMeter1" class="col-sm-2 col-form-label">MQTT topic - Power meter #1:</label>
                        <div class="col-sm-10">
                            <div class="input-group">
                                <input type="text" class="form-control" id="inputMqttTopicPowerMeter1"
                                    placeholder="shellies/shellyem3/emeter/0/power" v-model="powerLimiterConfigList.mqtt_topic_powermeter_1" />
                            </div>
                        </div>
                    </div>
                
                    <div class="row mb-3" v-show="powerLimiterConfigList.enabled">
                        <label for="inputMqttTopicPowerMeter2" class="col-sm-2 col-form-label">MQTT topic - Power meter #2:</label>
                        <div class="col-sm-10">
                            <div class="input-group">
                                <input type="text" class="form-control" id="inputMqttTopicPowerMeter2"
                                    placeholder="shellies/shellyem3/emeter/1/power" v-model="powerLimiterConfigList.mqtt_topic_powermeter_2" />
                            </div>
                        </div>
                    </div>

                    <div class="row mb-3" v-show="powerLimiterConfigList.enabled">
                        <label for="inputMqttTopicPowerMeter3" class="col-sm-2 col-form-label">MQTT topic - Power meter #3:</label>
                        <div class="col-sm-10">
                            <div class="input-group">
                                <input type="text" class="form-control" id="inputMqttTopicPowerMeter3"
                                    placeholder="shellies/shellyem3/emeter/2/power" v-model="powerLimiterConfigList.mqtt_topic_powermeter_3" />
                            </div>
                        </div>
                    </div>

                    <div class="row mb-3" v-show="powerLimiterConfigList.enabled">
                        <label class="col-sm-2 form-check-label" for="inputRetain">Power meter measures inverter power</label>
                        <div class="col-sm-10">
                            <div class="form-check form-switch">
                                <input class="form-check-input" type="checkbox" id="inputPowerMeterMeasuresInverter"
                                    v-model="powerLimiterConfigList.powermeter_measures_inverter" />
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <button type="submit" class="btn btn-primary mb-3">Save</button>
        </form>
    </BasePage>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import { handleResponse, authHeader } from '@/utils/authentication';
import type { PowerLimiterConfig } from "@/types/PowerLimiterConfig";

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
    },
    data() {
        return {
            dataLoading: true,
            powerLimiterConfigList: {} as PowerLimiterConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
        };
    },
    created() {
        this.getPowerLimiterConfig();
    },
    methods: {
        getPowerLimiterConfig() {
            this.dataLoading = true;
            fetch("/api/powerlimiter/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter))
                .then((data) => {
                    this.powerLimiterConfigList = data;
                    this.dataLoading = false;
                });
        },
        savePowerLimiterConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.powerLimiterConfigList));

            fetch("/api/powerlimiter/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter))
                .then(
                    (response) => {
                        this.alertMessage = response.message;
                        this.alertType = response.type;
                        this.showAlert = true;
                    }
                );
        },
    },
});
</script>
