<script setup lang="tsx">
import type { DataTableColumns, TreeOverrideNodeClickBehavior } from 'naive-ui'
import TableModal from './components/TableModal.vue'
import { useBoolean, useLoading } from '@/hooks'

const { loading, startLoading, endLoading } = useLoading(false)
const { bool: visible, setBool: setVisible } = useBoolean(false)
const detail = ref<any | null>({})
const state = reactive({
  menu: {} as API.MenuTree,
  menuList: [] as API.MenuTree[],
  btnList: [] as API.ButtonInfo[],
})

const columns: DataTableColumns<API.ButtonInfo> = [
  { title: '按钮名称', key: 'name' },
  { title: '按钮编号', key: 'code' },
  {
    title: '操作',
    align: 'center',
    key: 'actions',
    width: '15em',
    render: (row) => {
      return (
        <>
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => onAddOrEdit(row)}
          >
            编辑
          </n-button>
          <n-popconfirm
            onPositiveClick={() => onDelete(row.id as string)}
          >
            {{
              default: () => ['是否确认删除？'],
              trigger: () => (
                <n-button size="small" quaternary type="error">
                  删除
                </n-button>
              ),
            }}
          </n-popconfirm>
        </>
      )
    },
  },
]

const override: TreeOverrideNodeClickBehavior = ({ option }) => {
  if (option.children) {
    return 'toggleExpand'
  }
  state.menu = option as any
  getBtnList()
  return 'default'
}

function getBtnList() {
  startLoading()
  api.button.getButtonButtonsByMenuIdMenuId({ menuId: state.menu.id as string }).then((r: any) => {
    state.btnList = r.data
  }).finally(() => {
    endLoading()
  })
}

function onDelete(id: string) {
  api.button.deleteButtonDeleteId({ id }).then(() => {
    getBtnList()
  })
}

async function getMenuList() {
  startLoading()
  const { data } = await api.menu.getMenuMenuTree()
  state.menuList = data
  endLoading()
}

function onAddOrEdit(row: any | null) {
  if (!state.menu?.id) {
    return window.$message.warning('请先选择左侧菜单')
  }
  detail.value = JSON.parse(JSON.stringify(row || {}))
  setVisible(true)
}

onMounted(() => {
  getMenuList()
})
</script>

<template>
  <n-card content-class="flex">
    <div class="flex flex-1 gap-10px">
      <n-card size="small" class="h-full w-250px overflow-y-auto" content-style="padding:0;height:100%">
        <n-tree

          block-line default-expand-all
          :data="state.menuList"
          label-field="name"
          key-field="id"
          children-field="childrens"
          :override-default-node-click-behavior="override"
        />
      </n-card>
      <div class="flex-1">
        <div class="mb-10px flex gap-4">
          <n-button type="primary" @click="onAddOrEdit({})">
            <template #icon>
              <icon-park-outline-add-one />
            </template>
            新建
          </n-button>
        </div>
        <n-data-table
          :row-key="(row: any) => row.id"
          :columns="columns"
          :data="state.btnList"
          :loading="loading"
          size="small"
        />
      </div>
    </div>
    <TableModal v-model:visible="visible" :modal-data="detail" :menu="state.menu" @refresh="getBtnList" />
  </n-card>
</template>
