<script setup lang="tsx">
import type { DataTableColumns, TreeOption } from 'naive-ui'
import { NButton } from 'naive-ui'
import { useLoading } from '@/hooks'
import { removeIds } from '@/utils'

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
const formModel = ref({ orgIds: [] as string[] })
const state = reactive({
  selectDataList: [] as API.ModifyOrganizationRequest[],
  actualCheckedOrgIds: [] as string[],
})
const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

const columns: DataTableColumns< API.ModifyOrganizationRequest> = [
  {
    type: 'selection',
    width: 30,
  },
  {
    title: '组织',
    key: 'name',
    width: 200,
  },
]

function closeModal(visible = false) {
  formModel.value = { orgIds: [] }
  emit('update:visible', visible)
}

function getSelectDataList() {
  if (!props.modalData?.id)
    return
  api.organization.getOrganizationOrgTreeByUserIdUserId({ userId: props.modalData.id }).then((r) => {
    state.selectDataList = r.data.orgs
    formModel.value.orgIds = removeIds(r.data.orgs, JSON.parse(JSON.stringify(r.data.checkedOrgIds)))
    state.actualCheckedOrgIds = JSON.parse(JSON.stringify(r.data.checkedOrgIds))
  })
}

function onUpdate(keys: string[], option: Array<TreeOption | null>) {
  let pids = option?.map((i: any) => i.parentId).filter(Boolean) || []
  pids = [...new Set(pids)]
  state.actualCheckedOrgIds = [...keys, ...pids]
  formModel.value.orgIds = keys
  console.log(state.actualCheckedOrgIds)
}

function onSave(e: MouseEvent) {
  e.preventDefault()

  startLoading()
  api.user.postUserAddUserOrg({
    userId: props.modalData?.id as string,
    orgIds: state.actualCheckedOrgIds,
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
    title="组织授权"
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
    <div class="mb-20px mt-[-10px] border-dialog-b" />
    <n-data-table
      v-model:checked-row-keys="formModel.orgIds"
      :on-update:checked-row-keys="onUpdate"
      class="h-300px"
      flex-height
      :row-key="(row: any) => row.id"
      children-key="childrens"
      check-strategy="child"
      :columns="columns"
      :data="state.selectDataList"
      :loading="loading"
      size="small"
    />

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
