<script setup lang="tsx">
import type { DataTableColumns } from 'naive-ui'
import { NButton, NTag } from 'naive-ui'
import TableModal from './components/TableModal.vue'
import { useLoading } from '@/hooks'
import { renderIcon } from '@/utils'
import CopyText from '@/components/custom/CopyText.vue'

const { loading, startLoading, endLoading } = useLoading(false)

const tableModalRef = ref()
const tableData = ref<API.MenuTree[]>([])
const columns: DataTableColumns<API.MenuTree> = [
  { title: '名称', key: 'name', minWidth: 120 },
  {
    title: '图标',
    align: 'center',
    key: 'icon',
    minWidth: '6em',
    render: (row) => {
      return row.icon && renderIcon('row.icon', { size: 20 })?.()
    },
  },
  {
    title: '路由路径',
    key: 'url',
    minWidth: 200,
    render: (row) => {
      return (
        <CopyText value={row.url} />
      )
    },
  },
  { title: '排序', minWidth: '6em', key: 'sort', align: 'center' },
  {
    title: '菜单类型',
    align: 'center',
    key: 'isCatalog',
    minWidth: '6em',
    render: (row) => {
      const menuType = row.isCatalog ? 'dir' : 'page'
      const menuTagType: Record<AppRoute.MenuType, NaiveUI.ThemeColor> = {
        dir: 'warning',
        page: 'primary',
      }
      return <NTag size="small" type={menuTagType[menuType]}>{menuType === 'dir' ? '文件夹' : '文件'}</NTag>
    },
  },
  { title: '备注', key: 'remark', align: 'center', minWidth: 200, ellipsis: { tooltip: true } },
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
            onClick={() => tableModalRef.value.openModal('edit', row)}
          >
            编辑
          </n-button>
          <n-popconfirm
            onPositiveClick={() => deleteData(row.id!)}
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

function deleteData(id: string) {
  api.menu.deleteMenuDeleteId({ id }).then(() => {
    getAllRoutes()
  })
}

onMounted(() => {
  getAllRoutes()
})
async function getAllRoutes() {
  startLoading()
  const { data } = await api.menu.getMenuMenuTree()
  tableData.value = data
  endLoading()
}
</script>

<template>
  <n-card>
    <n-flex vertical>
      <div class="flex gap-4">
        <NButton type="primary" @click="tableModalRef.openModal('add')">
          <template #icon>
            <icon-park-outline-add-one />
          </template>
          新建
        </NButton>
      </div>
      <n-data-table
        :row-key="(row:AppRoute.RowRoute) => row.id" :columns="columns" :data="tableData"
        children-key="childrens"
        :loading="loading"
        size="small"
        remote
        :striped="false"
        :bordered="false"
        :scroll-x="1200"
      />
      <TableModal ref="tableModalRef" :all-routes="tableData" modal-name="菜单" @close="getAllRoutes" />
    </n-flex>
  </n-card>
</template>
