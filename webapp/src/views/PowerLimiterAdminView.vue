<template>
    <BasePage :title="$t('powerlimiteradmin.PowerLimiterSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <BootstrapAlert v-model="configAlert" variant="warning">
            {{ $t('powerlimiteradmin.ConfigAlertMessage') }}
        </BootstrapAlert>

        <CardElement
            :text="$t('powerlimiteradmin.ConfigHints')"
            textVariant="text-bg-primary"
            v-if="getConfigHints().length"
        >
            <div class="row">
                <div class="col-sm-12">
                    {{ $t('powerlimiteradmin.ConfigHintsIntro') }}
                    <ul class="mb-0">
                        <li v-for="(hint, idx) in getConfigHints()" :key="idx">
                            <b v-if="hint.severity === 'requirement'"
                                >{{ $t('powerlimiteradmin.ConfigHintRequirement') }}:</b
                            >
                            <b v-if="hint.severity === 'optional'">{{ $t('powerlimiteradmin.ConfigHintOptional') }}:</b>
                            {{ $t('powerlimiteradmin.ConfigHint' + hint.subject) }}
                        </li>
                    </ul>
                </div>
            </div>
        </CardElement>

        <form @submit="savePowerLimiterConfig" v-if="!configAlert">
            <CardElement :text="$t('powerlimiteradmin.General')" textVariant="text-bg-primary" add-space>
                <InputElement
                    :label="$t('powerlimiteradmin.Enable')"
                    v-model="powerLimiterConfigList.enabled"
                    type="checkbox"
                    wide
                />

                <InputElement
                    v-show="isEnabled()"
                    :label="$t('powerlimiteradmin.VerboseLogging')"
                    v-model="powerLimiterConfigList.verbose_logging"
                    type="checkbox"
                    wide
                />

                <InputElement
                    v-show="isEnabled() && hasPowerMeter()"
                    :label="$t('powerlimiteradmin.TargetPowerConsumption')"
                    :tooltip="$t('powerlimiteradmin.TargetPowerConsumptionHint')"
                    v-model="powerLimiterConfigList.target_power_consumption"
                    postfix="W"
                    type="number"
                    wide
                />

                <InputElement
                    v-show="isEnabled()"
                    :label="$t('powerlimiteradmin.BaseLoadLimit')"
                    :tooltip="$t('powerlimiteradmin.BaseLoadLimitHint')"
                    v-model="powerLimiterConfigList.base_load_limit"
                    placeholder="200"
                    postfix="W"
                    type="number"
                    min="0"
                    wide
                />

                <InputElement
                    v-show="isEnabled()"
                    :label="$t('powerlimiteradmin.TargetPowerConsumptionHysteresis')"
                    :tooltip="$t('powerlimiteradmin.TargetPowerConsumptionHysteresisHint')"
                    v-model="powerLimiterConfigList.target_power_consumption_hysteresis"
                    postfix="W"
                    type="number"
                    min="1"
                    wide
                />

                <InputElement
                    v-show="isEnabled()"
                    :label="$t('powerlimiteradmin.TotalUpperPowerLimit')"
                    :tooltip="$t('powerlimiteradmin.TotalUpperPowerLimitHint')"
                    v-model="powerLimiterConfigList.total_upper_power_limit"
                    postfix="W"
                    type="number"
                    min="1"
                    wide
                />
            </CardElement>

            <CardElement
                :text="$t('powerlimiteradmin.ManagedInverters')"
                textVariant="text-bg-primary"
                add-space
                v-if="isEnabled()"
            >
                <div class="row mb-3" v-if="unmanagedInverters.length > 0">
                    <label for="add_inverter" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.AddInverter') }}
                    </label>
                    <div class="col-sm-7">
                        <select id="add_inverter" class="form-select" v-model="additionalInverterSerial">
                            <option v-for="serial in unmanagedInverters" :key="serial" :value="serial">
                                {{ inverterLabel(serial) }}
                            </option>
                        </select>
                    </div>
                    <div class="col-sm-1">
                        <button type="button" class="btn btn-success w-100" @click="addInverter">
                            <BIconDatabaseAdd />
                        </button>
                    </div>
                </div>
                <div class="table-responsive" v-if="powerLimiterConfigList.inverters.length > 0">
                    <table class="table">
                        <thead>
                            <tr>
                                <th>{{ $t('powerlimiteradmin.InverterLabel') }}</th>
                                <th>{{ $t('powerlimiteradmin.PowerSource') }}</th>
                                <th>{{ $t('powerlimiteradmin.LowerPowerLimit') }}</th>
                                <th>{{ $t('powerlimiteradmin.UpperPowerLimit') }}</th>
                                <th></th>
                            </tr>
                        </thead>
                        <tbody>
                            <tr v-for="inverter in powerLimiterConfigList.inverters" v-bind:key="inverter.serial">
                                <td>{{ inverterLabel(inverter.serial) }}</td>
                                <td v-if="inverter.is_solar_powered">
                                    {{ $t('powerlimiteradmin.PowerSourceSolarPanels') }}
                                </td>
                                <td v-else>{{ $t('powerlimiteradmin.PowerSourceBattery') }}</td>
                                <td>{{ inverter.lower_power_limit }}</td>
                                <td>{{ inverter.upper_power_limit }}</td>
                                <td>
                                    <span
                                        role="button"
                                        class="text-danger"
                                        @click="deleteStart(inverter)"
                                        :title="$t('powerlimiteradmin.DeleteInverter')"
                                    >
                                        <BIconTrash /> </span
                                    >&nbsp;
                                    <span
                                        role="button"
                                        class="text-primary"
                                        @click="editStart(inverter)"
                                        :title="$t('powerlimiteradmin.EditInverter')"
                                    >
                                        <BIconPencil />
                                    </span>
                                </td>
                            </tr>
                        </tbody>
                    </table>
                </div>
                <div v-else class="alert alert-warning" role="alert">
                    {{ $t('powerlimiteradmin.NoManagedInverters') }}
                </div>
            </CardElement>

            <CardElement
                :text="$t('powerlimiteradmin.InverterSettings')"
                textVariant="text-bg-primary"
                add-space
                v-if="isEnabled() && batteryPoweredInverterConfigured()"
            >
                <InputElement
                    :label="$t('powerlimiteradmin.BatteryDischargeAtNight')"
                    v-model="powerLimiterConfigList.battery_always_use_at_night"
                    type="checkbox"
                    wide
                />

                <div class="row mb-3">
                    <label for="inverter_serial_for_dc_voltage" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.InverterForDcVoltage') }}
                    </label>
                    <div class="col-sm-8">
                        <select
                            id="inverter_serial_for_dc_voltage"
                            class="form-select"
                            v-model="powerLimiterConfigList.inverter_serial_for_dc_voltage"
                            required
                        >
                            <option value="" disabled hidden selected>
                                {{ $t('powerlimiteradmin.SelectInverter') }}
                            </option>
                            <option v-for="inv in batteryPoweredInverters" :key="inv.serial" :value="inv.serial">
                                {{ inverterLabel(inv.serial) }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3" v-if="needsChannelSelection()">
                    <label for="inverter_channel" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.InverterChannelId') }}
                    </label>
                    <div class="col-sm-8">
                        <select
                            id="inverter_channel"
                            class="form-select"
                            v-model="powerLimiterConfigList.inverter_channel_id_for_dc_voltage"
                        >
                            <option
                                v-for="channel in range(
                                    powerLimiterMetaData.inverters[
                                        powerLimiterConfigList.inverter_serial_for_dc_voltage
                                    ].channels
                                )"
                                :key="channel"
                                :value="channel"
                            >
                                {{ channel + 1 }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inverter_restart" class="col-sm-4 col-form-label">
                        {{ $t('powerlimiteradmin.InverterRestartHour') }}
                        <BIconInfoCircle v-tooltip :title="$t('powerlimiteradmin.InverterRestartHint')" />
                    </label>
                    <div class="col-sm-8">
                        <select
                            id="inverter_restart"
                            class="form-select"
                            v-model="powerLimiterConfigList.inverter_restart_hour"
                        >
                            <option value="-1">
                                {{ $t('powerlimiteradmin.InverterRestartDisabled') }}
                            </option>
                            <option v-for="hour in range(24)" :key="hour" :value="hour">
                                {{ hour > 9 ? hour : '0' + hour }}:00
                            </option>
                        </select>
                    </div>
                </div>
            </CardElement>

            <CardElement
                :text="$t('powerlimiteradmin.SolarPassthrough')"
                textVariant="text-bg-primary"
                add-space
                v-if="canUseSolarPassthrough()"
            >
                <div
                    class="alert alert-secondary"
                    role="alert"
                    v-html="$t('powerlimiteradmin.SolarpassthroughInfo')"
                ></div>

                <InputElement
                    :label="$t('powerlimiteradmin.EnableSolarPassthrough')"
                    v-model="powerLimiterConfigList.solar_passthrough_enabled"
                    type="checkbox"
                    wide
                />

                <div v-if="powerLimiterConfigList.solar_passthrough_enabled">
                    <InputElement
                        :label="$t('powerlimiteradmin.SolarPassthroughLosses')"
                        v-model="powerLimiterConfigList.solar_passthrough_losses"
                        placeholder="3"
                        min="0"
                        max="10"
                        postfix="%"
                        type="number"
                        wide
                    />

                    <div
                        class="alert alert-secondary"
                        role="alert"
                        v-html="$t('powerlimiteradmin.SolarPassthroughLossesInfo')"
                    ></div>
                </div>
            </CardElement>

            <CardElement
                :text="$t('powerlimiteradmin.SocThresholds')"
                textVariant="text-bg-primary"
                add-space
                v-if="canUseSoCThresholds()"
            >
                <InputElement
                    :label="$t('powerlimiteradmin.IgnoreSoc')"
                    v-model="powerLimiterConfigList.ignore_soc"
                    type="checkbox"
                    wide
                />

                <div v-if="!powerLimiterConfigList.ignore_soc">
                    <div
                        class="alert alert-secondary"
                        role="alert"
                        v-html="$t('powerlimiteradmin.BatterySocInfo')"
                    ></div>

                    <InputElement
                        :label="$t('powerlimiteradmin.StartThreshold')"
                        v-model="powerLimiterConfigList.battery_soc_start_threshold"
                        placeholder="80"
                        min="0"
                        max="100"
                        postfix="%"
                        type="number"
                        wide
                    />

                    <InputElement
                        :label="$t('powerlimiteradmin.StopThreshold')"
                        v-model="powerLimiterConfigList.battery_soc_stop_threshold"
                        placeholder="20"
                        min="0"
                        max="100"
                        postfix="%"
                        type="number"
                        wide
                    />

                    <InputElement
                        :label="$t('powerlimiteradmin.FullSolarPassthroughStartThreshold')"
                        :tooltip="$t('powerlimiteradmin.FullSolarPassthroughStartThresholdHint')"
                        v-model="powerLimiterConfigList.full_solar_passthrough_soc"
                        v-if="isSolarPassthroughEnabled()"
                        placeholder="80"
                        min="0"
                        max="100"
                        postfix="%"
                        type="number"
                        wide
                    />
                </div>
            </CardElement>

            <CardElement
                :text="$t('powerlimiteradmin.VoltageThresholds')"
                textVariant="text-bg-primary"
                add-space
                v-if="canUseVoltageThresholds()"
            >
                <InputElement
                    :label="$t('powerlimiteradmin.StartThreshold')"
                    v-model="powerLimiterConfigList.voltage_start_threshold"
                    placeholder="50"
                    min="16"
                    max="66"
                    postfix="V"
                    type="number"
                    step="0.01"
                    wide
                />

                <InputElement
                    :label="$t('powerlimiteradmin.StopThreshold')"
                    v-model="powerLimiterConfigList.voltage_stop_threshold"
                    placeholder="49"
                    min="16"
                    max="66"
                    postfix="V"
                    type="number"
                    step="0.01"
                    wide
                />

                <div v-if="isSolarPassthroughEnabled()">
                    <InputElement
                        :label="$t('powerlimiteradmin.FullSolarPassthroughStartThreshold')"
                        :tooltip="$t('powerlimiteradmin.FullSolarPassthroughStartThresholdHint')"
                        v-model="powerLimiterConfigList.full_solar_passthrough_start_voltage"
                        placeholder="49"
                        min="16"
                        max="66"
                        postfix="V"
                        type="number"
                        step="0.01"
                        wide
                    />

                    <InputElement
                        :label="$t('powerlimiteradmin.VoltageSolarPassthroughStopThreshold')"
                        v-model="powerLimiterConfigList.full_solar_passthrough_stop_voltage"
                        placeholder="49"
                        min="16"
                        max="66"
                        postfix="V"
                        type="number"
                        step="0.01"
                        wide
                    />
                </div>

                <InputElement
                    :label="$t('powerlimiteradmin.VoltageLoadCorrectionFactor')"
                    v-model="powerLimiterConfigList.voltage_load_correction_factor"
                    placeholder="0.0001"
                    postfix="1/A"
                    type="number"
                    step="0.0001"
                    wide
                />

                <div
                    class="alert alert-secondary"
                    role="alert"
                    v-html="$t('powerlimiteradmin.VoltageLoadCorrectionInfo')"
                ></div>
            </CardElement>

            <FormFooter @reload="getAllData" />
        </form>
    </BasePage>

    <ModalDialog
        modalId="inverterEdit"
        :title="
            unmanagedInverters.includes(editInverter.serial)
                ? $t('powerlimiteradmin.AddInverter')
                : $t('powerlimiteradmin.EditInverter')
        "
        :closeText="$t('powerlimiteradmin.Cancel')"
    >
        <div class="row mb-3">
            <label for="inverter_label" class="col-sm-4 col-form-label">
                {{ $t('powerlimiteradmin.EditInverterLabel') }}
            </label>
            <div class="col-sm-8 col-form-label">
                {{ inverterLabel(editInverter.serial) }}
            </div>
        </div>

        <InputElement
            v-show="hasPowerMeter()"
            :label="$t('powerlimiteradmin.InverterIsBehindPowerMeter')"
            v-model="editInverter.is_behind_power_meter"
            :tooltip="$t('powerlimiteradmin.InverterIsBehindPowerMeterHint')"
            type="checkbox"
            wide
        />

        <InputElement
            :label="$t('powerlimiteradmin.InverterIsSolarPowered')"
            v-model="editInverter.is_solar_powered"
            type="checkbox"
            wide
        />

        <InputElement
            v-show="editInverter.is_solar_powered"
            :label="$t('powerlimiteradmin.UseOverscalingToCompensateShading')"
            :tooltip="$t('powerlimiteradmin.UseOverscalingToCompensateShadingHint')"
            v-model="editInverter.use_overscaling_to_compensate_shading"
            type="checkbox"
            wide
        />

        <InputElement
            :label="$t('powerlimiteradmin.LowerPowerLimit')"
            :tooltip="$t('powerlimiteradmin.LowerPowerLimitHint')"
            v-model="editInverter.lower_power_limit"
            postfix="W"
            type="number"
            wide
        />

        <InputElement
            :label="$t('powerlimiteradmin.UpperPowerLimit')"
            v-model="editInverter.upper_power_limit"
            :tooltip="$t('powerlimiteradmin.UpperPowerLimitHint')"
            postfix="W"
            type="number"
            wide
        />

        <template #footer>
            <button type="button" class="btn btn-primary" @click="editSubmit">
                {{ $t('powerlimiteradmin.Apply') }}
            </button>
        </template>
    </ModalDialog>

    <ModalDialog
        modalId="inverterDelete"
        small
        :title="$t('powerlimiteradmin.DeleteInverter')"
        :closeText="$t('powerlimiteradmin.Cancel')"
    >
        <div>
            {{
                $t('powerlimiteradmin.DeleteInverterMsg', {
                    serial: editInverter.serial,
                    label: inverterLabel(editInverter.serial),
                })
            }}
        </div>
        <template #footer>
            <button type="button" class="btn btn-danger" @click="deleteSubmit">
                {{ $t('powerlimiteradmin.Delete') }}
            </button>
        </template>
    </ModalDialog>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import { handleResponse, authHeader } from '@/utils/authentication';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import ModalDialog from '@/components/ModalDialog.vue';
import * as bootstrap from 'bootstrap';
import { BIconInfoCircle, BIconDatabaseAdd, BIconTrash, BIconPencil } from 'bootstrap-icons-vue';
import type { PowerLimiterConfig, PowerLimiterInverterConfig, PowerLimiterMetaData } from '@/types/PowerLimiterConfig';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        InputElement,
        BIconInfoCircle,
        BIconDatabaseAdd,
        BIconTrash,
        BIconPencil,
        ModalDialog,
    },
    data() {
        return {
            dataLoading: true,
            powerLimiterConfigList: {} as PowerLimiterConfig,
            powerLimiterMetaData: {} as PowerLimiterMetaData,
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            configAlert: false,
            additionalInverterSerial: '',
            modalEdit: {} as bootstrap.Modal,
            modalDelete: {} as bootstrap.Modal,
            editInverter: {} as PowerLimiterInverterConfig,
        };
    },
    mounted() {
        this.modalEdit = new bootstrap.Modal('#inverterEdit');
        this.modalDelete = new bootstrap.Modal('#inverterDelete');
    },
    created() {
        this.getAllData();
    },
    watch: {
        unmanagedInverters(newInverters) {
            if (newInverters.includes(this.additionalInverterSerial)) {
                return;
            }
            this.additionalInverterSerial = newInverters.length > 0 ? newInverters[0] : '';
        },
        'powerLimiterConfigList.inverter_serial_for_dc_voltage'(newValue) {
            if (newValue === '') {
                return; // do no inspect placeholder value
            }

            const managedInverters = this.powerLimiterConfigList.inverters;
            if (!managedInverters) {
                return [];
            }

            const managedSerials = managedInverters.map((inverter) => inverter.serial);
            if (!managedSerials.includes(newValue)) {
                // marks serial as invalid, selects placeholder option
                this.powerLimiterConfigList.inverter_serial_for_dc_voltage = '';
            }
        },
    },
    computed: {
        unmanagedInverters() {
            const managedInverters = this.powerLimiterConfigList.inverters;
            if (!managedInverters) {
                return [];
            }

            const managedSerials = managedInverters.map((inverter) => inverter.serial);
            const res = Object.keys(this.powerLimiterMetaData.inverters).filter(
                (serial) => !managedSerials.includes(serial)
            );
            return res;
        },
        batteryPoweredInverters() {
            return this.powerLimiterConfigList.inverters.filter((inverter) => !inverter.is_solar_powered);
        },
    },
    methods: {
        getConfigHints() {
            const meta = this.powerLimiterMetaData;
            const hints = [];

            if (meta.power_meter_enabled !== true) {
                hints.push({ severity: 'optional', subject: 'PowerMeterDisabled' });
            }

            if (typeof meta.inverters === 'undefined' || Object.keys(meta.inverters).length == 0) {
                hints.push({ severity: 'requirement', subject: 'NoInverter' });
                this.configAlert = true;
            } else {
                const managedInverters = this.powerLimiterConfigList.inverters;
                for (const managedInv of Object.values(managedInverters)) {
                    const metaInverters = this.powerLimiterMetaData.inverters;
                    const inv = metaInverters[managedInv.serial];
                    if (!inv) {
                        continue;
                    }
                    if (
                        inv !== undefined &&
                        !(inv.poll_enable && inv.command_enable && inv.poll_enable_night && inv.command_enable_night)
                    ) {
                        hints.push({ severity: 'requirement', subject: 'InverterCommunication' });
                        break;
                    }
                }
            }

            if (this.batteryPoweredInverterConfigured()) {
                if (!meta.charge_controller_enabled) {
                    hints.push({ severity: 'optional', subject: 'NoChargeController' });
                }

                if (!meta.battery_enabled) {
                    hints.push({ severity: 'optional', subject: 'NoBatteryInterface' });
                }
            }

            return hints;
        },
        isEnabled() {
            return this.powerLimiterConfigList.enabled;
        },
        hasPowerMeter() {
            return this.powerLimiterMetaData.power_meter_enabled;
        },
        canUseSolarPassthrough() {
            const meta = this.powerLimiterMetaData;
            return this.isEnabled() && meta.charge_controller_enabled && this.batteryPoweredInverterConfigured();
        },
        canUseSoCThresholds() {
            const meta = this.powerLimiterMetaData;
            return this.isEnabled() && meta.battery_enabled && this.batteryPoweredInverterConfigured();
        },
        canUseVoltageThresholds() {
            return this.isEnabled() && this.batteryPoweredInverterConfigured();
        },
        isSolarPassthroughEnabled() {
            return this.powerLimiterConfigList.solar_passthrough_enabled;
        },
        range(end: number) {
            return Array.from(Array(end).keys());
        },
        inverterLabel(serial: string) {
            if (serial === undefined) {
                return 'undefined';
            }
            const meta = this.powerLimiterMetaData;
            if (meta === undefined) {
                return 'metadata pending';
            }
            const inv = meta.inverters[serial];
            if (inv === undefined) {
                return 'not found';
            }
            return inv.name + ' (' + inv.type + ')';
        },
        batteryPoweredInverterConfigured() {
            return this.batteryPoweredInverters.length > 0;
        },
        needsChannelSelection() {
            const cfg = this.powerLimiterConfigList;
            const meta = this.powerLimiterMetaData;

            const reset = function () {
                cfg.inverter_channel_id_for_dc_voltage = 0;
                return false;
            };

            if (!this.batteryPoweredInverterConfigured()) {
                return reset();
            }

            if (cfg.inverter_serial_for_dc_voltage === '') {
                return reset();
            }

            const inverter = meta.inverters[cfg.inverter_serial_for_dc_voltage];
            if (inverter === undefined) {
                return reset();
            }

            if (cfg.inverter_channel_id_for_dc_voltage >= inverter.channels) {
                cfg.inverter_channel_id_for_dc_voltage = 0;
            }

            return inverter.channels > 1;
        },
        addInverter() {
            const newInverter = {} as PowerLimiterInverterConfig;
            newInverter.is_behind_power_meter = true;
            newInverter.serial = this.additionalInverterSerial;
            this.editStart(newInverter);
        },
        editStart(inverter: PowerLimiterInverterConfig) {
            this.editInverter = {} as PowerLimiterInverterConfig;
            Object.assign(this.editInverter, inverter);
            this.modalEdit.show();
        },
        editSubmit() {
            this.modalEdit.hide();

            const cfg = this.powerLimiterConfigList;

            let assigned = false;
            for (const inverter of cfg.inverters) {
                if (inverter.serial == this.editInverter.serial) {
                    Object.assign(inverter, this.editInverter);
                    assigned = true;
                    break;
                }
            }

            if (!assigned) {
                cfg.inverters.push(JSON.parse(JSON.stringify(this.editInverter)));
            }
        },
        deleteStart(inverter: PowerLimiterInverterConfig) {
            Object.assign(this.editInverter, inverter);
            this.modalDelete.show();
        },
        deleteSubmit() {
            this.modalDelete.hide();
            const cfg = this.powerLimiterConfigList;
            for (let i = 0; i < cfg.inverters.length; i++) {
                if (cfg.inverters[i].serial === this.editInverter.serial) {
                    cfg.inverters.splice(i, 1);
                    break;
                }
            }

            if (cfg.inverter_serial_for_dc_voltage === this.editInverter.serial) {
                // previously selected inverter was deleted. marks serial as
                // invalid, selects placeholder option.
                cfg.inverter_serial_for_dc_voltage = '';
            }
        },
        getAllData() {
            this.dataLoading = true;
            fetch('/api/powerlimiter/metadata', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.powerLimiterMetaData = data;
                    fetch('/api/powerlimiter/config', { headers: authHeader() })
                        .then((response) => handleResponse(response, this.$emitter, this.$router))
                        .then((data) => {
                            this.powerLimiterConfigList = data;
                            this.dataLoading = false;
                        });
                });
        },
        savePowerLimiterConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.powerLimiterConfigList));

            fetch('/api/powerlimiter/config', {
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
