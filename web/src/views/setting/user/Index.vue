<script setup lang="tsx">
import type { DataTableColumns, FormInst } from 'naive-ui'
import TableModal from './components/TableModal.vue'
import ResetModal from './components/ResetModal.vue'
import RoleAuthModal from './components/RoleAuthModal.vue'
// import OrgAuthModal from './components/OrgAuthModal.vue'
import { useBoolean, useLoading, usePagination } from '@/hooks'

const { loading, startLoading, endLoading } = useLoading(false)
const { bool: visible, setBool: setVisible } = useBoolean(false)
const { bool: resetVisible, setBool: setResetVisible } = useBoolean(false)
const { bool: roleAuthVisible, setBool: setRoleAuthVisible } = useBoolean(false)
// const { bool: orgAuthVisible, setBool: setOrgAuthVisible } = useBoolean(false)
const initialModel = {
  Keywords: '',
}
const searchForm = ref({ ...initialModel })
const state = reactive({
  id: undefined as undefined | string,
})
const formRef = ref<FormInst | null>()
const columns: DataTableColumns<API.ModifyUserRequest> = [
  { title: '用户名称', key: 'name' },
  { title: '账号', key: 'account' },
  { title: '手机号', key: 'mobile' },
  { title: '所属组织', key: 'orgName' },
  { title: '备注', key: 'remark', ellipsis: { tooltip: true } },
  {
    title: '操作',
    key: 'actions',
    width: 360,
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
            type="warning"
            onClick={() => {
              state.id = row.id
              setResetVisible(true)
            }}
          >
            重置密码
          </n-button>
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => {
              setEditData(JSON.parse(JSON.stringify(row || {})))
              setRoleAuthVisible(true)
            }}
          >
            角色授权
          </n-button>
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => {
              setEditData(JSON.parse(JSON.stringify(row || {})))
              // eslint-disable-next-line no-lone-blocks
              { /* setOrgAuthVisible(true) */ }
            }}
          >
            组织授权
          </n-button>
        </>
      )
    },
  },
]

const listData = ref<API.UserBaseInfo[]>([])
const pagination = usePagination({ fn: getList })
function getList() {
  startLoading()
  api.user
    .getUserUserPage({
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
  api.user.deleteUserDeleteId({ id }).then(() => {
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
            <n-form-item label="用户名称" path="Keywords">
              <n-input
                v-model:value="searchForm.Keywords"
                clearable
                placeholder="请输入用户名称"
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
      <ResetModal
        :id="state.id"
        v-model:visible="resetVisible"
        @refresh="init"
      />
      <RoleAuthModal
        v-model:visible="roleAuthVisible"
        :modal-data="detail"
        @refresh="init"
      />
      <!-- <OrgAuthModal
        v-model:visible="orgAuthVisible"
        :modal-data="detail"
        @refresh="init"
      /> -->
    </n-card>
  </n-space>
</template>
