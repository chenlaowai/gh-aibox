<script setup lang="tsx">
import type { DataTableColumns } from 'naive-ui'
import { useLoading } from '@/hooks'

interface Props {
  visible: boolean
  modalData: API.ModifyUserRequest & { orgName: string } | null
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
const formModel = ref({ roleIds: [] })
const state = reactive({
  selectDataList: [] as API.ModifyRoleRequest[],
})
const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

const columns: DataTableColumns< API.ModifyRoleRequest> = [
  {
    type: 'selection',
    width: 30,
  },
  {
    title: '角色',
    key: 'name',
    width: 200,
  },
]

function closeModal(visible = false) {
  formModel.value = { roleIds: [] }
  emit('update:visible', visible)
}

function getSelectDataList() {
  if (!props.modalData?.id)
    return
  api.role.getRoleRoleByUserIdUserId({ userId: props.modalData.id! }).then((r) => {
    state.selectDataList = r.data.roles
    formModel.value.roleIds = r.data.checkedRoleIds
  })
}

function onSave(e: MouseEvent) {
  e.preventDefault()

  startLoading()
  api.role.postRoleAddUserRole({
    userId: props.modalData?.id as string,
    roleIds: formModel.value.roleIds,
  } as any).then(() => {
    closeModal()
    emit('refresh')
  }).finally(() => {
    endLoading()
  })
}

watch(() => props.visible, (val) => {
  if (val) {
    getSelectDataList()
  }
})
</script>

<template>
  <n-modal
    v-model:show="modalVisible"
    :show-icon="false"
    :mask-closable="false"
    preset="dialog"
    title="角色授权"
    :segmented="{
      content: true,
      action: true,
    }"
  >
    <n-form
      label-placement="top"
      label-align="left"
      :label-width="90"
      class="mt-20px"
    >
      <n-grid :cols="2" :x-gap="0">
        <n-form-item-grid-item label="用户姓名：">
          <div>{{ modalData?.name }}</div>
        </n-form-item-grid-item>
        <n-form-item-grid-item label="所属组织：">
          <div>{{ modalData?.orgName }}</div>
        </n-form-item-grid-item>
      </n-grid>
    </n-form>
    <div class="border-dialog-b mb-20px mt-[-10px]" />
    <n-data-table
      v-model:checked-row-keys="formModel.roleIds"
      :row-key="(row: API.ModifyRoleRequest) => row.id"
      :columns="columns"
      :data="state.selectDataList"
      :loading="loading"
      size="small"
    />

    <template #action>
      <n-space>
        <n-button @click="closeModal()">
          取消
        </n-button>
        <n-button type="primary" :loading="loading" @click="onSave">
          提交
        </n-button>
      </n-space>
    </template>
  </n-modal>
</template>

<style scoped></style>
