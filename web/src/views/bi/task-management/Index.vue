<script setup lang="tsx">
import { onMounted, onUnmounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import type { DataTableColumns } from 'naive-ui'
import { useBoolean, useLoading, usePagination, useResponsiveHeight } from '@/hooks'
import { useAppStore } from '@/store'
import { useAuthStore } from '@/store/auth'
import { getTaskStatusDisplay } from '@/utils/systemDict'
import CustomTag from '@/components/common/CustomTag.vue'
import { compactStreamError, fetchAiTaskHealth, type AiTaskHealth } from '@/services/aiTask'

const { height: containerHeight } = useResponsiveHeight(124)
const router = useRouter()
const { bool: deleteVisible, setBool: setDeleteVisible } = useBoolean(false)
const { loading, startLoading, endLoading } = useLoading(true)

const appStore = useAppStore()
const authStore = useAuthStore()
const taskData = ref<API.GetTaskPageResult[]>([])
const deleteTarget = ref<any | null>(null)
const aiHealth = ref<Record<string, AiTaskHealth>>({})
let healthTimer: ReturnType<typeof setInterval> | null = null

async function refreshAiHealth() {
  const list = await fetchAiTaskHealth(authStore.rawAccessToken)
  const map: Record<string, AiTaskHealth> = {}
  for (const item of list)
    map[item.taskId] = item
  aiHealth.value = map
}

function taskHealthDisplay(row: API.GetTaskPageResult) {
  if (row.status === TaskStatus.Stopped)
    return { text: '未运行', type: 'info' }
  if (row.status === TaskStatus.Exception)
    return { text: '异常', type: 'error' }
  if (row.status !== TaskStatus.Running) {
    const display = getTaskStatusDisplay(row.status || 1)
    return { text: display.text, type: display.type as any }
  }
  const h = aiHealth.value[row.id!]
  if (!h)
    return { text: '启动中', type: 'warning' }
  if (h.health === 'ok')
    return { text: '运行中', type: 'success' }
  if (h.health === 'stalled')
    return { text: 'Input/output error', type: 'error' }
  if (h.health === 'error')
    return { text: compactStreamError(h.lastError) || 'Input/output error', type: 'warning' }
  if (h.lastError)
    return { text: compactStreamError(h.lastError), type: 'warning' }
  return { text: '正在连接', type: 'warning' }
}

const pagination = usePagination({ fn: getList })
enum TaskStatus {
  Running = 1,
  Stopped = 2,
  Exception = 3,
}

async function getList() {
  try {
    startLoading()
    const [error, r1]: any = await utils.tryit(api.task.getTaskGetTaskPage)({
      PageIndex: pagination.page,
      PageSize: pagination.pageSize,
    })
    if (error || !r1.isSuccess)
      return
    taskData.value = r1.data.list || []
    pagination.itemCount = r1.data.totalCount
  }
  catch (error) {
    console.error('获取任务列表失败:', error)
  }
  finally {
    endLoading()
  }
}

const columns: DataTableColumns<API.GetTaskPageResult> = [
  { title: '任务编号', key: 'code', width: '28em' },
  { title: '通道名称', key: 'channelName', width: '28em' },
  {
    title: '算法配置信息',
    key: 'modelNames',
    render(row: API.GetTaskPageResult) {
      return (
        <div class="flex flex-wrap gap-2">
          {row.modelNames?.map((modelName: string, index: number) => {
            const tagType = ['primary', 'success', 'warning', 'error'][index % 4] as 'primary' | 'success' | 'warning' | 'error'
            return (
              <div key={`${modelName}-${index}`} class="flex items-center gap-1">
                <CustomTag type={tagType}>
                  {modelName}
                </CustomTag>
              </div>
            )
          })}
        </div>
      )
    },
  },
  {
    title: '状态',
    key: 'status',
    width: '12em',
    render(row: API.GetTaskPageResult) {
      const statusDisplay = taskHealthDisplay(row)
      return (
        <CustomTag type={statusDisplay.type as 'primary' | 'success' | 'warning' | 'error'}>
          {statusDisplay.text}
        </CustomTag>
      )
    },
  },
  {
    title: '操作',
    align: 'center' as const,
    key: 'actions',
    width: '22em',
    render: (row: API.GetTaskPageResult) => {
      return (
        <div class="flex justify-center gap-2">
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => onAddOrEdit(row)}
          >
            编辑
          </n-button>
          <n-button
            size="small"
            quaternary
            type={row.status === TaskStatus.Running ? 'warning' : 'success'}
            onClick={() => updateTaskStatus(row, row.status === TaskStatus.Running ? TaskStatus.Stopped : TaskStatus.Running)}
          >
            {row.status === TaskStatus.Running ? '停止' : '启动'}
          </n-button>
          <n-button
            size="small"
            quaternary
            type="error"
            onClick={() => onDeleteClick(row)}
          >
            删除
          </n-button>
          <n-button
            size="small"
            quaternary
            type="warning"
            disabled={row.status !== TaskStatus.Running}
            onClick={() => onConfigClick(row)}
          >
            配置区域
          </n-button>
        </div>
      )
    },
  },
]

function init() {
  pagination.page = 1
  getList()
}
onMounted(async () => {
  init()
  refreshAiHealth()
  healthTimer = setInterval(refreshAiHealth, 3000)
  // 初始化系统字典
  await appStore.loadSystemDict()
})

onUnmounted(() => {
  if (healthTimer)
    clearInterval(healthTimer)
})

function onAddOrEdit(row: API.GetTaskPageResult) {
  if (row && row.id) {
    // 编辑模式：跳转到编辑页面
    router.push({
      name: 'taskManagementEdit',
      params: { id: row.id },
    })
  }
  else {
    // 新增模式：跳转到新增页面
    router.push({
      name: 'taskManagementEdit',
    })
  }
}

const MAX_RUNNING_TASKS = 36 // 4 merge groups x 9 slots

async function countRunningTasks() {
  const [error, r]: any = await utils.tryit(api.task.getTaskGetTaskPage)({
    PageIndex: 1,
    PageSize: 200,
  })
  if (error || !r?.isSuccess)
    return -1
  return (r.data?.list || []).filter((item: API.GetTaskPageResult) => item.status === TaskStatus.Running).length
}

async function updateTaskStatus(row: API.GetTaskPageResult, status: TaskStatus) {
  try {
    startLoading()
    // The AI side has only 4 x 9 merge slots.  Check before asking the
    // backend, so the 37th task is rejected in the UI with a useful message
    // instead of being marked Running and later failing in 配置区域.
    if (status === TaskStatus.Running) {
      const running = await countRunningTasks()
      if (running >= MAX_RUNNING_TASKS) {
        window.$message.warning(`合流槽位已满（最多 ${MAX_RUNNING_TASKS} 路），请先停止一个任务`)
        return
      }
    }
    const [error, r1]: any = await utils.tryit(api.task.patchTaskUpdateTaskStatusIdStatus)({
      id: row.id!,
      status,
    })
    if (error || !r1?.isSuccess) {
      window.$message.error(r1?.message || error?.message || '更新任务状态失败')
      return
    }
    getList()
    window.$message.success(status === TaskStatus.Running ? '启动成功' : '停止成功')
  }
  catch (error) {
    console.error('更新任务状态失败:', error)
    window.$message.error('更新任务状态失败')
  }
  finally {
    endLoading()
  }
}

function onConfigClick(row: API.GetTaskPageResult) {
  router.push({
    name: 'taskManagementConfig',
    params: { id: row.id },
  })
}

function onDeleteClick(row: API.GetTaskPageResult) {
  deleteTarget.value = row
  setDeleteVisible(true)
}

async function onConfirmDelete() {
  if (!deleteTarget.value?.id) {
    return window.$message.warning('ID不能为空')
  }
  try {
    startLoading()
    const [error, r1]: any = await utils.tryit(api.task.deleteTaskDeleteTaskId)({
      id: deleteTarget.value.id,
    })
    if (error || !r1.isSuccess)
      return
    getList()
    window.$message.success('删除成功')
  }
  catch (error) {
    console.error('删除任务失败:', error)
  }
  finally {
    deleteTarget.value = null
    endLoading()
  }
}
</script>

<template>
  <div class="mt-[8px] px-[74px]">
    <CardHeader title="任务管理" class="flex flex-col" :style="{ height: containerHeight }">
      <template #header-extra>
        <NButton type="info" @click="onAddOrEdit({})">
          <template #icon>
            <icon-park-outline-plus />
          </template>
          新增
        </NButton>
      </template>
      <n-data-table
        class="mt-[17px] flex-1 overflow-x-hidden overflow-y-auto"
        :columns="columns"
        :data="taskData"
        :pagination="pagination"
        :loading="loading && taskData.length === 0"
        remote
        flex-height
      />
    </CardHeader>
  </div>
  <DeleteModal v-model:visible="deleteVisible" @confirm="onConfirmDelete" />
</template>

<style lang="scss" scoped>
</style>
