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
                        :class="getStatusClass()"
                    >
                        <div class="p-1 flex-grow-1">
                            <div class="d-flex flex-wrap">
                                <div style="padding-right: 2em">
                                    <template
                                        v-if="
                                            data.vendor_name !== undefined &&
                                            data.product_name !== undefined
                                        "
                                    >
                                        {{ data.vendor_name }} {{ data.product_name }}
                                    </template>
                                    <template v-else>Grid Charger</template>
                                </div>
                                <div v-if="data.serial" style="padding-right: 2em">
                                    {{ $t('gridcharger.SerialNumber') }}: {{ data.serial }}
                                </div>
                                <DataAgeDisplay :data-age-ms="data.data_age" />
                            </div>
                        </div>
                        <div class="btn-toolbar p-2" role="toolbar">
                            <div class="btn-group me-2" role="group">
                                <button
                                    :disabled="!data.reachable"
                                    type="button"
                                    class="btn btn-sm btn-danger"
                                    @click="onShowLimitSettings()"
                                    v-tooltip
                                    :title="$t('gridcharger.LimitSettings')"
                                >
                                    <BIconSpeedometer style="font-size: 24px" />
                                </button>
                            </div>
                            <div class="btn-group me-2" role="group">
                                <button
                                    :disabled="!data.reachable"
                                    type="button"
                                    class="btn btn-sm btn-danger"
                                    @click="onShowPowerModal()"
                                    v-tooltip
                                    :title="$t('gridcharger.TurnOnOff')"
                                >
                                    <BIconPower style="font-size: 24px" />
                                </button>
                            </div>
                        </div>
                    </div>

                    <div class="card-body">
                        <div class="row flex-row flex-wrap align-items-start g-3">
                            <div class="col order-0">
                                <div class="card card-table border-info">
                                    <div class="card-header text-bg-info">{{ $t('gridcharger.Device') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('gridcharger.Property') }}</th>
                                                    <th scope="col">
                                                        {{ $t('gridcharger.Value') }}
                                                    </th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="data.board_type">
                                                    <th scope="row">{{ $t('gridcharger.BoardType') }}</th>
                                                    <td>
                                                        {{ data.board_type }}
                                                    </td>
                                                </tr>
                                                <tr v-if="data.manufactured">
                                                    <th scope="row">{{ $t('gridcharger.Manufactured') }}</th>
                                                    <td>
                                                        {{ data.manufactured }}
                                                    </td>
                                                </tr>
                                                <tr v-if="data.product_description">
                                                    <th scope="row">{{ $t('gridcharger.ProductDescription') }}</th>
                                                    <td>
                                                        {{ data.product_description }}
                                                    </td>
                                                </tr>
                                                <tr
                                                    v-if="data.row !== undefined && data.slot !== undefined"
                                                >
                                                    <th scope="row">{{ $t('gridcharger.SlotLabel') }}</th>
                                                    <td>
                                                        {{
                                                            $t('gridcharger.SlotText', {
                                                                row: data.row,
                                                                slot: data.slot,
                                                            })
                                                        }}
                                                    </td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-0">
                                <div class="card card-table">
                                    <div class="card-header">{{ $t('gridcharger.Input') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('gridcharger.Property') }}</th>
                                                    <th class="value" scope="col">
                                                        {{ $t('gridcharger.Value') }}
                                                    </th>
                                                    <th scope="col">{{ $t('gridcharger.Unit') }}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="data.input_voltage !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.voltage') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(data.input_voltage.v) }}
                                                    </td>
                                                    <td>{{ data.input_voltage.u }}</td>
                                                </tr>
                                                <tr v-if="data.input_current !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.current') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(data.input_current.v) }}
                                                    </td>
                                                    <td>{{ data.input_current.u }}</td>
                                                </tr>
                                                <tr v-if="data.input_power !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.power') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(data.input_power.v) }}
                                                    </td>
                                                    <td>{{ data.input_power.u }}</td>
                                                </tr>
                                                <tr v-if="data.input_temp !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.temp') }}</th>
                                                    <td class="value">
                                                        {{ Math.round(data.input_temp.v) }}
                                                    </td>
                                                    <td>{{ data.input_temp.u }}</td>
                                                </tr>
                                                <tr v-if="data.input_frequency !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.frequency') }}</th>
                                                    <td class="value">
                                                        {{ formatNumber(data.input_frequency.v) }}
                                                    </td>
                                                    <td>{{ data.input_frequency.u }}</td>
                                                </tr>
                                                <tr v-if="data.efficiency !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.efficiency') }}</th>
                                                    <td class="value">
                                                        {{ data.efficiency.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ data.efficiency.u }}</td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-1">
                                <div class="card card-table">
                                    <div class="card-header">{{ $t('gridcharger.Output') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('gridcharger.Property') }}</th>
                                                    <th class="value" scope="col">
                                                        {{ $t('gridcharger.Value') }}
                                                    </th>
                                                    <th scope="col">{{ $t('gridcharger.Unit') }}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="data.output_voltage !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.voltage') }}</th>
                                                    <td class="value">
                                                        {{ data.output_voltage.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ data.output_voltage.u }}</td>
                                                </tr>
                                                <tr v-if="data.output_current !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.current') }}</th>
                                                    <td class="value">
                                                        {{ data.output_current.v.toFixed(2) }}
                                                    </td>
                                                    <td>{{ data.output_current.u }}</td>
                                                </tr>
                                                <tr v-if="data.max_output_current !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.max_current') }}</th>
                                                    <td class="value">
                                                        {{ data.max_output_current.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ data.max_output_current.u }}</td>
                                                </tr>
                                                <tr v-if="data.output_power !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.power') }}</th>
                                                    <td class="value">
                                                        {{ data.output_power.v.toFixed(1) }}
                                                    </td>
                                                    <td>{{ data.output_power.u }}</td>
                                                </tr>
                                                <tr v-if="data.output_temp !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.temp') }}</th>
                                                    <td class="value">
                                                        {{ Math.round(data.output_temp.v) }}
                                                    </td>
                                                    <td>{{ data.output_temp.u }}</td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-2">
                                <div class="card card-table">
                                    <div class="card-header">{{ $t('gridcharger.Acknowledgements') }}</div>
                                    <div class="card-body">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('gridcharger.Property') }}</th>
                                                    <th class="value" scope="col">{{ $t('gridcharger.Value') }}</th>
                                                    <th scope="col">{{ $t('gridcharger.Unit') }}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-if="data.online_voltage !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.OnlineVoltage') }}</th>
                                                    <td class="value">{{ data.online_voltage.v.toFixed(2) }}</td>
                                                    <td>{{ data.online_voltage.u }}</td>
                                                </tr>
                                                <tr v-if="data.offline_voltage !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.OfflineVoltage') }}</th>
                                                    <td class="value">{{ data.offline_voltage.v.toFixed(2) }}</td>
                                                    <td>{{ data.offline_voltage.u }}</td>
                                                </tr>
                                                <tr v-if="data.online_current !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.OnlineCurrent') }}</th>
                                                    <td class="value">{{ data.online_current.v.toFixed(2) }}</td>
                                                    <td>{{ data.online_current.u }}</td>
                                                </tr>
                                                <tr v-if="data.offline_current !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.OfflineCurrent') }}</th>
                                                    <td class="value">{{ data.offline_current.v.toFixed(2) }}</td>
                                                    <td>{{ data.offline_current.u }}</td>
                                                </tr>
                                                <tr v-if="data.input_current_limit !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.InputCurrentLimit') }}</th>
                                                    <td class="value">
                                                        {{ data.input_current_limit.v.toFixed(2) }}
                                                    </td>
                                                    <td>{{ data.input_current_limit.u }}</td>
                                                </tr>
                                                <tr v-if="data.production_enabled !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.ProductionEnabled') }}</th>
                                                    <td class="value">
                                                        {{
                                                            data.production_enabled
                                                                ? $t('base.Yes')
                                                                : $t('base.No')
                                                        }}
                                                    </td>
                                                    <td></td>
                                                </tr>
                                                <tr v-if="data.fan_online_full_speed !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.FanOnlineFullSpeed') }}</th>
                                                    <td class="value">
                                                        {{
                                                            data.fan_online_full_speed
                                                                ? $t('gridcharger.FanFullSpeed')
                                                                : $t('gridcharger.FanAuto')
                                                        }}
                                                    </td>
                                                    <td></td>
                                                </tr>
                                                <tr v-if="data.fan_offline_full_speed !== undefined">
                                                    <th scope="row">{{ $t('gridcharger.FanOfflineFullSpeed') }}</th>
                                                    <td class="value">
                                                        {{
                                                            data.fan_offline_full_speed
                                                                ? $t('gridcharger.FanFullSpeed')
                                                                : $t('gridcharger.FanAuto')
                                                        }}
                                                    </td>
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

        <ModalDialog modalId="huaweiLimitSettingView" :title="$t('gridcharger.LimitSettings')" :loading="dataLoading">
            <BootstrapAlert v-model="showAlertLimit" :variant="alertTypeLimit">
                {{ alertMessageLimit }}
            </BootstrapAlert>

            <InputElement
                :label="$t('gridcharger.SetVoltageLimit')"
                type="number"
                step="0.01"
                v-model="targetLimitList.voltage"
                postfix="V"
            />

            <InputElement
                :label="$t('gridcharger.SetCurrentLimit')"
                type="number"
                step="0.1"
                v-model="targetLimitList.current"
                postfix="A"
            />
            <template #footer>
                <button type="button" class="btn btn-danger" @click="onSubmitLimit">
                    {{ $t('gridcharger.SetLimits') }}
                </button>
            </template>
        </ModalDialog>

        <ModalDialog modalId="huaweiPowerView" :title="$t('gridcharger.PowerControl')" :loading="dataLoading">
            <div class="d-grid gap-2 col-6 mx-auto">
                <button type="button" class="btn btn-success" @click="onSetPower(true)">
                    <BIconToggleOn class="fs-4" />&nbsp;{{ $t('gridcharger.TurnOn') }}
                </button>
                <button type="button" class="btn btn-danger" @click="onSetPower(false)">
                    <BIconToggleOff class="fs-4" />&nbsp;{{ $t('gridcharger.TurnOff') }}
                </button>
            </div>
        </ModalDialog>
    </template>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { GridCharger } from '@/types/GridChargerDataStatus';
import type { HuaweiLimitConfig } from '@/types/HuaweiLimitConfig';
import { handleResponse, authHeader, authUrl } from '@/utils/authentication';

import * as bootstrap from 'bootstrap';
import { BIconSpeedometer, BIconPower, BIconToggleOn, BIconToggleOff } from 'bootstrap-icons-vue';
import DataAgeDisplay from '@/components/DataAgeDisplay.vue';
import InputElement from '@/components/InputElement.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import ModalDialog from '@/components/ModalDialog.vue';

export default defineComponent({
    components: {
        BIconSpeedometer,
        BIconPower,
        BIconToggleOn,
        BIconToggleOff,
        InputElement,
        DataAgeDisplay,
        BootstrapAlert,
        ModalDialog,
    },
    data() {
        return {
            socket: {} as WebSocket,
            heartInterval: 0,
            dataAgeInterval: 0,
            dataLoading: true,
            data: {} as GridCharger,
            isFirstFetchAfterConnect: true,
            targetLimitList: {} as HuaweiLimitConfig,
            huaweiLimitSettingView: {} as bootstrap.Modal,
            huaweiPowerView: {} as bootstrap.Modal,
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
            console.log('Get initialData for GridCharger');
            this.dataLoading = true;

            fetch('/api/gridchargerlivedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.data = data;
                    this.dataLoading = false;
                });
        },
        initSocket() {
            console.log('Starting connection to GridCharger WebSocket Server');

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === 'https:' ? 'wss' : 'ws'}://${authString}${host}/gridchargerlivedata`;

            this.socket = new WebSocket(webSocketUrl);

            this.socket.onmessage = (event) => {
                console.log(event);
                this.data = JSON.parse(event.data);
                this.dataLoading = false;
                this.heartCheck(); // Reset heartbeat detection
            };

            this.socket.onopen = function (event) {
                console.log(event);
                console.log('Successfully connected to the GridCharger websocket server...');
            };

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.closeSocket();
            };
        },
        initDataAgeing() {
            this.dataAgeInterval = setInterval(() => {
                if (this.data) {
                    this.data.data_age += 1000;
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
        onShowPowerModal() {
            this.huaweiPowerView = new bootstrap.Modal('#huaweiPowerView');
            this.huaweiPowerView.show();
        },
        onSubmitLimit(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.targetLimitList));

            console.log(this.targetLimitList);

            fetch('/api/gridcharger/limit', {
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
        onSetPower(power: boolean) {
            const formData = new FormData();
            formData.append('data', JSON.stringify({ power }));

            fetch('/api/gridcharger/power', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    console.log(response);
                    if (response.type == 'success') {
                        this.huaweiPowerView.hide();
                    }
                });
        },
        getStatusClass() {
            if (this.data.reachable !== true) {
                return 'text-bg-danger';
            }

            if (
                this.data.reachable === true &&
                this.data.output_power?.v < 10 &&
                this.data.output_current?.v < 0.1
            ) {
                return 'text-bg-warning';
            }

            return 'text-bg-success';
        },
    },
});
</script>
