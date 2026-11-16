<script setup lang="tsx">
import type { DataTableColumns, FormInst } from 'naive-ui'
import TableModal from './components/TableModal.vue'
import MenuAuthModal from './components/MenuAuthModal.vue'
import { useBoolean, useLoading, usePagination } from '@/hooks'

const { loading, startLoading, endLoading } = useLoading(false)
const { bool: visible, setBool: setVisible } = useBoolean(false)
const { bool: menuVisible, setBool: setMenuVisible } = useBoolean(false)

const initialModel = {
  Keywords: '',
}
const searchForm = ref({ ...initialModel })

const formRef = ref<FormInst | null>()
const columns: DataTableColumns<API.ModifyRoleRequest> = [
  { title: '名称', key: 'name' },
  { title: '编码', key: 'code' },
  { title: '备注', key: 'remark', ellipsis: { tooltip: true } },
  {
    title: '操作',
    key: 'actions',
    width: 240,
    render(row) {
      return (
        <>
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => {
              onAddOrEdit(row)
            }}
          >
            编辑
          </n-button>
          <n-popconfirm
            onPositiveClick={() => {
              onDelete(row.id as string)
            }}
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
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => {
              setEditData(JSON.parse(JSON.stringify(row || {})))
              setMenuVisible(true)
            }}
          >
            菜单按钮授权
          </n-button>
        </>
      )
    },
  },
]

const listData = ref<any[]>([])
const pagination = usePagination({ fn: getList })
function getList() {
  startLoading()
  api.role
    .getRoleRolePage({
      PageIndex: pagination.page,
      PageSize: pagination.pageSize,
      ...searchForm.value,
    })
    .then((r) => {
      listData.value = r.data.list || []
      pagination.itemCount = r.data?.totalCount
    })
    .finally(() => {
      endLoading()
    })
}

function onDelete(id: string) {
  api.role.postRoleDeleteId({ id }).then(() => {
    init()
  })
}

function init() {
  pagination.page = 1
  getList()
}

onMounted(() => {
  init()
})

function handleResetSearch() {
  searchForm.value = { ...initialModel }
}

const detail = ref<any | null>({})
function setEditData(data: any | null) {
  detail.value = data
}

function onAddOrEdit(row: any | null) {
  setEditData(JSON.parse(JSON.stringify(row || {})))
  setVisible(true)
}
</script>

<template>
  <n-space vertical size="large" :wrap-item="false">
    <n-card>
      <div class="flex">
        <n-form
          ref="formRef"
          :model="searchForm"
          label-placement="left"
          inline
          :show-feedback="false"
        >
          <n-flex>
            <n-form-item label="名称" path="Keywords">
              <n-input
                v-model:value="searchForm.Keywords"
                clearable
                placeholder="请输入名称"
              />
            </n-form-item>
            <n-flex class="ml-auto">
              <n-button type="primary" @click="init()">
                <template #icon>
                  <icon-park-outline-search />
                </template>
                搜索
              </n-button>
              <n-button strong secondary @click="handleResetSearch">
                <template #icon>
                  <icon-park-outline-redo />
                </template>
                重置
              </n-button>
            </n-flex>
          </n-flex>
        </n-form>
      </div>
    </n-card>
    <n-card class="flex flex-1" content-class="flex flex-col">
      <div class="mb-20px flex gap-4">
        <n-button type="primary" @click="onAddOrEdit({})">
          <template #icon>
            <icon-park-outline-add />
          </template>
          新增
        </n-button>
      </div>
      <n-data-table
        flex-height
        class="flex-1-hidden"
        remote
        :striped="false"
        :bordered="false"
        :columns="columns"
        :data="listData"
        :loading="loading"
        :pagination="pagination"
      />
      <TableModal
        v-model:visible="visible"
        :modal-data="detail"
        @refresh="init"
      />
      <MenuAuthModal
        v-model:visible="menuVisible"
        :modal-data="detail"
      />
    </n-card>
  </n-space>
</template>
