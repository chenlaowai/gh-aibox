<script setup lang="tsx">
import type { FormInst } from 'naive-ui'
import { NButton } from 'naive-ui'
import { useLoading } from '@/hooks'

interface Props {
  visible: boolean
  modalData?: API.ModifyRoleRequest | null
}
interface Emits {
  (e: 'update:visible', visible: boolean): void
  (e: 'refresh'): void
}

const props = withDefaults(defineProps<Props>(), {
  visible: false,
  modalData: null,
})
const emit = defineEmits<Emits>()

const { loading, startLoading, endLoading } = useLoading(false)
const formModel = ref({ ...props.modalData })
const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

const rules = {
  name: [{ required: true, message: '请输入名称' }],
  code: [{ required: true, message: '请输入编码' }],
}
const formRef = ref<FormInst | null>()

function closeModal(visible = false) {
  emit('update:visible', visible)
}

function onSave(e: MouseEvent) {
  e.preventDefault()
  formRef.value?.validate(async (errors) => {
    if (errors)
      return

    startLoading()
    const fn = formModel.value.id
      ? api.role.postRoleModify
      : api.role.postRoleAdd
    fn({ ...formModel.value } as any).then(() => {
      closeModal()
      emit('refresh')
    }).finally(() => {
      endLoading()
    })
  })
}

watch(
  () => props.visible,
  (newValue) => {
    if (newValue) {
      formModel.value = { ...props.modalData }
    }
  },
)
</script>

<template>
  <n-modal
    v-model:show="modalVisible"
    :show-icon="false"
    :mask-closable="false"
    preset="dialog"
    :title="formModel.id ? '编辑角色' : '新增角色'"
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
      :label-width="60"
      class="mt-20px"
    >
      <n-grid :cols="24" :x-gap="0">
        <n-form-item-grid-item :span="24" label="名称" path="name">
          <n-input
            v-model:value="formModel.name"
            placeholder="请输入名称"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="24" label="编码" path="code">
          <n-input
            v-model:value="formModel.code"
            placeholder="请输入编码"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="24" label="备注" path="remark">
          <n-input
            v-model:value="formModel.remark"
            placeholder="请输入备注"
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
