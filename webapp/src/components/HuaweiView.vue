<template>
    <div class="text-center" v-if="dataLoading">
        <div class="spinner-border" role="status">
            <span class="visually-hidden">Loading...</span>
        </div>
    </div>

    <template v-else>
        <div class="row gy-3 mt-0">
            <div class="tab-content col-sm-12 col-md-12" id="v-pills-tabContent">
                <div class="card">
                    <div
                        class="card-header d-flex justify-content-between align-items-center"
                        :class="{
                            'text-bg-danger': huaweiData.reachable !== true,
                            'text-bg-success': huaweiData.reachable === true
                        }"
                    >
                        <div class="p-1 flex-grow-1">
                            <div class="d-flex flex-wrap">
                                <div style="padding-right: 2em">
                                    <template v-if="huaweiData.vendor_name !== undefined && huaweiData.product_name !== undefined">
                                        {{ huaweiData.vendor_name }} {{ huaweiData.product_name }}
                                    </template>
                                    <template v-else>
                                        Huawei PSU
                                    </template>
                                </div>
                                <div v-if="huaweiData.serial" style="padding-right: 2em">
                                    {{ $t('huawei.SerialNumber') }}: {{ huaweiData.serial }}
                                </div>
                                <DataAgeDisplay :data-age-ms="huaweiData.data_age" />
                            </div>
                        </div>
                        <div class="btn-toolbar p-2" role="toolbar">
                            <div class="btn-group me-2" role="group">
                                <button
                                    :disabled="!huaweiData.reachable"
                                    type="button"
                                    class="btn btn-sm btn-danger"
                                    @click="onShowLimitSettings()"
                                    v-tooltip
                                    :title="$t('huawei.LimitSettings')"
                                >
                                    <BIconSpeedometer style="font-size: 24px" />
                                </button>
                            </div>
                        </div>
                    </div>

                    <div class="card-body">
                        <div class="row flex-row flex-wrap align-items-start g-3">
                            <div class="col order-0">
                                <div class="card card-table border-info">
                                    <div class="card-header text-bg-info">{{ $t('huawei.Device') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('huawei.Property') }}</th>
                                                    <th scope="col">
                                                        {{ $t('huawei.Value') }}
                                                    </th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="huaweiData.board_type">
                                                    <th scope="row">{{ $t('huawei.BoardType') }}</th>
                                                    <td>
                                                        {{ huaweiData.board_type }}
                                                    </td>
                                                </tr>
                                                <tr v-if="huaweiData.manufactured">
                                                    <th scope="row">{{ $t('huawei.Manufactured') }}</th>
                                                    <td>
                                                        {{ huaweiData.manufactured }}
                                                    </td>
                                                </tr>
                                                <tr v-if="huaweiData.product_description">
                                                    <th scope="row">{{ $t('huawei.ProductDescription') }}</th>
                                                    <td>
                                                        {{ huaweiData.product_description }}
                                                    </td>
                                                </tr>
                                                <tr v-if="huaweiData.row !== undefined && huaweiData.slot !== undefined">
                                                    <th scope="row">{{ $t('huawei.SlotLabel') }}</th>
                                                    <td>
                                                        {{ $t('huawei.SlotText', { row: huaweiData.row, slot: huaweiData.slot }) }}
                                                    </td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-0">
                                <div class="card card-table">
                                    <div class="card-header">{{ $t('huawei.Input') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('huawei.Property') }}</th>
                                                    <th class="value" scope="col">
                                                        {{ $t('huawei.Value') }}
                                                    </th>
                                                    <th scope="col">{{ $t('huawei.Unit') }}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="huaweiData.input_voltage !== undefined">
                                                    <th scope="row">{{ $t('huawei.input_voltage') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(huaweiData.input_voltage.v) }}
                                                    </td>
                                                    <td>{{ huaweiData.input_voltage.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.input_current !== undefined">
                                                    <th scope="row">{{ $t('huawei.input_current') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(huaweiData.input_current.v) }}
                                                    </td>
                                                    <td>{{ huaweiData.input_current.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.input_power !== undefined">
                                                    <th scope="row">{{ $t('huawei.input_power') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(huaweiData.input_power.v) }}
                                                    </td>
                                                    <td>{{ huaweiData.input_power.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.input_temp !== undefined">
                                                    <th scope="row">{{ $t('huawei.input_temp') }}</th>
                                                    <td class="value">
                                                        {{ Math.round(huaweiData.input_temp.v) }}
                                                    </td>
                                                    <td>{{ huaweiData.input_temp.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.input_frequency !== undefined">
                                                    <th scope="row">{{ $t('huawei.input_frequency') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(huaweiData.input_frequency.v) }}
                                                    </td>
                                                    <td>{{ huaweiData.input_frequency.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.efficiency !== undefined">
                                                    <th scope="row">{{ $t('huawei.efficiency') }}</th>
                                                    <td class="value">
                                                        {{ huaweiData.efficiency.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ huaweiData.efficiency.u }}</td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-1">
                                <div class="card card-table">
                                    <div class="card-header">{{ $t('huawei.Output') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('huawei.Property') }}</th>
                                                    <th class="value" scope="col">
                                                        {{ $t('huawei.Value') }}
                                                    </th>
                                                    <th scope="col">{{ $t('huawei.Unit') }}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="huaweiData.output_voltage !== undefined">
                                                    <th scope="row">{{ $t('huawei.output_voltage') }}</th>
                                                    <td class="value">
                                                        {{ huaweiData.output_voltage.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ huaweiData.output_voltage.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.output_current !== undefined">
                                                    <th scope="row">{{ $t('huawei.output_current') }}</th>
                                                    <td class="value">
                                                        {{ huaweiData.output_current.v.toFixed(2) }}
                                                    </td>
                                                    <td>{{ huaweiData.output_current.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.max_output_current !== undefined">
                                                    <th scope="row">{{ $t('huawei.max_output_current') }}</th>
                                                    <td class="value">
                                                        {{ huaweiData.max_output_current.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ huaweiData.max_output_current.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.output_power !== undefined">
                                                    <th scope="row">{{ $t('huawei.output_power') }}</th>
                                                    <td class="value">
                                                        {{ huaweiData.output_power.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ huaweiData.output_power.u }}</td>
                                                </tr>
                                                <tr v-if="huaweiData.output_temp !== undefined">
                                                    <th scope="row">{{ $t('huawei.output_temp') }}</th>
                                                    <td class="value">
                                                        {{ Math.round(huaweiData.output_temp.v) }}
                                                    </td>
                                                    <td>{{ huaweiData.output_temp.u }}</td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-2">
                                <div class="card card-table">
                                    <div class="card-header">{{ $t('huawei.Acknowledgements') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                        <thead>
                                            <tr>
                                                <th scope="col">{{ $t('huawei.Property') }}</th>
                                                <th class="value" scope="col">{{ $t('huawei.Value') }}</th>
                                                <th scope="col">{{ $t('huawei.Unit') }}</th>
                                            </tr>
                                        </thead>
                                        <tbody>
                                            <tr v-if="huaweiData.online_voltage !== undefined">
                                                <th scope="row">{{ $t('huawei.OnlineVoltage') }}</th>
                                                <td class="value">{{ huaweiData.online_voltage.v.toFixed(2) }}</td>
                                                <td>{{ huaweiData.online_voltage.u }}</td>
                                            </tr>
                                            <tr v-if="huaweiData.offline_voltage !== undefined">
                                                <th scope="row">{{ $t('huawei.OfflineVoltage') }}</th>
                                                <td class="value">{{ huaweiData.offline_voltage.v.toFixed(2) }}</td>
                                                <td>{{ huaweiData.offline_voltage.u }}</td>
                                            </tr>
                                            <tr v-if="huaweiData.online_current !== undefined">
                                                <th scope="row">{{ $t('huawei.OnlineCurrent') }}</th>
                                                <td class="value">{{ huaweiData.online_current.v.toFixed(2) }}</td>
                                                <td>{{ huaweiData.online_current.u }}</td>
                                            </tr>
                                            <tr v-if="huaweiData.offline_current !== undefined">
                                                <th scope="row">{{ $t('huawei.OfflineCurrent') }}</th>
                                                <td class="value">{{ huaweiData.offline_current.v.toFixed(2) }}</td>
                                                <td>{{ huaweiData.offline_current.u }}</td>
                                            </tr>
                                            <tr v-if="huaweiData.input_current_limit !== undefined">
                                                <th scope="row">{{ $t('huawei.InputCurrentLimit') }}</th>
                                                <td class="value">{{ huaweiData.input_current_limit.v.toFixed(2) }}</td>
                                                <td>{{ huaweiData.input_current_limit.u }}</td>
                                            </tr>
                                            <tr v-if="huaweiData.production_enabled !== undefined">
                                                <th scope="row">{{ $t('huawei.ProductionEnabled') }}</th>
                                                <td class="value">{{ huaweiData.production_enabled ? $t('base.Yes') : $t('base.No') }}</td>
                                                <td></td>
                                            </tr>
                                            <tr v-if="huaweiData.fan_online_full_speed !== undefined">
                                                <th scope="row">{{ $t('huawei.FanOnlineFullSpeed') }}</th>
                                                <td class="value">{{ huaweiData.fan_online_full_speed ? $t('huawei.FanFullSpeed') : $t('huawei.FanAuto') }}</td>
                                                <td></td>
                                            </tr>
                                            <tr v-if="huaweiData.fan_offline_full_speed !== undefined">
                                                <th scope="row">{{ $t('huawei.FanOfflineFullSpeed') }}</th>
                                                <td class="value">{{ huaweiData.fan_offline_full_speed ? $t('huawei.FanFullSpeed') : $t('huawei.FanAuto') }}</td>
                                                <td></td>
                                            </tr>
                                        </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <div class="modal" id="huaweiLimitSettingView" ref="huaweiLimitSettingView" tabindex="-1">
            <div class="modal-dialog modal-lg">
                <div class="modal-content">
                    <BootstrapAlert v-model="showAlertLimit" :variant="alertTypeLimit">
                        {{ alertMessageLimit }}
                    </BootstrapAlert>

                    <form @submit="onSubmitLimit">
                        <div class="modal-header">
                            <h5 class="modal-title">{{ $t('huawei.LimitSettings') }}</h5>
                            <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                        </div>
                        <div class="modal-body">
                            <InputElement
                                :label="$t('huawei.SetVoltageLimit')"
                                type="number"
                                step="0.01"
                                min="42"
                                max="58"
                                v-model="targetLimitList.voltage"
                                postfix="V"
                            />

                            <InputElement
                                :label="$t('huawei.SetCurrentLimit')"
                                type="number"
                                step="0.1"
                                min="0"
                                max="75"
                                v-model="targetLimitList.current"
                                postfix="A"
                            />
                        </div>

                        <div class="modal-footer">
                            <button type="submit" class="btn btn-danger">
                                {{ $t('huawei.SetLimits') }}
                            </button>

                            <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">
                                {{ $t('huawei.Close') }}
                            </button>
                        </div>
                    </form>
                </div>
            </div>
        </div>
    </template>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { Huawei } from '@/types/HuaweiDataStatus';
import type { HuaweiLimitConfig } from '@/types/HuaweiLimitConfig';
import { handleResponse, authHeader, authUrl } from '@/utils/authentication';

import * as bootstrap from 'bootstrap';
import { BIconSpeedometer } from 'bootstrap-icons-vue';
import DataAgeDisplay from '@/components/DataAgeDisplay.vue';
import InputElement from '@/components/InputElement.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';

export default defineComponent({
    components: {
        BIconSpeedometer,
        InputElement,
        DataAgeDisplay,
        BootstrapAlert,
    },
    data() {
        return {
            socket: {} as WebSocket,
            heartInterval: 0,
            dataAgeInterval: 0,
            dataLoading: true,
            huaweiData: {} as Huawei,
            isFirstFetchAfterConnect: true,
            targetLimitList: {} as HuaweiLimitConfig,
            huaweiLimitSettingView: {} as bootstrap.Modal,
            alertMessageLimit: '',
            alertTypeLimit: 'info',
            showAlertLimit: false,
        };
    },
    created() {
        this.getInitialData();
        this.initSocket();
        this.initDataAgeing();
    },
    unmounted() {
        this.closeSocket();
    },
    methods: {
        getInitialData() {
            console.log('Get initalData for Huawei');
            this.dataLoading = true;

            fetch('/api/huaweilivedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.huaweiData = data;
                    this.dataLoading = false;
                });
        },
        initSocket() {
            console.log('Starting connection to Huawei WebSocket Server');

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === 'https:' ? 'wss' : 'ws'}://${authString}${host}/huaweilivedata`;

            this.socket = new WebSocket(webSocketUrl);

            this.socket.onmessage = (event) => {
                console.log(event);
                this.huaweiData = JSON.parse(event.data);
                this.dataLoading = false;
                this.heartCheck(); // Reset heartbeat detection
            };

            this.socket.onopen = function (event) {
                console.log(event);
                console.log('Successfully connected to the Huawei websocket server...');
            };

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.closeSocket();
            };
        },
        initDataAgeing() {
            this.dataAgeInterval = setInterval(() => {
                if (this.huaweiData) {
                    this.huaweiData.data_age += 1000;
                }
            }, 1000);
        },
        // Send heartbeat packets regularly * 59s Send a heartbeat
        heartCheck() {
            if (this.heartInterval) {
                clearTimeout(this.heartInterval);
            }
            this.heartInterval = setInterval(() => {
                if (this.socket.readyState === 1) {
                    // Connection status
                    this.socket.send('ping');
                } else {
                    this.initSocket(); // Breakpoint reconnection 5 Time
                }
            }, 59 * 1000);
        },
        /** To break off websocket Connect */
        closeSocket() {
            this.socket.close();
            if (this.heartInterval) {
                clearTimeout(this.heartInterval);
            }
            this.isFirstFetchAfterConnect = true;
        },
        formatNumber(num: number) {
            return new Intl.NumberFormat(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 }).format(num);
        },
        onShowLimitSettings() {
            this.huaweiLimitSettingView = new bootstrap.Modal('#huaweiLimitSettingView');
            this.huaweiLimitSettingView.show();
        },
        onSubmitLimit(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.targetLimitList));

            console.log(this.targetLimitList);

            fetch('/api/huawei/limit', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    if (response.type == 'success') {
                        this.huaweiLimitSettingView.hide();
                        this.showAlertLimit = false;
                    } else {
                        this.alertMessageLimit = this.$t('onbatteryapiresponse.' + response.code, response.param);
                        this.alertTypeLimit = response.type;
                        this.showAlertLimit = true;
                    }
                });
        },
    },
});
</script>
