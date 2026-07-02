<template>
    <BasePage :title="$t('modbusserveradmin.ModbusServerSettings')" :isLoading="dataLoading">
        <BootstrapAlert
            v-model="showAlert"
            :show="duplicateUnitIds.size > 0"
            :dismissible="duplicateUnitIds.size === 0"
            :variant="duplicateUnitIds.size > 0 ? 'danger' : alertType"
            :auto-dismiss="duplicateUnitIds.size > 0 ? 0 : (alertType != 'success' ? 0 : 5000)"
        >
            {{ duplicateUnitIds.size > 0 ? $t('modbusserveradmin.DuplicateUnitId') : alertMessage }}
        </BootstrapAlert>

        <form @submit="saveModbusServerConfig">
            <CardElement :text="$t('modbusserveradmin.ModbusServerConfiguration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('modbusserveradmin.ModbusServerEnable')"
                    v-model="modbusServerConfigList.enabled"
                    type="checkbox"
                    wide
                />

                <template v-if="modbusServerConfigList.enabled">
                    <InputElement
                        :label="$t('modbusserveradmin.Port')"
                        v-model="modbusServerConfigList.port"
                        type="number"
                        min="1"
                        max="65535"
                        wide
                    />
                </template>
            </CardElement>

            <CardElement
                v-if="modbusServerConfigList.enabled"
                :text="$t('modbusserveradmin.InverterMapping')"
                textVariant="text-bg-primary"
                add-space
            >
                <p>{{ $t('modbusserveradmin.InverterMappingHint') }}</p>

                <table class="table table-borderless">
                    <thead>
                        <tr>
                            <th scope="col" style="width: 1%; white-space: nowrap">{{ $t('modbusserveradmin.Enable') }}</th>
                            <th scope="col">{{ $t('modbusserveradmin.Inverter') }}</th>
                            <th scope="col">{{ $t('modbusserveradmin.UnitId') }}</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr v-for="i in inverterList" :key="i.serial">
                            <td style="width: 1%; white-space: nowrap">
                                <div class="form-check form-switch mb-0">
                                    <input
                                        class="form-check-input"
                                        type="checkbox"
                                        :checked="isEnabled(i.serial)"
                                        @change="toggleInverter(i.serial, ($event.target as HTMLInputElement).checked)"
                                    />
                                </div>
                            </td>
                            <td>{{ i.name }} ({{ i.type }})</td>
                            <td>
                                <input
                                    class="form-control"
                                    :class="{ 'is-invalid': isEnabled(i.serial) && duplicateUnitIds.has(getEntry(i.serial)!.unit_id) }"
                                    type="number"
                                    min="1"
                                    max="247"
                                    :disabled="!isEnabled(i.serial)"
                                    :value="getEntry(i.serial)?.unit_id ?? ''"
                                    @input="onUnitIdInput(i.serial, ($event.target as HTMLInputElement).value)"
                                />
                            </td>
                        </tr>
                    </tbody>
                </table>
            </CardElement>

            <div v-if="modbusServerConfigList.enabled" class="alert alert-secondary mt-5" role="alert">
                <h2>{{ $t('modbusserveradmin.ExposedModelsHeading') }}</h2>
                <p>{{ $t('modbusserveradmin.ExposedModelsIntro') }}</p>
                <ul>
                    <li>{{ $t('modbusserveradmin.ExposedModel1') }}</li>
                    <li>{{ $t('modbusserveradmin.ExposedModel101103') }}</li>
                    <li>{{ $t('modbusserveradmin.ExposedModel120') }}</li>
                </ul>
            </div>

            <FormFooter :disabled="duplicateUnitIds.size > 0" @reload="getModbusServerConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import { defineComponent } from 'vue';
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import { handleResponse, authHeader } from '@/utils/authentication';
import type { ModbusServerConfig, ModbusServerInverterConfig } from '@/types/ModbusServerConfig';
import type { Inverter } from '@/types/InverterConfig';

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
            modbusServerConfigList: {} as ModbusServerConfig,
            inverterList: [] as Array<Inverter>,
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            // Remembers each inverter's last-used unit ID across disable/re-enable
            // toggles in this editing session, so re-enabling in a different order
            // doesn't reshuffle IDs that are already in use elsewhere.
            rememberedUnitIds: {} as Record<string, number>,
        };
    },
    created() {
        this.getModbusServerConfig();
        this.getInverterList();
    },
    computed: {
        duplicateUnitIds(): Set<number> {
            const seen = new Set<number>();
            const dupes = new Set<number>();
            for (const inv of this.modbusServerConfigList.inverter) {
                if (seen.has(inv.unit_id)) {
                    dupes.add(inv.unit_id);
                }
                seen.add(inv.unit_id);
            }
            return dupes;
        },
    },
    methods: {
        getModbusServerConfig() {
            this.dataLoading = true;
            fetch('/api/modbusserver/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.modbusServerConfigList = data;
                    for (const inv of this.modbusServerConfigList.inverter) {
                        this.rememberedUnitIds[inv.serial] = inv.unit_id;
                    }
                    this.dataLoading = false;
                });
        },
        getInverterList() {
            fetch('/api/inverter/list', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.inverterList = data.inverter;
                });
        },
        getEntry(serial: string): ModbusServerInverterConfig | undefined {
            return this.modbusServerConfigList.inverter.find((i) => i.serial === serial);
        },
        isEnabled(serial: string): boolean {
            return this.getEntry(serial) !== undefined;
        },
        nextUnitId(): number {
            const used = new Set(this.modbusServerConfigList.inverter.map((i) => i.unit_id));
            let id = 126;
            while (used.has(id) && id < 247) {
                id++;
            }
            return id;
        },
        toggleInverter(serial: string, checked: boolean) {
            if (checked) {
                const remembered = this.rememberedUnitIds[serial];
                const unitId = remembered !== undefined ? remembered : this.nextUnitId();
                this.modbusServerConfigList.inverter.push({ serial, unit_id: unitId });
                this.rememberedUnitIds[serial] = unitId;
                return;
            }
            const idx = this.modbusServerConfigList.inverter.findIndex((i) => i.serial === serial);
            if (idx >= 0) {
                this.modbusServerConfigList.inverter.splice(idx, 1);
            }
        },
        onUnitIdInput(serial: string, value: string) {
            const entry = this.getEntry(serial);
            if (entry) {
                entry.unit_id = Number(value);
                this.rememberedUnitIds[serial] = entry.unit_id;
            }
        },
        saveModbusServerConfig(e: Event) {
            e.preventDefault();

            if (this.duplicateUnitIds.size > 0) {
                return;
            }

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.modbusServerConfigList));

            fetch('/api/modbusserver/config', {
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
