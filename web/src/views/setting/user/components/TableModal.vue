<script setup lang="tsx">
import type { FormInst, FormItemRule } from 'naive-ui'
import { NButton } from 'naive-ui'
import { useLoading } from '@/hooks'

interface Props {
  visible: boolean
  modalData?: API.AddUserRequest & API.ModifyUserRequest | null
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

const state = reactive({
  selectDataList: [] as API.ModifyOrganizationRequest[],
})
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
  name: [{ required: true, message: '请输入姓名' }],
  account: [{ required: true, message: '请输入账号' }],
  mobile: [{ message: '请输入正确的手机号', validator: (rule: FormItemRule, value: any) => /^(?:(?:\+|00)86)?1[3-9]\d{9}$/.test(value) }],
  password: [{ required: true, message: '请输入密码' }],
  orgId: [{ required: true, message: '请选择所属组织' }],
}
const formRef = ref<FormInst | null>()

function closeModal(visible = false) {
  emit('update:visible', visible)
}

// function getSelectDataList() {
//   api.organization.getOrganizationOrganizationsTree({}).then((r) => {
//     state.selectDataList = r.data
//   })
// }

function onSave(e: MouseEvent) {
  e.preventDefault()
  formRef.value?.validate(async (errors) => {
    if (errors)
      return

    startLoading()
    const fn = formModel.value.id
      ? api.user.postUserModify
      : api.user.postUserAdd
    fn({ ...formModel.value } as any).then(() => {
      closeModal()
      emit('refresh')
    }).finally(() => {
      endLoading()
    })
  })
}

// watch(
//   () => props.visible,
//   (newValue) => {
//     if (newValue) {
//       formModel.value = { ...props.modalData }
//       getSelectDataList()
//     }
//   },
// )
</script>

<template>
  <n-modal
    v-model:show="modalVisible"
    :show-icon="false"
    :mask-closable="false"
    preset="dialog"
    :title="formModel.id ? '编辑用户' : '新增用户'"
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
        <n-form-item-grid-item :span="24" label="姓名" path="name">
          <n-input
            v-model:value="formModel.name"
            placeholder="请输入姓名"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="24" label="账号" path="account">
          <n-input
            v-model:value="formModel.account"
            placeholder="请输入账号"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="24" label="手机号" path="mobile">
          <n-input
            v-model:value="formModel.mobile"
            placeholder="请输入手机号"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item v-if="!formModel.id" :span="24" label="密码" path="password">
          <n-input
            v-model:value="formModel.password"
            show-password-on="click"
            type="password"
            placeholder="请输入密码"
          />
        </n-form-item-grid-item>
        <n-form-item-grid-item :span="24" label="所属组织" path="orgId">
          <n-tree-select
            v-model:value="formModel.orgId"
            filterable
            clearable
            :options="state.selectDataList"
            key-field="id"
            label-field="name"
            children-field="childrens"
            placeholder="请选择所属组织"
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
