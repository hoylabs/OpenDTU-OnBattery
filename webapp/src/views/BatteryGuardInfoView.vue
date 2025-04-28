<template>
    <BasePage
        :title="$t('batteryguardinfo.BatteryGuardInformation')"
        :isLoading="dataLoading"
        :show-reload="true"
        @reload="getBatteryGuardInfo"
    >
        <CardElement :text="$t('batteryguardinfo.Status')" textVariant="text-bg-primary" add-space table>
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

                            <template v-if="batteryGuardStatus.enabled">
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.OpenCircuitVoltage') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.open_circuit_voltage, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>V</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceConfigured') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.resistance_configured, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mOhm</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceCalculated') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.resistance_calculated, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mOhm</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceCalculationState') }}</th>
                                    <td class="value">{{ batteryGuardStatus.resistance_calculation_state }}</td>
                                    <td></td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.ResistanceCalculationCount') }}</th>
                                    <td class="value">{{ batteryGuardStatus.resistance_calculation_count }}</td>
                                    <td></td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.MeasurementTimePeriod') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.measurement_time_period, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 0,
                                            })
                                        }}
                                    </td>
                                    <td>ms</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.BatteryDataSufficient') }}</th>
                                    <td class="value">
                                        <StatusBadge
                                            :status="batteryGuardStatus.battery_data_sufficient"
                                            true_text="batteryguardinfo.Yes"
                                            false_text="batteryguardinfo.No"
                                        />
                                    </td>
                                    <td></td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.VoltageResolution') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.voltage_resolution, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mV</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.CurrentResolution') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.current_resolution, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 2,
                                            })
                                        }}
                                    </td>
                                    <td>mA</td>
                                </tr>
                                <tr>
                                    <th scope="row">{{ $t('batteryguardinfo.VIStampDelay') }}</th>
                                    <td class="value">
                                        {{
                                            $n(batteryGuardStatus.v_i_time_stamp_delay, 'decimal', {
                                                minimumFractionDigits: 0,
                                                maximumFractionDigits: 0,
                                            })
                                        }}
                                    </td>
                                    <td>ms</td>
                                </tr>
                            </template>
                        </tbody>
                    </table>
                </div>
            </div>
        </CardElement>
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
