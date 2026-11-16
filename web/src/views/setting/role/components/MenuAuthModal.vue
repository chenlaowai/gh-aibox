<script setup lang="tsx">
import type { DataTableColumns, TreeOption, TreeOverrideNodeClickBehavior } from 'naive-ui'
import { NButton } from 'naive-ui'
import { useBoolean, useLoading } from '@/hooks'
import { removeIds } from '@/utils'

interface Props {
  visible: boolean
  modalData: API.ModifyRoleRequest | null
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
const { bool, setBool } = useBoolean(false)
const formModel = ref({ orgIds: [] })
const columns: DataTableColumns< API.ModifyButtonRequest> = [
  { type: 'selection', width: 30 },
  { title: '按钮名称', key: 'name' },
  { title: '按钮编号', key: 'code' },
]
const state = reactive({
  selectDataList: [] as API.ModifyMenuRequest[],
  checkedMenuIds: [] as string[],
  actualCheckedMenuIds: [] as string[],
  checkedButtonIds: [] as string[],
  btnList: [] as API.ModifyButtonRequest[],
})
const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

function closeModal(visible = false) {
  formModel.value = { orgIds: [] }
  emit('update:visible', visible)
}

function getSelectDataList() {
  if (!props.modalData?.id)
    return
  api.menu.getMenuMenuButtonTreeByRoleIdRoleId({ roleId: props.modalData.id }).then((r) => {
    state.selectDataList = r.data.menus
    state.checkedMenuIds = removeIds(r.data.menus, JSON.parse(JSON.stringify(r.data.checkedMenuIds)))
    state.actualCheckedMenuIds = JSON.parse(JSON.stringify(r.data.checkedMenuIds))
    state.checkedButtonIds = r.data.checkedButtonIds
  })
}

function onUpdate(keys: string[], option: Array<TreeOption | null>) {
  let pids = option?.map((i: any) => i.parentId).filter(Boolean) || []
  pids = [...new Set(pids)]
  state.actualCheckedMenuIds = [...keys, ...pids]
  state.checkedMenuIds = keys
}
const override: TreeOverrideNodeClickBehavior = ({ option }) => {
  if (option.children) {
    return 'toggleExpand'
  }
  setBool(true)
  api.button.getButtonButtonsByMenuIdMenuId({ menuId: option.id as string }).then((r) => {
    state.btnList = r.data as any
  }).finally(() => {
    setBool(false)
  })
  return 'default'
}

function onSave(e: MouseEvent) {
  e.preventDefault()

  startLoading()
  api.role.postRoleAddRoleMenuButton({
    roleId: props.modalData?.id as string,
    menuIds: state.actualCheckedMenuIds,
    buttonIds: state.checkedButtonIds,
    // MenuFieldIds: [],
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
  else {
    state.btnList = []
  }
})
</script>

<template>
  <n-modal
    v-model:show="modalVisible"
    :show-icon="false"
    :mask-closable="false"
    preset="dialog"
    title="菜单按钮授权"
    style="width: 800px;"
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
        <n-form-item-grid-item label="角色名称：">
          <div>{{ modalData?.name }}</div>
        </n-form-item-grid-item>
        <n-form-item-grid-item label="角色编码：">
          <div>{{ modalData?.code }}</div>
        </n-form-item-grid-item>
      </n-grid>
    </n-form>
    <div class="border-dialog-b mb-20px mt-[-10px]" />
    <div class="h-400px flex gap-10px overflow-hidden">
      <n-card size="small" class="h-full w-250px overflow-y-auto" content-style="padding:0;height:100%">
        <n-tree
          v-model:checked-keys="state.checkedMenuIds"
          default-expand-all
          block-line
          :data="state.selectDataList"
          label-field="name"
          key-field="id"
          children-field="childrens"
          checkable
          cascade
          check-strategy="child"
          :on-update:checked-keys="onUpdate"
          :override-default-node-click-behavior="override"
        />
      </n-card>
      <n-data-table
        v-model:checked-row-keys="state.checkedButtonIds"
        :row-key="(row: any) => row.id"
        :columns="columns"
        :data="state.btnList"
        :loading="bool"
        size="small"
      />
    </div>
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
