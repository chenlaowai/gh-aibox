<script setup lang='ts'>
const emit = defineEmits<{
  (e: 'update:isVerifying', value: boolean): void
}>()
const modalTitle = ref<string>()
const modalVisible = ref<boolean>(false)

function open(title: string) {
  modalVisible.value = true
  modalTitle.value = title
}
function close() {
  modalVisible.value = false
  emit('update:isVerifying', false)
}
defineExpose({
  open,
  close,
})
</script>

<template>
  <n-modal
    v-model:show="modalVisible"
    display-directive="if"
    :close-on-esc="false"
    :show-icon="false"
    :mask-closable="false"
    preset="dialog"
    :title="modalTitle"
    @close="close"
  >
    <slot />
  </n-modal>
</template>

<style>

</style>
