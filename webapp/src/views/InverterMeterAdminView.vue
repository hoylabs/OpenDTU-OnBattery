<template>
    <BasePage :title="$t('invertermeteradmin.InverterMeterSettings')" :isLoading="dataLoading">
        <BootstrapAlert
            v-model="showAlert"
            dismissible
            :variant="alertType"
            :auto-dismiss="alertType != 'success' ? 0 : 5000"
        >
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveInverterMeterConfig">
            <CardElement :text="$t('invertermeteradmin.InverterMeterConfiguration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('invertermeteradmin.InverterMeterEnable')"
                    v-model="inverterMeterConfigList.enabled"
                    type="checkbox"
                    wide
                />

                <template v-if="inverterMeterConfigList.enabled">
                    <div class="row mb-3">
                        <label for="inputInverterMeterSource" class="col-sm-4 col-form-label">{{
                            $t('invertermeteradmin.InverterMeterSource')
                        }}</label>
                        <div class="col-sm-8">
                            <select
                                id="inputInverterMeterSource"
                                class="form-select"
                                v-model="inverterMeterConfigList.source"
                            >
                                <option v-for="source in inverterMeterSourceList" :key="source.key" :value="source.key">
                                    {{ source.value }}
                                </option>
                            </select>
                        </div>
                    </div>

                    <InputElement
                        :label="$t('invertermeteradmin.Serial')"
                        v-model="inverterMeterConfigList.serial"
                        type="text"
                        maxlength="32"
                        wide
                    />

                </template>
            </CardElement>

            <template v-if="inverterMeterConfigList.enabled">

                <!-- yarn linter wants us to not combine v-if with v-for, so we need to wrap the CardElements //-->
                <template v-if="inverterMeterConfigList.source === 0">
                    <CardElement
                        v-for="(mqtt, index) in inverterMeterConfigList.mqtt.values"
                        v-bind:key="index"
                        :text="$t('powermeteradmin.MqttValue', { valueNumber: index + 1 })"
                        textVariant="text-bg-primary"
                        add-space
                    >
                        <InputElement
                            :label="$t('powermeteradmin.MqttTopic')"
                            v-model="mqtt.topic"
                            type="text"
                            maxlength="256"
                            wide
                        />

                        <InputElement
                            :label="$t('powermeteradmin.mqttJsonPath')"
                            v-model="mqtt.json_path"
                            type="text"
                            maxlength="256"
                            :tooltip="$t('powermeteradmin.valueJsonPathDescription')"
                            wide
                        />

                        <div class="row mb-3">
                            <label for="mqtt_power_unit" class="col-sm-4 col-form-label">
                                {{ $t('powermeteradmin.valueUnit') }}
                            </label>
                            <div class="col-sm-8">
                                <select id="mqtt_power_unit" class="form-select" v-model="mqtt.unit">
                                    <option v-for="u in unitTypeList" :key="u.key" :value="u.key">
                                        {{ u.value }}
                                    </option>
                                </select>
                            </div>
                        </div>

                        <InputElement
                            :label="$t('powermeteradmin.valueSignInverted')"
                            v-model="mqtt.sign_inverted"
                            :tooltip="$t('powermeteradmin.valueSignInvertedHint')"
                            type="checkbox"
                            wide
                        />
                    </CardElement>
                </template>

                <CardElement
                    v-if="inverterMeterConfigList.source === 1 || inverterMeterConfigList.source === 2"
                    :text="$t('powermeteradmin.SDM')"
                    textVariant="text-bg-primary"
                    add-space
                >
                    <InputElement
                        :label="$t('powermeteradmin.pollingInterval')"
                        v-model="inverterMeterConfigList.serial_sdm.polling_interval"
                        type="number"
                        min="1"
                        max="15"
                        :postfix="$t('powermeteradmin.seconds')"
                        wide
                    />

                    <InputElement
                        :label="$t('powermeteradmin.sdmaddress')"
                        v-model="inverterMeterConfigList.serial_sdm.address"
                        type="number"
                        wide
                    />
                </CardElement>

                <template v-if="inverterMeterConfigList.source === 3">
                    <CardElement :text="$t('invertermeteradmin.HTTP')" textVariant="text-bg-primary" add-space>
                        <InputElement
                            :label="$t('powermeteradmin.httpIndividualRequests')"
                            v-model="inverterMeterConfigList.http_json.individual_requests"
                            type="checkbox"
                            wide
                        />

                        <InputElement
                            :label="$t('powermeteradmin.pollingInterval')"
                            v-model="inverterMeterConfigList.http_json.polling_interval"
                            type="number"
                            min="1"
                            max="15"
                            :postfix="$t('powermeteradmin.seconds')"
                            wide
                        />
                    </CardElement>

                    <CardElement
                        v-for="(httpJson, index) in inverterMeterConfigList.http_json.values"
                        :key="index"
                        :text="$t('powermeteradmin.httpValue', { valueNumber: index + 1 })"
                        textVariant="text-bg-primary"
                        add-space
                    >
                        <InputElement
                            v-if="index > 0"
                            :label="$t('powermeteradmin.httpEnabled')"
                            v-model="httpJson.enabled"
                            type="checkbox"
                            wide
                        />

                        <template v-if="httpJson.enabled || index == 0">
                            <HttpRequestSettings
                                v-model="httpJson.http_request"
                                v-if="index == 0 || inverterMeterConfigList.http_json.individual_requests"
                            />

                            <InputElement
                                :label="$t('powermeteradmin.valueJsonPath')"
                                v-model="httpJson.json_path"
                                type="text"
                                maxlength="256"
                                :tooltip="$t('powermeteradmin.valueJsonPathDescription')"
                                wide
                            />

                            <div class="row mb-3">
                                <label for="power_unit" class="col-sm-4 col-form-label">
                                    {{ $t('powermeteradmin.valueUnit') }}
                                </label>
                                <div class="col-sm-8">
                                    <select id="power_unit" class="form-select" v-model="httpJson.unit">
                                        <option v-for="u in unitTypeList" :key="u.key" :value="u.key">
                                            {{ u.value }}
                                        </option>
                                    </select>
                                </div>
                            </div>

                            <InputElement
                                :label="$t('powermeteradmin.valueSignInverted')"
                                v-model="httpJson.sign_inverted"
                                :tooltip="$t('powermeteradmin.valueSignInvertedHint')"
                                type="checkbox"
                                wide
                            />
                        </template>
                    </CardElement>

                    <CardElement
                        :text="$t('powermeteradmin.testHttpJsonHeader')"
                        textVariant="text-bg-primary"
                        add-space
                    >
                        <div class="text-center mb-3">
                            <button type="button" class="btn btn-primary" @click="testHttpJsonRequest()">
                                {{ $t('powermeteradmin.testHttpJsonRequest') }}
                            </button>
                        </div>

                        <BootstrapAlert
                            v-model="testHttpJsonRequestAlert.show"
                            dismissible
                            :variant="testHttpJsonRequestAlert.type"
                        >
                            {{ testHttpJsonRequestAlert.message }}
                        </BootstrapAlert>
                    </CardElement>
                </template>

                <template v-if="inverterMeterConfigList.source === 6">
                    <CardElement :text="$t('powermeteradmin.HTTP_SML')" textVariant="text-bg-primary" add-space>
                        <InputElement
                            :label="$t('powermeteradmin.pollingInterval')"
                            v-model="inverterMeterConfigList.http_sml.polling_interval"
                            type="number"
                            min="1"
                            max="15"
                            :postfix="$t('powermeteradmin.seconds')"
                            wide
                        />

                        <HttpRequestSettings v-model="inverterMeterConfigList.http_sml.http_request" />
                    </CardElement>

                    <CardElement
                        :text="$t('powermeteradmin.testHttpSmlHeader')"
                        textVariant="text-bg-primary"
                        add-space
                    >
                        <div class="text-center mb-3">
                            <button type="button" class="btn btn-primary" @click="testHttpSmlRequest()">
                                {{ $t('powermeteradmin.testHttpSmlRequest') }}
                            </button>
                        </div>

                        <BootstrapAlert
                            v-model="testHttpSmlRequestAlert.show"
                            dismissible
                            :variant="testHttpSmlRequestAlert.type"
                        >
                            {{ testHttpSmlRequestAlert.message }}
                        </BootstrapAlert>
                    </CardElement>
                </template>

                <template v-if="inverterMeterConfigList.source === 7">
                    <CardElement :text="$t('powermeteradmin.UDP_VICTRON')" textVariant="text-bg-primary" add-space>
                        <InputElement
                            :label="$t('powermeteradmin.pollingInterval')"
                            v-model="udpVictronPollIntervalSeconds"
                            type="number"
                            min="0.5"
                            max="15.0"
                            step="0.1"
                            :postfix="$t('powermeteradmin.seconds')"
                            wide
                        />

                        <InputElement
                            :label="$t('powermeteradmin.ipAddress')"
                            v-model="inverterMeterConfigList.udp_victron.ip_address"
                            type="text"
                            maxlength="15"
                            wide
                        />
                    </CardElement>
                </template>
            </template>

            <FormFooter @reload="getInverterMeterConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import HttpRequestSettings from '@/components/HttpRequestSettings.vue';
import { handleResponse, authHeader } from '@/utils/authentication';
import type { InverterMeterConfig } from '@/types/InverterMeterConfig';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        HttpRequestSettings,
        InputElement,
    },
    data() {
        return {
            dataLoading: true,
            inverterMeterConfigList: {} as InverterMeterConfig,
            inverterMeterSourceList: [
                { key: 0, value: this.$t('powermeteradmin.typeMQTT') },
                { key: 1, value: this.$t('powermeteradmin.typeSDM1ph') },
                { key: 2, value: this.$t('powermeteradmin.typeSDM3ph') },
                { key: 3, value: this.$t('powermeteradmin.typeHTTP_JSON') },
                { key: 4, value: this.$t('powermeteradmin.typeSML') },
                { key: 5, value: this.$t('powermeteradmin.typeSMAHM2') },
                { key: 6, value: this.$t('powermeteradmin.typeHTTP_SML') },
                { key: 7, value: this.$t('powermeteradmin.typeUDP_VICTRON') },
            ],
            unitTypeList: [
                { key: 1, value: 'mW' },
                { key: 0, value: 'W' },
                { key: 2, value: 'kW' },
            ],
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            testHttpJsonRequestAlert: { message: '', type: '', show: false } as {
                message: string;
                type: string;
                show: boolean;
            },
            testHttpSmlRequestAlert: { message: '', type: '', show: false } as {
                message: string;
                type: string;
                show: boolean;
            },
        };
    },
    created() {
        this.getInverterMeterConfig();
    },
    computed: {
        udpVictronPollIntervalSeconds: {
            get(): number {
                return this.inverterMeterConfigList.udp_victron.polling_interval_ms / 1000;
            },
            set(value: number) {
                this.inverterMeterConfigList.udp_victron.polling_interval_ms = value * 1000;
            },
        },
    },
    methods: {
        getInverterMeterConfig() {
            this.dataLoading = true;
            fetch('/api/invertermeter/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.inverterMeterConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveInverterMeterConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.inverterMeterConfigList));

            fetch('/api/invertermeter/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alertMessage = response.message;
                    this.alertType = response.type;
                    this.showAlert = true;
                    window.scrollTo(0, 0);
                });
        },
        testHttpJsonRequest() {
            this.testHttpJsonRequestAlert = {
                message: 'Triggering HTTP request...',
                type: 'info',
                show: true,
            };

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.inverterMeterConfigList));

            fetch('/api/invertermeter/testhttpjsonrequest', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.testHttpJsonRequestAlert = {
                        message: response.message,
                        type: response.type,
                        show: true,
                    };
                });
        },
        testHttpSmlRequest() {
            this.testHttpSmlRequestAlert = {
                message: 'Triggering HTTP request...',
                type: 'info',
                show: true,
            };

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.inverterMeterConfigList));

            fetch('/api/invertermeter/testhttpsmlrequest', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.testHttpSmlRequestAlert = {
                        message: response.message,
                        type: response.type,
                        show: true,
                    };
                });
        },
    },
});
</script>
