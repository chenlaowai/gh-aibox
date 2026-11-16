<script setup lang="tsx">
import type { FormInst } from 'naive-ui'

interface Props {
  visible: boolean
  modalData: API.ModifyButtonRequest | null
  menu: API.MenuTree
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
  name: [{ required: true, message: '请输入按钮名称' }],
  code: [{ required: true, message: '请输入按钮编码' }],
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

    const fn = formModel.value.id
      ? api.button.postButtonModify
      : api.button.postButtonAdd
    fn({ ...formModel.value, menuId: props.menu.id } as any).then(() => {
      closeModal()
      emit('refresh')
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
    :title="`【${menu.name}】${formModel.id ? '编辑按钮' : '新增按钮'}`"
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
        <n-form-item-grid-item :span="24" label="按钮名称" path="name">
          <n-input
            v-model:value="formModel.name"
            placeholder="请输入按钮名称"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="24" label="按钮编码" path="code">
          <n-input
            v-model:value="formModel.code"
            placeholder="请输入按钮编码"
          />
        </n-form-item-grid-item>
      </n-grid>
    </n-form>
    <template #action>
      <n-space>
        <n-button @click="closeModal()">
          取消
        </n-button>
        <n-button type="primary" @click="onSave">
          提交
        </n-button>
      </n-space>
    </template>
  </n-modal>
</template>

<style scoped></style>
