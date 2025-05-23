<template>
    <BasePage
        :title="$t('batteryguardinfo.BatteryGuardInformation')"
        :isLoading="dataLoading"
        :show-reload="true"
        @reload="getBatteryGuardInfo"
    >
        <CardElement :text="$t('batteryguardinfo.General')" textVariant="text-bg-primary" add-space table>
            <div class="card-body">
                <div class="table-responsive">
                    <table class="table table-striped table-hover">
                        <tbody>
                            <tr>
                                <th scope="row">{{ $t('batteryguardinfo.Status') }}</th>
                                <td class="value">
                                    <StatusBadge
                                        :status="batteryGuardStatus.enabled"
                                        true_text="mqttinfo.Enabled"
                                        false_text="mqttinfo.Disabled"
                                    />
                                </td>
                                <td></td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </CardElement>

        <template v-if="batteryGuardStatus.enabled">
            <CardElement :text="$t('batteryguardinfo.BatteryVoltage')" textVariant="text-bg-primary" add-space table>
                <div class="card-body">
                    <div class="table-responsive">
                        <table class="table table-striped table-hover">
                            <tbody>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.OpenCircuitVoltage') }}</th>
                                    <template v-if="batteryGuardStatus.values.open_circuit_voltage_calculated">
                                        <td class="value">
                                            {{
                                                $n(batteryGuardStatus.values.open_circuit_voltage, 'decimal', {
                                                    minimumFractionDigits: 0,
                                                    maximumFractionDigits: 2,
                                                })
                                            }}
                                        </td>
                                        <td>V</td>
                                    </template>
                                    <template v-else>
                                        <td class="value">
                                            <span class="badge text-bg-danger"> Calculation not possible </span>
                                        </td>
                                        <td></td>
                                    </template>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.UncompensatedVoltage') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.values.uncompensated_voltage, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>V</td>
                                </tr>
                            </tbody>
                        </table>
                    </div>
                </div>
            </CardElement>

            <CardElement
                :text="$t('batteryguardinfo.BatteryDCPulseResistance')"
                textVariant="text-bg-primary"
                add-space
                table
            >
                <div class="card-body">
                    <div class="table-responsive">
                        <table class="table table-striped table-hover">
                            <tbody>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceConfigured') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.values.resistance_configured, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mOhm</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceCalculated') }}</th>
                                    <template v-if="batteryGuardStatus.values.internal_resistance_calculated">
                                        <td class="value">
                                            {{
                                                $n(batteryGuardStatus.values.resistance_calculated, 'decimal', {
                                                    minimumFractionDigits: 0,
                                                    maximumFractionDigits: 2,
                                                })
                                            }}
                                        </td>
                                        <td>mOhm</td>
                                    </template>
                                    <template v-else>
                                        <td class="value">
                                            <span class="badge text-bg-danger"> Calculation not possible </span>
                                        </td>
                                        <td></td>
                                    </template>
                                </tr>
                            </tbody>
                        </table>
                    </div>
                </div>
            </CardElement>

            <CardElement :text="$t('batteryguardinfo.MeasurementData')" textVariant="text-bg-primary" add-space table>
                <div class="card-body">
                    <div class="table-responsive">
                        <table class="table table-striped table-hover">
                            <thead>
                                <tr>
                                    <th scope="col">{{ $t('batteryguardinfo.Property') }}</th>
                                    <th class="value" scope="col">
                                        {{ $t('batteryguardinfo.Limit') }}
                                    </th>
                                    <th scope="col"></th>
                                    <th class="value" scope="col">
                                        {{ $t('batteryguardinfo.Value') }}
                                    </th>
                                    <th scope="col"></th>
                                </tr>
                            </thead>
                            <tbody>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.VoltageResolution') }}</th>
                                    <td class="value">
                                        &leq;
                                        {{
                                            $n(batteryGuardStatus.limits.max_voltage_resolution, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mV</td>
                                    <td class="value">
                                        <span
                                            class="badge"
                                            :class="[
                                                batteryGuardStatus.values.voltage_resolution <=
                                                batteryGuardStatus.limits.max_voltage_resolution
                                                    ? 'text-bg-success'
                                                    : 'text-bg-danger',
                                            ]"
                                        >
                                            {{
                                                $n(batteryGuardStatus.values.voltage_resolution, 'decimal', {
                                                    minimumFractionDigits: 0,
                                                    maximumFractionDigits: 2,
                                                })
                                            }}
                                        </span>
                                    </td>
                                    <td>mV</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.CurrentResolution') }}</th>
                                    <td class="value">
                                        &leq;
                                        {{
                                            $n(batteryGuardStatus.limits.max_current_resolution, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mA</td>
                                    <td class="value">
                                        <span
                                            class="badge"
                                            :class="[
                                                batteryGuardStatus.values.current_resolution <=
                                                batteryGuardStatus.limits.max_current_resolution
                                                    ? 'text-bg-success'
                                                    : 'text-bg-danger',
                                            ]"
                                        >
                                            {{
                                                $n(batteryGuardStatus.values.current_resolution, 'decimal', {
                                                    minimumFractionDigits: 0,
                                                    maximumFractionDigits: 2,
                                                })
                                            }}
                                        </span>
                                    </td>
                                    <td>mA</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.MeasurementTimePeriod') }}</th>
                                    <td class="value">
                                        &leq;
                                        {{
                                            $n(batteryGuardStatus.limits.max_measurement_time_period, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 0,
                                            })
                                        }}
                                    </td>
                                    <td>ms</td>
                                    <td class="value">
                                        <span
                                            class="badge"
                                            :class="[
                                                batteryGuardStatus.values.measurement_time_period <=
                                                batteryGuardStatus.limits.max_measurement_time_period
                                                    ? 'text-bg-success'
                                                    : 'text-bg-danger',
                                            ]"
                                        >
                                            {{
                                                $n(batteryGuardStatus.values.measurement_time_period, 'decimal', {
                                                    minimumFractionDigits: 0,
                                                    maximumFractionDigits: 0,
                                                })
                                            }}
                                        </span>
                                    </td>
                                    <td>ms</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.VIStampDelay') }}</th>
                                    <td class="value">
                                        &leq;
                                        {{
                                            $n(batteryGuardStatus.limits.max_v_i_time_stamp_delay, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 0,
                                            })
                                        }}
                                    </td>
                                    <td>ms</td>
                                    <td class="value">
                                        <span
                                            class="badge"
                                            :class="[
                                                batteryGuardStatus.values.v_i_time_stamp_delay <=
                                                batteryGuardStatus.limits.max_v_i_time_stamp_delay
                                                    ? 'text-bg-success'
                                                    : 'text-bg-danger',
                                            ]"
                                        >
                                            {{
                                                $n(batteryGuardStatus.values.v_i_time_stamp_delay, 'decimal', {
                                                    minimumFractionDigits: 0,
                                                    maximumFractionDigits: 0,
                                                })
                                            }}
                                        </span>
                                    </td>
                                    <td>ms</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceCalculationCount') }}</th>
                                    <td class="value">
                                        &geq;
                                        {{ batteryGuardStatus.limits.min_resistance_calculation_count }}
                                    </td>
                                    <td></td>
                                    <td class="value">
                                        <span
                                            class="badge"
                                            :class="[
                                                batteryGuardStatus.values.resistance_calculation_count > 0
                                                    ? batteryGuardStatus.values.resistance_calculation_count >=
                                                      batteryGuardStatus.limits.min_resistance_calculation_count
                                                        ? 'text-bg-success'
                                                        : 'text-bg-warning'
                                                    : 'text-bg-danger',
                                            ]"
                                        >
                                            {{ batteryGuardStatus.values.resistance_calculation_count }}
                                        </span>
                                    </td>
                                    <td></td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceCalculationState') }}</th>
                                    <td class="value" colspan="3">
                                        <span
                                            class="badge"
                                            :class="[
                                                batteryGuardStatus.values.resistance_calculation_count > 0
                                                    ? batteryGuardStatus.values.internal_resistance_calculated
                                                        ? 'text-bg-success'
                                                        : 'text-bg-warning'
                                                    : 'text-bg-danger',
                                            ]"
                                        >
                                            {{ batteryGuardStatus.values.resistance_calculation_state }}
                                        </span>
                                    </td>
                                    <td></td>
                                </tr>
                            </tbody>
                        </table>
                    </div>
                </div>
            </CardElement>
        </template>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import CardElement from '@/components/CardElement.vue';
import StatusBadge from '@/components/StatusBadge.vue';
import type { BatteryGuardStatus } from '@/types/BatteryGuardStatus';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        CardElement,
        StatusBadge,
    },
    data() {
        return {
            dataLoading: false,
            batteryGuardStatus: {} as BatteryGuardStatus,
        };
    },
    created() {
        this.getBatteryGuardInfo();
    },
    methods: {
        getBatteryGuardInfo() {
            this.dataLoading = true;
            fetch('/api/batteryguard/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.dataLoading = false;
                    this.batteryGuardStatus = data;
                });
        },
    },
});
</script>
