<template>
    <div class="text-center" v-if="dataLoading">
        <div class="spinner-border" role="status">
            <span class="visually-hidden">Loading...</span>
        </div>
    </div>

    <div v-else-if="'values' in batteryData">
        <!-- ── Battery overview card ──────────────────────────────────────── -->
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
                                <!-- single module: the module is the battery, its identity goes here -->
                                <div style="padding-right: 2em" v-if="singleModule?.moduleSerialNumber">
                                    S/N: {{ singleModule.moduleSerialNumber }}
                                </div>
                                <div style="padding-right: 2em" v-if="singleModule?.swversion">
                                    {{ $t('battery.FwVersion') }}: {{ singleModule.swversion }}
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
                                v-for="(values, section) in batterySections"
                                v-bind:key="section"
                                class="col-auto order-0"
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

                            <!-- ── Per-module section ──────────────────────────────────────────── -->
                            <!-- module selector: same pills as the inverter selector. Only the
                                 selected module is rendered. A single module is one more card of
                                 the battery (only its cells table is left), several modules get a
                                 row of their own. -->
                            <div v-if="activeModule" :class="singleModule ? 'col-auto order-0' : 'col-12 order-2'">
                                <div class="row gy-3 mt-0">
                                    <div class="col-sm-3 col-md-2" v-if="sortedModules.length > 1">
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
                                                :class="{ active: mod.moduleNumber === activeModuleNumber }"
                                                :id="'battery-module-tab-' + mod.moduleNumber"
                                                @click="selectModule(mod.moduleNumber)"
                                                type="button"
                                                role="tab"
                                                aria-controls="battery-module-content"
                                                :aria-selected="mod.moduleNumber === activeModuleNumber"
                                            >
                                                <div class="d-flex align-items-center">
                                                    <div class="me-2">
                                                        <span class="badge" :class="moduleStateClass(mod)">
                                                            <template v-if="mod.SoC !== undefined">
                                                                {{ $n(mod.SoC, 'decimalNoDigits') }} %
                                                            </template>
                                                            <template v-else>-</template>
                                                        </span>
                                                    </div>
                                                    <div class="ms-auto me-auto">{{ mod.moduleName }}</div>
                                                </div>
                                            </button>
                                        </div>
                                    </div>
                                    <div
                                        :class="sortedModules.length > 1 ? 'col-sm-9 col-md-10' : 'col-12'"
                                        id="battery-module-content"
                                        role="tabpanel"
                                    >
                                        <div class="card" :class="{ 'border-0': singleModule }">
                                            <div
                                                v-if="!singleModule"
                                                class="card-header d-flex flex-wrap align-items-center column-gap-4"
                                                :class="moduleStateClass(activeModule)"
                                            >
                                                <span>{{ activeModule.moduleName }}</span>
                                                <span v-if="activeModule.moduleSerialNumber"
                                                    >S/N: {{ activeModule.moduleSerialNumber }}</span
                                                >
                                                <span v-if="activeModule.swversion"
                                                    >{{ $t('battery.FwVersion') }}: {{ activeModule.swversion }}</span
                                                >
                                            </div>
                                            <div
                                                class="card-body"
                                                :class="{ 'p-0': singleModule }"
                                                v-for="mod in [activeModule]"
                                                :key="mod.moduleNumber"
                                            >
                                                <div class="row flex-row flex-wrap align-items-start g-3">
                                                    <!-- small cards wrap next to the (tall) cells table -->
                                                    <div
                                                        class="col-12 module-grid"
                                                        :class="{ 'd-block': singleModule }"
                                                    >
                                                        <div>
                                                            <div class="row g-3 align-items-start">
                                                                <!-- Module-level value card -->
                                                                <div
                                                                    class="col-auto"
                                                                    v-for="(values, section) in moduleSections(mod)"
                                                                    :key="section"
                                                                >
                                                                    <div
                                                                        class="card card-table border-info"
                                                                        style="overflow: hidden"
                                                                    >
                                                                        <div class="card-header text-bg-info">
                                                                            {{ $t('battery.' + section) }}
                                                                        </div>
                                                                        <div class="table-responsive">
                                                                            <table
                                                                                class="table table-striped table-hover mb-0"
                                                                            >
                                                                                <tbody>
                                                                                    <tr
                                                                                        v-for="(prop, key) in values"
                                                                                        :key="key"
                                                                                    >
                                                                                        <th scope="row">
                                                                                            {{ $t('battery.' + key) }}
                                                                                        </th>
                                                                                        <td class="value">
                                                                                            <template
                                                                                                v-if="
                                                                                                    isStringValue(
                                                                                                        prop
                                                                                                    ) && prop.translate
                                                                                                "
                                                                                            >
                                                                                                {{
                                                                                                    $t(
                                                                                                        'battery.' +
                                                                                                            prop.value
                                                                                                    )
                                                                                                }}
                                                                                            </template>
                                                                                            <template
                                                                                                v-else-if="
                                                                                                    isStringValue(prop)
                                                                                                "
                                                                                            >
                                                                                                {{ prop.value }}
                                                                                            </template>
                                                                                            <template v-else>
                                                                                                {{
                                                                                                    $n(
                                                                                                        prop.v,
                                                                                                        'decimal',
                                                                                                        {
                                                                                                            minimumFractionDigits:
                                                                                                                prop.d,
                                                                                                            maximumFractionDigits:
                                                                                                                prop.d,
                                                                                                        }
                                                                                                    )
                                                                                                }}
                                                                                            </template>
                                                                                        </td>
                                                                                        <td>
                                                                                            <template
                                                                                                v-if="
                                                                                                    !isStringValue(prop)
                                                                                                "
                                                                                            >
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
                                                                <div
                                                                    class="col-auto"
                                                                    v-if="mod.cellStatus && !singleModule"
                                                                >
                                                                    <div
                                                                        class="card card-table border-info"
                                                                        style="overflow: hidden"
                                                                    >
                                                                        <div class="card-header text-bg-info">
                                                                            {{ $t('battery.cell_status') }}
                                                                        </div>
                                                                        <div class="table-responsive">
                                                                            <table
                                                                                class="table table-striped table-hover mb-0"
                                                                            >
                                                                                <tbody>
                                                                                    <tr
                                                                                        v-for="(
                                                                                            prop, key
                                                                                        ) in mod.cellStatus"
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

                                                        <!-- Per-cell table: cells are compact number rows, unit and decimals
                                                         come from cellColumns (keeps the JSON built on the ESP small) -->
                                                        <div
                                                            class="module-grid-cells"
                                                            v-if="mod.cells && mod.cells.length > 0"
                                                        >
                                                            <div
                                                                class="card card-table border-info"
                                                                style="overflow: hidden"
                                                            >
                                                                <div class="card-header text-bg-info">
                                                                    {{ $t('battery.cellTable') }}
                                                                </div>
                                                                <div class="table-responsive">
                                                                    <table class="table table-striped table-hover mb-0">
                                                                        <thead>
                                                                            <tr>
                                                                                <th scope="col">
                                                                                    {{ $t('battery.cell') }}
                                                                                </th>
                                                                                <template
                                                                                    v-for="col in mod.cellColumns"
                                                                                    :key="col.name"
                                                                                >
                                                                                    <th scope="col" class="value">
                                                                                        {{ $t('battery.' + col.name) }}
                                                                                    </th>
                                                                                    <th scope="col"></th>
                                                                                </template>
                                                                            </tr>
                                                                        </thead>
                                                                        <tbody>
                                                                            <tr
                                                                                v-for="(row, idx) in mod.cells"
                                                                                :key="idx"
                                                                            >
                                                                                <th scope="row">
                                                                                    {{ idx + 1 }}
                                                                                    <span
                                                                                        v-if="
                                                                                            ((mod.balancing ?? 0) >>
                                                                                                idx) &
                                                                                            1
                                                                                        "
                                                                                        class="badge text-bg-info ms-1"
                                                                                        :title="
                                                                                            $t(
                                                                                                'battery.balancingActive'
                                                                                            )
                                                                                        "
                                                                                        >⚖</span
                                                                                    >
                                                                                </th>
                                                                                <template
                                                                                    v-for="(col, ci) in mod.cellColumns"
                                                                                    :key="col.name"
                                                                                >
                                                                                    <td class="value">
                                                                                        {{
                                                                                            $n(
                                                                                                row[ci] ?? 0,
                                                                                                'decimal',
                                                                                                {
                                                                                                    minimumFractionDigits:
                                                                                                        col.d,
                                                                                                    maximumFractionDigits:
                                                                                                        col.d,
                                                                                                }
                                                                                            )
                                                                                        }}
                                                                                    </td>
                                                                                    <td>{{ col.u }}</td>
                                                                                </template>
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
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import type { Battery, BatteryModule, BatteryModuleRaw, CellColumn } from '@/types/BatteryDataStatus';
import { isStringValue } from '@/types/StringValue';
import type { StringValue } from '@/types/StringValue';
import type { ValueObject } from '@/types/LiveDataStatus';

// positions of the module arrays sent by the ESP (see Pytes RS485 Stats.cpp),
// d = decimals, -1 = yes/no flag
const moduleColumns: { values: CellColumn[]; cellStatus: CellColumn[]; cells: CellColumn[] } = {
    values: [
        { name: 'SoC', u: '%', d: 2 },
        { name: 'voltage', u: 'V', d: 2 },
        { name: 'current', u: 'A', d: 3 },
        { name: 'dischargeCurrentLimitation', u: 'A', d: 1 },
        { name: 'chargeCurrentLimitation', u: 'A', d: 1 },
        { name: 'chargeVoltage', u: 'V', d: 1 },
        { name: 'dischargeVoltageLimitation', u: 'V', d: 1 },
        { name: 'stateOfHealth', u: '%', d: 0 },
        { name: 'chargeCycles', u: '', d: 0 },
        { name: 'temperature', u: '°C', d: 1 },
        { name: 'capacity', u: 'Ah', d: 2 },
        { name: 'availableCapacity', u: 'Ah', d: 2 },
        { name: 'chargeImmediately', u: '', d: -1 },
        { name: 'fullChargeRequest', u: '', d: -1 },
        { name: 'balancingActive', u: '', d: -1 },
    ],
    cellStatus: [
        { name: 'cellMinVoltage', u: 'V', d: 3 },
        { name: 'cellMaxVoltage', u: 'V', d: 3 },
        { name: 'cellDiffVoltage', u: 'mV', d: 0 },
        { name: 'cellMinTemperature', u: '°C', d: 0 },
        { name: 'cellMaxTemperature', u: '°C', d: 0 },
        { name: 'cellMinVoltageName', u: '', d: 0 },
        { name: 'cellMaxVoltageName', u: '', d: 0 },
        { name: 'cellMinTemperatureName', u: '', d: 0 },
        { name: 'cellMaxTemperatureName', u: '', d: 0 },
    ],
    cells: [
        { name: 'voltage', u: 'V', d: 3 },
        { name: 'SoC', u: '%', d: 0 },
        { name: 'temperature', u: '°C', d: 0 },
    ],
};
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
            selectedModuleNumber: -1,
            // details are sent one module per message, see handleMessage()
            moduleDetails: {} as Record<number, BatteryModuleRaw>,
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
            if (!('issues' in this.batteryData)) return 0;
            return Math.max(0, ...Object.values(this.batteryData.issues));
        },
        sortedModules(): BatteryModule[] {
            if (!this.batteryData.modules) return [];
            const cols = moduleColumns;
            return this.batteryData.modules
                .slice()
                .sort((a, b) => a.moduleNumber - b.moduleNumber)
                .map((summary) => {
                    const mod = this.moduleDetails[summary.moduleNumber];
                    // turn the compact arrays sent by the ESP back into value cards
                    const values: BatteryModule['values'] = {};
                    (mod?.values ?? []).forEach((v, i) => {
                        const col = cols.values[i];
                        if (!col || v === null) return;
                        values[col.name] =
                            typeof v === 'boolean'
                                ? { value: v ? 'yes' : 'no', translate: true }
                                : { v, u: col.u, d: col.d };
                    });
                    let cellStatus: BatteryModule['cellStatus'];
                    if (mod?.cellStatus) {
                        cellStatus = {};
                        mod.cellStatus.forEach((v, i) => {
                            const col = cols.cellStatus[i];
                            if (col) cellStatus![col.name] = { v, u: col.u, d: col.d };
                        });
                    }
                    return { ...mod, ...summary, values, cellStatus, cellColumns: cols.cells };
                });
        },
        // a battery made of a single module: module and battery values are the
        // same, the module is not shown as a card of its own
        singleModule(): BatteryModule | undefined {
            return this.sortedModules.length === 1 ? this.sortedModules[0] : undefined;
        },
        batterySections(): Record<string, Record<string, ValueObject | StringValue>> {
            const sections = this.batteryData.values as unknown as Record<
                string,
                Record<string, ValueObject | StringValue>
            >;
            if (!this.singleModule || !sections) return sections;
            // values only reported per module (e.g. charge cycles) join the battery status,
            // the module counts ("Battery modules") say nothing for a single module
            const result: Record<string, Record<string, ValueObject | StringValue>> = {
                ...sections,
                status: { ...sections.status },
            };
            delete result.modules;
            for (const [key, value] of Object.entries(this.singleModule.values)) {
                if (!(key in result.status!)) result.status![key] = value as ValueObject | StringValue;
            }
            return result;
        },
        activeModule(): BatteryModule | undefined {
            return this.sortedModules.find((m) => m.moduleNumber === this.activeModuleNumber);
        },
        activeModuleNumber(): number {
            if (this.sortedModules.some((m) => m.moduleNumber === this.selectedModuleNumber)) {
                return this.selectedModuleNumber;
            }
            return this.sortedModules.length > 0 ? this.sortedModules[0]!.moduleNumber : -1;
        },
    },
    methods: {
        isStringValue,
        // same scheme as the inverter view: red = unreachable, yellow = reachable but not ok, green = ok
        moduleStateClass(mod: BatteryModule) {
            if (mod.online === false) return 'text-bg-danger';
            if (mod.error) return 'text-bg-warning';
            return 'text-bg-success';
        },
        moduleSections(mod: BatteryModule) {
            if (this.singleModule) return {};
            const sections = { status: mod.values };
            return Object.fromEntries(Object.entries(sections).filter(([, v]) => v && Object.keys(v).length > 0));
        },
        getInitialData() {
            console.log('Get initalData for Battery');
            this.dataLoading = true;

            fetch('/api/batterylivedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.batteryData = data;
                    this.dataLoading = false;
                    this.fetchModule(this.activeModuleNumber);
                });
        },
        // the websocket only sends modules that changed, fetch the selected one right away
        fetchModule(moduleNumber: number) {
            if (moduleNumber < 1) return;
            fetch(`/api/batterylivedata/status?module=${moduleNumber}`, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    if (data?.module) this.moduleDetails[data.module.moduleNumber] = data.module;
                });
        },
        selectModule(moduleNumber: number) {
            this.selectedModuleNumber = moduleNumber;
            if (!this.moduleDetails[moduleNumber]) this.fetchModule(moduleNumber);
        },
        handleMessage(event: MessageEvent) {
            console.log(event);
            const data = JSON.parse(event.data);
            if ('module' in data) {
                this.moduleDetails[data.module.moduleNumber] = data.module;
                return;
            }
            this.batteryData = data;
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

<style scoped>
/* value cards take up to their natural width and wrap when space runs out, the cells table sits right next to them */
.module-grid {
    display: grid;
    gap: 1rem;
    grid-template-columns: minmax(0, max-content) auto;
    justify-content: start;
    align-items: start;
}

@media (max-width: 991.98px) {
    .module-grid {
        grid-template-columns: minmax(0, 1fr);
    }

    .module-grid-cells {
        justify-self: start;
    }
}

/* single module: only the cells table is left, keep it as narrow as its content */
.module-grid.d-block > .module-grid-cells {
    display: inline-block;
    max-width: 100%;
}

/* phones: nested cards eat a lot of width, use less padding and a smaller
   cells table so it fits the screen (it still scrolls inside its card if not) */
@media (max-width: 575.98px) {
    .card-body {
        padding: 0.375rem;
    }

    .module-grid-cells table {
        font-size: 0.875rem;
    }

    .module-grid-cells {
        justify-self: stretch;
        min-width: 0;
    }

    .module-grid-cells th,
    .module-grid-cells td {
        padding: 0.25rem 0.2rem;
    }
}
</style>
