<template>
    <CardElement :text="$t('uartallocations.Allocations')" textVariant="text-bg-primary" table>
        <div class="table-responsive">
            <table class="table table-hover table-condensed">
                <thead>
                    <tr>
                        <th>{{ $t('uartallocations.Owner') }}</th>
                        <th>{{ $t('uartallocations.Port') }}</th>
                    </tr>
                </thead>
                <tbody>
                    <tr v-for="(allocation, idx) in allocations" :key="idx">
                        <td>
                            <template v-if="allocation.owner.length === 0">{{ $t('uartallocations.Free') }}</template>
                            <template v-else>{{ allocation.owner }}</template>
                        </td>
                        <td>
                            <span v-if="allocation.port >= 0" class="badge text-bg-success">
                                UART {{ allocation.port }}
                            </span>
                            <span v-else class="badge text-bg-danger">
                                {{ $t('uartallocations.Rejected') }}
                            </span>
                        </td>
                    </tr>
                </tbody>
            </table>
        </div>
    </CardElement>
</template>

<script lang="ts">
import CardElement from '@/components/CardElement.vue';
import type { UartAllocation } from '@/types/SystemStatus';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    components: {
        CardElement,
    },
    props: {
        allocations: { type: Object as PropType<UartAllocation[]>, required: true },
    },
});
</script>
