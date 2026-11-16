<script setup lang="tsx">
import type { FormInst } from 'naive-ui'
import { NButton } from 'naive-ui'
import { useLoading } from '@/hooks'

interface Props {
  visible: boolean
  id: string | undefined
}
interface Emits {
  (e: 'update:visible', visible: boolean): void
  (e: 'refresh'): void
}

const props = withDefaults(defineProps<Props>(), {
  visible: false,
})
const emit = defineEmits<Emits>()

const { loading, startLoading, endLoading } = useLoading(false)
const formModel = ref({} as any)
const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

const rules = {
  newPassword: [{ required: true, message: '请输入新密码' }],
}
const formRef = ref<FormInst | null>()

function closeModal(visible = false) {
  formModel.value = {}
  emit('update:visible', visible)
}

function onSave(e: MouseEvent) {
  e.preventDefault()
  formRef.value?.validate(async (errors) => {
    if (errors)
      return

    startLoading()
    api.user.postUserResetPassword({
      userId: props.id,
      newPassword: formModel.value.newPassword,
    } as any).then(() => {
      closeModal()
      emit('refresh')
    }).finally(() => {
      endLoading()
    })
  })
}
</script>

<template>
  <n-modal
    v-model:show="modalVisible"
    :show-icon="false"
    :mask-closable="false"
    preset="dialog"
    title="重置密码"
    :segmented="{
      content: true,
      action: true,
    }"
  >
    <n-form
      ref="formRef"
      :rules="rules"
      label-placement="top"
      :model="formModel"
      label-align="left"
      :label-width="80"
      class="mt-20px"
    >
      <n-grid :cols="24" :x-gap="0">
        <n-form-item-grid-item :span="24" label="新密码" path="newPassword">
          <n-input
            v-model:value="formModel.newPassword"
            show-password-on="click"
            type="password"
            placeholder="请输入新密码"
          />
        </n-form-item-grid-item>
      </n-grid>
    </n-form>
    <template #action>
      <n-space>
        <NButton @click="closeModal()">
          取消
        </NButton>
        <NButton type="primary" :loading="loading" @click="onSave">
          提交
        </NButton>
      </n-space>
    </template>
  </n-modal>
</template>

<style scoped></style>
