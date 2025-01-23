<template>
    <BasePage :title="$t('solarchargeradmin.SolarChargerSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveSolarChargerConfig">
            <CardElement :text="$t('solarchargeradmin.SolarChargerConfiguration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('solarchargeradmin.EnableSolarCharger')"
                    v-model="solarChargerConfigList.enabled"
                    type="checkbox"
                    wide
                />

                <template v-if="solarChargerConfigList.enabled">
                    <InputElement
                        :label="$t('solarchargeradmin.VerboseLogging')"
                        v-model="solarChargerConfigList.verbose_logging"
                        type="checkbox"
                        wide
                    />

                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('solarchargeradmin.Provider') }}
                        </label>
                        <div class="col-sm-8">
                            <select class="form-select" v-model="solarChargerConfigList.provider">
                                <option v-for="provider in providerTypeList" :key="provider.key" :value="provider.key">
                                    {{ $t(`solarchargeradmin.Provider` + provider.value) }}
                                </option>
                            </select>
                        </div>
                    </div>

                    <InputElement
                        :label="$t('solarchargeradmin.MqttPublishUpdatesOnly')"
                        v-model="solarChargerConfigList.publish_updates_only"
                        v-if="solarChargerConfigList.provider === 0"
                        type="checkbox"
                        wide
                    />

                    <template v-if="solarChargerConfigList.provider === 1">
                        <InputElement
                            :label="$t('solarchargeradmin.CalculateOutputPower')"
                            v-model="solarChargerConfigList.mqtt.calculate_output_power"
                            :tooltip="$t('solarchargeradmin.CalculateOutputPowerDescription')"
                            type="checkbox"
                            wide
                        />

                        <div class="row">
                            <div class="col-sm-4"></div>
                            <div class="col-sm-8">
                                <div
                                    class="alert alert-secondary mb-0"
                                    role="alert"
                                    v-html="$t('solarchargeradmin.OutputPowerUsageHint')"
                                ></div>
                            </div>
                        </div>
                    </template>
                </template>
            </CardElement>

            <template v-if="solarChargerConfigList.enabled && solarChargerConfigList.provider === 1">
                <CardElement
                    v-if="!solarChargerConfigList.mqtt.calculate_output_power"
                    :text="$t('solarchargeradmin.MqttOutputPowerConfiguration')"
                    textVariant="text-bg-primary"
                    addSpace
                >
                    <InputElement
                        :label="$t('solarchargeradmin.MqttOutputPowerTopic')"
                        v-model="solarChargerConfigList.mqtt.power_topic"
                        type="text"
                        maxlength="256"
                        wide
                    />

                    <InputElement
                        :label="$t('solarchargeradmin.MqttJsonPath')"
                        v-model="solarChargerConfigList.mqtt.power_path"
                        type="text"
                        maxlength="256"
                        :tooltip="$t('solarchargeradmin.MqttJsonPathDescription')"
                        wide
                    />

                    <div class="row mb-3">
                        <label for="power_unit" class="col-sm-4 col-form-label">
                            {{ $t('solarchargeradmin.MqttOutputPowerUnit') }}
                        </label>
                        <div class="col-sm-8">
                            <select
                                id="power_unit"
                                class="form-select"
                                v-model="solarChargerConfigList.mqtt.power_unit"
                            >
                                <option v-for="u in wattageUnitTypeList" :key="u.key" :value="u.key">
                                    {{ u.value }}
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <CardElement
                    v-if="solarChargerConfigList.mqtt.calculate_output_power"
                    :text="$t('solarchargeradmin.MqttOutputCurrentConfiguration')"
                    textVariant="text-bg-primary"
                    addSpace
                >
                    <InputElement
                        :label="$t('solarchargeradmin.MqttOutputCurrentTopic')"
                        :tooltip="$t('solarchargeradmin.MqttOutputCurrentUsageHint')"
                        v-model="solarChargerConfigList.mqtt.current_topic"
                        type="text"
                        maxlength="256"
                        wide
                    />

                    <InputElement
                        :label="$t('solarchargeradmin.MqttJsonPath')"
                        v-model="solarChargerConfigList.mqtt.current_path"
                        type="text"
                        maxlength="256"
                        :tooltip="$t('solarchargeradmin.MqttJsonPathDescription')"
                        wide
                    />

                    <div class="row mb-3">
                        <label for="current_unit" class="col-sm-4 col-form-label">
                            {{ $t('solarchargeradmin.MqttOutputCurrentUnit') }}
                        </label>
                        <div class="col-sm-8">
                            <select
                                id="current_unit"
                                class="form-select"
                                v-model="solarChargerConfigList.mqtt.current_unit"
                            >
                                <option v-for="u in amperageUnitTypeList" :key="u.key" :value="u.key">
                                    {{ u.value }}
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>

                <CardElement
                    :text="$t('solarchargeradmin.MqttOutputVoltageConfiguration')"
                    textVariant="text-bg-primary"
                    addSpace
                >
                    <InputElement
                        :label="$t('solarchargeradmin.MqttOutputVoltageTopic')"
                        :tooltip="$t('solarchargeradmin.MqttOutputVoltagetUsageHint')"
                        v-model="solarChargerConfigList.mqtt.voltage_topic"
                        type="text"
                        maxlength="256"
                        wide
                    />

                    <InputElement
                        :label="$t('solarchargeradmin.MqttJsonPath')"
                        v-model="solarChargerConfigList.mqtt.voltage_path"
                        type="text"
                        maxlength="256"
                        :tooltip="$t('solarchargeradmin.MqttJsonPathDescription')"
                        wide
                    />

                    <div class="row mb-3">
                        <label for="voltage_unit" class="col-sm-4 col-form-label">
                            {{ $t('solarchargeradmin.MqttOutputVoltageUnit') }}
                        </label>
                        <div class="col-sm-8">
                            <select
                                id="voltage_unit"
                                class="form-select"
                                v-model="solarChargerConfigList.mqtt.voltage_unit"
                            >
                                <option v-for="u in voltageUnitTypeList" :key="u.key" :value="u.key">
                                    {{ u.value }}
                                </option>
                            </select>
                        </div>
                    </div>
                </CardElement>
            </template>

            <FormFooter @reload="getSolarChargerConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import type { SolarChargerConfig } from '@/types/SolarChargerConfig';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        InputElement,
    },
    data() {
        return {
            dataLoading: true,
            solarChargerConfigList: {} as SolarChargerConfig,
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            providerTypeList: [
                { key: 0, value: 'VeDirect' },
                { key: 1, value: 'Mqtt' },
            ],
            wattageUnitTypeList: [
                { key: 0, value: 'kW' },
                { key: 1, value: 'W' },
                { key: 2, value: 'mW' },
            ],
            voltageUnitTypeList: [
                { key: 0, value: 'V' },
                { key: 1, value: 'dV' },
                { key: 2, value: 'cV' },
                { key: 3, value: 'mV' },
            ],
            amperageUnitTypeList: [
                { key: 0, value: 'A' },
                { key: 1, value: 'mA' },
            ],
        };
    },
    created() {
        this.getSolarChargerConfig();
    },
    methods: {
        getSolarChargerConfig() {
            this.dataLoading = true;
            fetch('/api/solarcharger/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.solarChargerConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveSolarChargerConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.solarChargerConfigList));

            fetch('/api/solarcharger/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                    this.alertType = response.type;
                    this.showAlert = true;
                });
        },
    },
});
</script>
