<template>
    <BasePage :title="$t('batteryadmin.BatterySettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveBatteryConfig">
            <CardElement :text="$t('batteryadmin.BatteryConfiguration')" textVariant="text-bg-primary">
                <InputElement :label="$t('batteryadmin.EnableBattery')"
                              v-model="batteryConfigList.enabled"
                              type="checkbox" />

                <div class="row mb-3" v-show="batteryConfigList.enabled">
                    <label class="col-sm-2 col-form-label">
                        {{ $t('batteryadmin.Protocol') }}
                    </label>
                    <div class="col-sm-10">
                        <select class="form-select" v-model="batteryConfigList.protocol">
                            <option v-for="proto in protocolTypeList" :key="proto.key" :value="proto.key">
                                {{ $t(`batteryadmin.Protocol` + proto.value) }}
                            </option>
                        </select>
                    </div>
                </div>
            </CardElement>

            <button type="submit" class="btn btn-primary mb-3">{{ $t('batteryadmin.Save') }}</button>
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from "@/components/BootstrapAlert.vue";
import CardElement from '@/components/CardElement.vue';
import InputElement from '@/components/InputElement.vue';
import type { BatteryConfig } from "@/types/BatteryConfig";
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        InputElement,
    },
    data() {
        return {
            dataLoading: true,
            batteryConfigList: {} as BatteryConfig,
            alertMessage: "",
            alertType: "info",
            showAlert: false,
            protocolTypeList: [
                { key: 0, value: 'PylontechCan' },
                { key: 1, value: 'JkBmsSerialUart' },
                { key: 2, value: 'JkBmsSerialTransceiver' },
            ],
        };
    },
    created() {
        this.getBatteryConfig();
    },
    methods: {
        getBatteryConfig() {
            this.dataLoading = true;
            fetch("/api/battery/config", { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveBatteryConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append("data", JSON.stringify(this.batteryConfigList));

            fetch("/api/battery/config", {
                method: "POST",
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then(
                    (response) => {
                        this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                        this.alertType = response.type;
                        this.showAlert = true;
                    }
                );
        },
    },
});
</script>
