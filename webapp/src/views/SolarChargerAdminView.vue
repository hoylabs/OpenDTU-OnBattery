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
                </template>
            </CardElement>

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
            providerTypeList: [{ key: 0, value: 'VeDirect' }],
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
