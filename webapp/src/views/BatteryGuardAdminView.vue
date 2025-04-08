<template>
    <BasePage :title="$t('batteryguardadmin.BatteryGuardSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <BootstrapAlert v-model="configAlert" variant="warning">
            {{ $t('batteryguardadmin.ConfigAlertMessage') }}
        </BootstrapAlert>

        <CardElement
            :text="$t('batteryguardadmin.ConfigHints')"
            textVariant="text-bg-primary"
            v-if="getConfigHints().length"
        >
            <div class="row">
                <div class="col-sm-12">
                    {{ $t('batteryguardadmin.ConfigHintsIntro') }}
                    <ul class="mb-0">
                        <li v-for="(hint, idx) in getConfigHints()" :key="idx">
                            <b v-if="hint.severity === 'requirement'"
                                >{{ $t('batteryguardadmin.ConfigHintRequirement') }}:</b
                            >
                            <b v-if="hint.severity === 'optional'">{{ $t('batteryguardadmin.ConfigHintOptional') }}:</b>
                            {{ $t('batteryguardadmin.ConfigHint' + hint.subject) }}
                        </li>
                    </ul>
                </div>
            </div>
        </CardElement>

        <form @submit="saveConfig" v-if="!configAlert">
            <CardElement :text="$t('batteryguardadmin.BatteryGuardConfiguration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('batteryguardadmin.EnableBatteryGuard')"
                    v-model="batteryGuardConfig.enabled"
                    type="checkbox"
                    wide
                />

                <template v-if="batteryGuardConfig.enabled">
                    <InputElement
                        :label="$t('batteryguardadmin.VerboseLogging')"
                        v-model="batteryGuardConfig.verbose_logging"
                        type="checkbox"
                        wide
                    />

                    <InputElement
                        :label="$t('batteryguardadmin.InternalResistance')"
                        v-model="batteryGuardConfig.internal_resistance"
                        type="number"
                        min="0"
                        max="50"
                        step="0.1"
                        postfix="mOhm"
                        :tooltip="$t('batteryguardadmin.InternalResistanceHint')"
                        wide
                    />
                </template>
            </CardElement>

            <FormFooter @reload="getMetaData" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';
import type { BatteryGuardConfig, BatteryGuardMetaData } from '@/types/BatteryGuardConfig';

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
            dataLoading: false,
            batteryGuardConfig: {} as BatteryGuardConfig,
            batteryGuardMetaData: {} as BatteryGuardMetaData,
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            configAlert: false,
        };
    },
    created() {
        this.getMetaData();
    },
    methods: {
        getConfigHints(): { severity: string; subject: string }[] {
            const meta = this.batteryGuardMetaData;
            const hints = [];

            if (meta.battery_enabled !== true) {
                hints.push({ severity: 'requirement', subject: 'BatteryRequired' });
                this.configAlert = true;
            }

            return hints;
        },
        getMetaData() {
            this.dataLoading = true;
            fetch('/api/batteryguard/metadata', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryGuardMetaData = data;
                    this.getConfigData();
                });
        },
        getConfigData() {
            fetch('/api/batteryguard/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryGuardConfig = data;
                    this.dataLoading = false;
                });
        },
        saveConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.batteryGuardConfig));

            fetch('/api/batteryguard/config', {
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
    },
});
</script>
