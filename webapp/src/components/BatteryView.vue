<template>
    <div class="text-center" v-if="dataLoading">
        <div class="spinner-border" role="status">
            <span class="visually-hidden">Loading...</span>
        </div>
    </div>

    <div v-else-if="'values' in batteryData">
        <!-- ── Pack-level overview card ───────────────────────────────────── -->
        <div class="row gy-3 mt-0">
            <div class="tab-content col-sm-12 col-md-12" id="v-pills-tabContent">
                <div class="card">
                    <div
                        class="card-header d-flex justify-content-between align-items-center"
                        :class="{
                            'text-bg-danger': batteryData.data_age >= batteryData.max_age,
                            'text-bg-success': batteryData.data_age < batteryData.max_age,
                        }"
                    >
                        <div class="p-1 flex-grow-1">
                            <div class="d-flex flex-wrap">
                                <div style="padding-right: 2em">
                                    {{ $t('battery.battery') }}: {{ batteryData.manufacturer }}
                                </div>
                                <div style="padding-right: 2em" v-if="'serial' in batteryData">
                                    {{ $t('home.SerialNumber') }}{{ batteryData.serial }}
                                </div>
                                <div style="padding-right: 2em" v-if="'fwversion' in batteryData">
                                    {{ $t('battery.FwVersion') }}: {{ batteryData.fwversion }}
                                </div>
                                <div style="padding-right: 2em" v-if="'hwversion' in batteryData">
                                    {{ $t('battery.HwVersion') }}: {{ batteryData.hwversion }}
                                </div>
                                <DataAgeDisplay :data-age-ms="batteryData.data_age * 1000" />
                            </div>
                        </div>
                    </div>

                    <div class="card-body">
                        <div class="row flex-row flex-wrap align-items-start g-3">
                            <div
                                v-for="(values, section) in batteryData.values"
                                v-bind:key="section"
                                class="col order-0"
                            >
                                <div class="card card-table" :class="{ 'border-info': true }">
                                    <div class="card-header text-bg-info">
                                        <template v-if="section.toString().startsWith('_')">
                                            {{ section.toString().substring(1) }}
                                        </template>
                                        <template v-else>
                                            {{ $t('battery.' + section) }}
                                        </template>
                                    </div>
                                    <div class="card-body">
                                        <div class="table-responsive">
                                            <table class="table table-striped table-hover">
                                                <thead>
                                                    <tr>
                                                        <th scope="col">{{ $t('battery.Property') }}</th>
                                                        <th class="value" scope="col">
                                                            {{ $t('battery.Value') }}
                                                        </th>
                                                        <th scope="col">{{ $t('battery.Unit') }}</th>
                                                    </tr>
                                                </thead>
                                                <tbody>
                                                    <tr v-for="(prop, key) in values" v-bind:key="key">
                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                        <td class="value">
                                                            <template v-if="isStringValue(prop) && prop.translate">
                                                                {{ $t('battery.' + prop.value) }}
                                                            </template>
                                                            <template v-else-if="isStringValue(prop)">
                                                                {{ prop.value }}
                                                            </template>
                                                            <template v-else>
                                                                {{
                                                                    $n(prop.v, 'decimal', {
                                                                        minimumFractionDigits: prop.d,
                                                                        maximumFractionDigits: prop.d,
                                                                    })
                                                                }}
                                                            </template>
                                                        </td>
                                                        <td>
                                                            <template v-if="!isStringValue(prop)">
                                                                {{ prop.u }}
                                                            </template>
                                                        </td>
                                                    </tr>
                                                </tbody>
                                            </table>
                                        </div>
                                    </div>
                                </div>
                            </div>
                            <div class="col order-1" v-show="batteryData.showIssues">
                                <div class="card card-table">
                                    <div :class="{ 'card-header': true, 'border-bottom-0': maxIssueValue === 0 }">
                                        <div class="d-flex flex-row justify-content-between align-items-baseline">
                                            {{ $t('battery.issues') }}
                                            <div v-if="maxIssueValue === 0" class="badge text-bg-success">
                                                {{ $t('battery.noIssues') }}
                                            </div>
                                            <div
                                                v-else-if="maxIssueValue === 1"
                                                class="badge text-bg-warning text-dark"
                                            >
                                                {{ $t('battery.warning') }}
                                            </div>
                                            <div v-else-if="maxIssueValue === 2" class="badge text-bg-danger">
                                                {{ $t('battery.alarm') }}
                                            </div>
                                        </div>
                                    </div>
                                    <div class="card-body" v-if="'issues' in batteryData">
                                        <table class="table table-striped table-hover">
                                            <thead>
                                                <tr>
                                                    <th scope="col">{{ $t('battery.issueName') }}</th>
                                                    <th scope="col">{{ $t('battery.issueType') }}</th>
                                                </tr>
                                            </thead>
                                            <tbody>
                                                <tr v-for="(prop, key) in batteryData.issues" v-bind:key="key">
                                                    <th scope="row">{{ $t('battery.' + key) }}</th>
                                                    <td>
                                                        <span
                                                            class="badge"
                                                            :class="{
                                                                'text-bg-warning text-dark': prop === 1,
                                                                'text-bg-danger': prop === 2,
                                                            }"
                                                        >
                                                            <template v-if="prop === 1">{{
                                                                $t('battery.warning')
                                                            }}</template>
                                                            <template v-else>{{ $t('battery.alarm') }}</template>
                                                        </span>
                                                    </td>
                                                </tr>
                                            </tbody>
                                        </table>
                                    </div>
                                </div>
                            </div>
                        </div>

                        <!-- ── Per-module section ──────────────────────────────────────────── -->
                        <div v-if="sortedModules.length > 0" class="row gy-3 mt-0">
                            <!-- Left nav pills — hidden when there is only one module -->
                            <div
                                class="col-sm-3 col-md-2"
                                :style="sortedModules.length === 1 ? { display: 'none' } : {}"
                            >
                                <div
                                    class="nav nav-pills row-cols-sm-1 gap-3"
                                    id="battery-module-nav"
                                    role="tablist"
                                    aria-orientation="vertical"
                                >
                                    <button
                                        v-for="mod in sortedModules"
                                        :key="mod.moduleNumber"
                                        class="nav-link border border-primary text-break"
                                        :class="{ active: mod.moduleNumber === firstModuleNumber }"
                                        :id="'battery-module-tab-' + mod.moduleNumber"
                                        data-bs-toggle="pill"
                                        :data-bs-target="'#battery-module-' + mod.moduleNumber"
                                        type="button"
                                        role="tab"
                                        :aria-controls="'battery-module-' + mod.moduleNumber"
                                        :aria-selected="mod.moduleNumber === firstModuleNumber"
                                    >
                                        {{ mod.moduleName }}
                                    </button>
                                </div>
                            </div>

                            <!-- Tab panes -->
                            <div
                                class="tab-content"
                                id="battery-module-content"
                                :class="{
                                    'col-sm-9 col-md-10': sortedModules.length > 1,
                                    'col-sm-12 col-md-12': sortedModules.length === 1,
                                }"
                            >
                                <div
                                    v-for="mod in sortedModules"
                                    :key="mod.moduleNumber"
                                    class="tab-pane fade"
                                    :class="{ 'show active': mod.moduleNumber === firstModuleNumber }"
                                    :id="'battery-module-' + mod.moduleNumber"
                                    role="tabpanel"
                                    :aria-labelledby="'battery-module-tab-' + mod.moduleNumber"
                                    tabindex="0"
                                >
                                    <div class="card">
                                        <div class="card-header text-bg-primary d-flex flex-wrap align-items-center">
                                            <div style="padding-right: 2em">{{ mod.moduleName }}</div>
                                            <div v-if="mod.moduleSerialNumber" style="padding-right: 2em">
                                                S/N: {{ mod.moduleSerialNumber }}
                                            </div>
                                            <div v-if="mod.hwversion" style="padding-right: 2em">
                                                {{ $t('battery.HwVersion') }}: {{ mod.hwversion }}
                                            </div>
                                            <div v-if="mod.swversion" style="padding-right: 2em">
                                                {{ $t('battery.FwVersion') }}: {{ mod.swversion }}
                                            </div>
                                        </div>
                                        <div class="card-body">
                                            <div class="row flex-row flex-wrap align-items-start g-3">
                                                <!-- Module-level values (ambient temp, cycles, balance) -->
                                                <div
                                                    class="col-auto"
                                                    v-if="mod.values && Object.keys(mod.values).length > 0"
                                                >
                                                    <div class="card card-table border-info" style="overflow: hidden">
                                                        <div class="card-header text-bg-info">
                                                            {{ $t('battery.status') }}
                                                        </div>
                                                        <div class="table-responsive">
                                                            <table class="table table-striped table-hover mb-0">
                                                                <tbody>
                                                                    <tr v-for="(prop, key) in mod.values" :key="key">
                                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                                        <td class="value">
                                                                            <template
                                                                                v-if="
                                                                                    isStringValue(prop) &&
                                                                                    prop.translate
                                                                                "
                                                                            >
                                                                                {{ $t('battery.' + prop.value) }}
                                                                            </template>
                                                                            <template v-else-if="isStringValue(prop)">
                                                                                {{ prop.value }}
                                                                            </template>
                                                                            <template v-else>
                                                                                {{
                                                                                    $n(prop.v, 'decimal', {
                                                                                        minimumFractionDigits: prop.d,
                                                                                        maximumFractionDigits: prop.d,
                                                                                    })
                                                                                }}
                                                                            </template>
                                                                        </td>
                                                                        <td>
                                                                            <template v-if="!isStringValue(prop)">
                                                                                {{ prop.u }}
                                                                            </template>
                                                                        </td>
                                                                    </tr>
                                                                </tbody>
                                                            </table>
                                                        </div>
                                                    </div>
                                                </div>

                                                <!-- Cell status summary (min/max voltage and temperature) -->
                                                <div class="col-auto" v-if="mod.cellStatus">
                                                    <div class="card card-table border-info" style="overflow: hidden">
                                                        <div class="card-header text-bg-info">
                                                            {{ $t('battery.cell_status') }}
                                                        </div>
                                                        <div class="table-responsive">
                                                            <table class="table table-striped table-hover mb-0">
                                                                <tbody>
                                                                    <tr
                                                                        v-for="(prop, key) in mod.cellStatus"
                                                                        :key="key"
                                                                    >
                                                                        <th scope="row">{{ $t('battery.' + key) }}</th>
                                                                        <td class="value">
                                                                            {{
                                                                                $n(prop.v, 'decimal', {
                                                                                    minimumFractionDigits: prop.d,
                                                                                    maximumFractionDigits: prop.d,
                                                                                })
                                                                            }}
                                                                        </td>
                                                                        <td>{{ prop.u }}</td>
                                                                    </tr>
                                                                </tbody>
                                                            </table>
                                                        </div>
                                                    </div>
                                                </div>

                                                <!-- Per-cell table -->
                                                <div class="col" v-if="mod.cells && mod.cells.length > 0">
                                                    <div class="card card-table border-info" style="overflow: hidden">
                                                        <div class="card-header text-bg-info">
                                                            {{ $t('battery.cells') }}
                                                        </div>
                                                        <div class="table-responsive">
                                                            <table class="table table-striped table-hover mb-0">
                                                                <thead>
                                                                    <tr>
                                                                        <th scope="col">{{ $t('battery.cell') }}</th>
                                                                        <th scope="col" class="value">
                                                                            {{ $t('battery.voltage') }}
                                                                        </th>
                                                                        <th scope="col"></th>
                                                                        <th scope="col" class="value">
                                                                            {{ $t('battery.temperature') }}
                                                                        </th>
                                                                        <th scope="col"></th>
                                                                    </tr>
                                                                </thead>
                                                                <tbody>
                                                                    <tr v-for="(c, idx) in mod.cells" :key="idx">
                                                                        <th scope="row">{{ idx + 1 }}</th>
                                                                        <td class="value">
                                                                            {{
                                                                                $n(c.voltage.v, 'decimal', {
                                                                                    minimumFractionDigits: c.voltage.d,
                                                                                    maximumFractionDigits: c.voltage.d,
                                                                                })
                                                                            }}
                                                                        </td>
                                                                        <td>{{ c.voltage.u }}</td>
                                                                        <td class="value">
                                                                            {{
                                                                                $n(c.temperature.v, 'decimal', {
                                                                                    minimumFractionDigits:
                                                                                        c.temperature.d,
                                                                                    maximumFractionDigits:
                                                                                        c.temperature.d,
                                                                                })
                                                                            }}
                                                                        </td>
                                                                        <td>{{ c.temperature.u }}</td>
                                                                    </tr>
                                                                </tbody>
                                                            </table>
                                                        </div>
                                                    </div>
                                                </div>

                                                <!-- Protection parameters (collapsible) -->
                                                <div
                                                    class="col-12"
                                                    v-if="mod.parameters && Object.keys(mod.parameters).length > 0"
                                                >
                                                    <div class="accordion">
                                                        <div class="accordion-item">
                                                            <h2 class="accordion-header">
                                                                <button
                                                                    class="accordion-button collapsed"
                                                                    type="button"
                                                                    data-bs-toggle="collapse"
                                                                    :data-bs-target="
                                                                        '#module-params-' + mod.moduleNumber
                                                                    "
                                                                >
                                                                    {{ $t('battery.parameters') }}
                                                                </button>
                                                            </h2>
                                                            <div
                                                                :id="'module-params-' + mod.moduleNumber"
                                                                class="accordion-collapse collapse"
                                                            >
                                                                <div class="accordion-body p-0">
                                                                    <div class="table-responsive">
                                                                        <table
                                                                            class="table table-striped table-hover mb-0"
                                                                        >
                                                                            <tbody>
                                                                                <tr
                                                                                    v-for="(
                                                                                        prop, key
                                                                                    ) in mod.parameters"
                                                                                    :key="key"
                                                                                >
                                                                                    <th scope="row">
                                                                                        {{ $t('battery.' + key) }}
                                                                                    </th>
                                                                                    <td class="value">
                                                                                        {{
                                                                                            $n(prop.v, 'decimal', {
                                                                                                minimumFractionDigits:
                                                                                                    prop.d,
                                                                                                maximumFractionDigits:
                                                                                                    prop.d,
                                                                                            })
                                                                                        }}
                                                                                    </td>
                                                                                    <td>{{ prop.u }}</td>
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
                                    </div>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { Battery, BatteryModule } from '@/types/BatteryDataStatus';
import { isStringValue } from '@/types/StringValue';
import { handleResponse, authHeader, authUrl } from '@/utils/authentication';
import DataAgeDisplay from '@/components/DataAgeDisplay.vue';
import WebSocketService from '@/utils/websocketService';

export default defineComponent({
    components: {
        DataAgeDisplay,
    },
    data() {
        return {
            socket: {} as WebSocketService,
            dataAgeInterval: 0,
            dataLoading: true,
            batteryData: {} as Battery,
        };
    },
    created() {
        this.getInitialData();
        this.initSocket();
        this.initDataAgeing();
    },
    unmounted() {
        this.socket?.close();
        clearInterval(this.dataAgeInterval);
    },
    computed: {
        maxIssueValue() {
            return 'issues' in this.batteryData ? Math.max(...Object.values(this.batteryData.issues)) : 0;
        },
        sortedModules(): BatteryModule[] {
            if (!this.batteryData.modules) return [];
            return this.batteryData.modules.slice().sort((a, b) => a.moduleNumber - b.moduleNumber);
        },
        firstModuleNumber(): number {
            return this.sortedModules.length > 0 ? this.sortedModules[0]!.moduleNumber : -1;
        },
    },
    methods: {
        isStringValue,
        getInitialData() {
            console.log('Get initalData for Battery');
            this.dataLoading = true;

            fetch('/api/batterylivedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryData = data;
                    this.dataLoading = false;
                });
        },
        handleMessage(event: MessageEvent) {
            console.log(event);
            this.batteryData = JSON.parse(event.data);
            this.dataLoading = false;
        },
        initSocket() {
            console.log('Starting connection to Battery WebSocket Server');

            const { protocol, host } = location;
            const authString = authUrl();
            const webSocketUrl = `${protocol === 'https:' ? 'wss' : 'ws'}://${authString}${host}/batterylivedata`;

            this.socket = new WebSocketService(webSocketUrl, {
                onMessage: this.handleMessage,
                onOpen: () => {
                    console.log('Battery WebSocket connected');
                },
                onClose: () => {
                    console.log('Battery WebSocket closed');
                },
            });

            // Listen to window events , When the window closes , Take the initiative to disconnect websocket Connect
            window.onbeforeunload = () => {
                this.socket?.close();
            };

            this.socket?.connect();
        },
        initDataAgeing() {
            this.dataAgeInterval = setInterval(() => {
                if (this.batteryData) {
                    this.batteryData.data_age++;
                }
            }, 1000);
        },
    },
});
</script>
