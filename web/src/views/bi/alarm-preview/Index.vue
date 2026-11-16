<script setup lang="tsx">
import { computed, onMounted, ref, watch } from 'vue'
import dayjs from 'dayjs'
import AlarmCard from './components/AlarmCard.vue'
import TableModal from './components/TableModal.vue'
import { resolveAlarmMediaUrl } from './media'
import { useBoolean, useLoading, usePagination } from '@/hooks'
import { useResponsiveHeight } from '@/hooks/useResponsiveHeight'
import noData from '@/assets/images/no_data.png'

// 批量模式状态
const { bool: isBatchMode, setBool: setIsBatchMode } = useBoolean(false)
const { bool: deleteVisible, setBool: setDeleteVisible } = useBoolean(false)
const { bool: visible, setBool: setVisible } = useBoolean(false)
const { loading, startLoading, endLoading } = useLoading(false)

// 响应式高度计算 - 适配autofit缩放
const { height: containerHeight } = useResponsiveHeight(124)
const selectedAlarmIds = ref<string[]>([])
const deleteTarget = ref<API.QueryAlarmPageResult | null>(null)
const detailId = ref<string | null>(null)
const selectedTaskOpts = ref<{ label: string, value: string }[]>([])
const selectedAlarmTypeOpts = ref<{ label: string, value: string }[]>([])
const selectedChannelOpts = ref<{ label: string, value: string }[]>([])
const selectedTask = ref<string | null>(null)
const selectedChannel = ref<string | null>(null)
const selectedAlarmType = ref<string | null>(null)
const selectedTimeRange = ref<[number, number] | null>(null)

const alarmData = ref<API.QueryAlarmPageResult[]>([])

function onDeleteClick(row: any) {
  deleteTarget.value = row
  setDeleteVisible(true)
}

async function onConfirmDelete() {
  if (!deleteTarget.value?.id) {
    return window.$message.warning('ID不能为空')
  }

  try {
    startLoading()
    const [error, res] = await utils.tryit(api.alarm.deleteAlarmDeletedAlarm)({ ids: [deleteTarget.value.id] })
    if (error || !res.isSuccess)
      return
    window.$message.success('删除成功')
    getList()
    deleteTarget.value = null
  }
  catch (error) {
    console.error('删除告警失败:', error)
  }
  finally {
    endLoading()
  }
}

function onDetailClick(row: any) {
  detailId.value = row.id
  setVisible(true)
}

// 分页配置 - 每页6条数据
const pagination = usePagination({
  pageSize: 6,
  pageSizes: [6, 12, 18, 24],
  fn: () => {
    // 分页变化时的回调，这里可以添加额外逻辑
    getList()
  },
})

watch([selectedTask, selectedChannel, selectedAlarmType, selectedTimeRange], () => {
  pagination.page = 1
  getList()
}, { immediate: true })

// 当前页显示的数据
const currentPageData = computed(() => {
  const pageSize = pagination.pageSize || 6
  const currentPage = pagination.page || 1
  const startIndex = (currentPage - 1) * pageSize
  const endIndex = startIndex + pageSize
  return alarmData.value.slice(startIndex, endIndex)
})

// 全选状态 - 基于当前页数据
const isAllSelected = computed(() => {
  return currentPageData.value.length > 0
    && currentPageData.value.every(item => selectedAlarmIds.value.includes(item.id!))
})

// 批量操作函数
function toggleBatchMode() {
  setIsBatchMode(!isBatchMode.value)
  selectedAlarmIds.value = []
}

function toggleSelectAll() {
  if (isAllSelected.value) {
    // 取消选中当前页的所有项目
    const currentPageIds = currentPageData.value.map(item => item.id)
    selectedAlarmIds.value = selectedAlarmIds.value.filter(id => !currentPageIds.includes(id))
  }
  else {
    // 选中当前页的所有项目
    const currentPageIds = currentPageData.value.map(item => item.id).filter((id): id is string => id !== undefined)
    selectedAlarmIds.value = [...new Set([...selectedAlarmIds.value, ...currentPageIds])]
  }
}

async function deleteSelected() {
  if (selectedAlarmIds.value.length === 0) {
    window.$message.warning('请先选择要删除的告警')
    return
  }

  const deleteCount = selectedAlarmIds.value.length

  try {
    startLoading()
    const [error, res] = await utils.tryit(api.alarm.deleteAlarmDeletedAlarm)({ ids: selectedAlarmIds.value })
    if (error || !res.isSuccess)
      return
    window.$message.success(`删除了${deleteCount}条告警`)
    getList()
    selectedAlarmIds.value = []
  }
  catch (error) {
    console.error('删除告警失败:', error)
  }
  finally {
    endLoading()
  }
}

function cancelBatchMode() {
  setIsBatchMode(false)
  selectedAlarmIds.value = []
}

function toggleItemSelection(id: string) {
  const index = selectedAlarmIds.value.indexOf(id)
  if (index > -1) {
    selectedAlarmIds.value.splice(index, 1)
  }
  else {
    selectedAlarmIds.value.push(id)
  }
}

// 卡片点击事件处理
function handleAlarmCardClick(alarm) {
  console.log('点击了告警卡片:', alarm)
  // 这里可以添加具体的点击处理逻辑，比如打开详情弹窗等
}

async function getList() {
  try {
    startLoading()
    const [error, r1]: any = await utils.tryit(api.alarm.postAlarmQueryAlarmPage)({
      pageIndex: pagination.page,
      pageSize: pagination.pageSize,
      taskId: selectedTask.value as string,
      typeId: selectedAlarmType.value as string,
      channelId: selectedChannel.value as string,
      alarmStartTime: selectedTimeRange.value ? dayjs(selectedTimeRange.value[0]).format('YYYY-MM-DD HH:mm:ss') : undefined,
      alarmEndTime: selectedTimeRange.value ? dayjs(selectedTimeRange.value[1]).format('YYYY-MM-DD HH:mm:ss') : undefined,
    })
    if (error || !r1.isSuccess)
      return
    alarmData.value = (r1.data.list as API.QueryAlarmPageResult[]).map((item) => {
      item.imgPath = resolveAlarmMediaUrl(item.imgPath)
      return {
        ...item,
      }
    }) || []
    pagination.itemCount = r1.data.totalCount
  }
  catch (error) {
    console.error('获取任务列表失败:', error)
  }
  finally {
    endLoading()
  }
}

async function loadSelectOpts() {
  try {
    startLoading()
    const [r1, r2, r3]: any = await Promise.allSettled([utils.tryit(api.task.getTaskGetTaskDropdown)(), utils.tryit(api.channel.getChannelGetChannelDropdown)(), utils.tryit(api.alarmType.getAlarmTypeGetAlarmTypeDropdown)()])
    const [error1, res1] = r1.value
    const [error2, res2] = r2.value
    const [error3, res3] = r3.value
    if (error1 && error2 && error3)
      throw new Error(`${error1}\n${error2}\n${error3}`)

    if (!error1 && res1)
      selectedTaskOpts.value = res1.data.map(item => ({ label: item.code, value: item.id }))
    if (!error2 && res2)
      selectedChannelOpts.value = res2.data.map(item => ({ label: item.name, value: item.id }))
    if (!error3 && res3)
      selectedAlarmTypeOpts.value = res3.data.map(item => ({ label: item.remark, value: item.id }))
  }
  catch (error) {
    console.error('获取下拉框失败:', error)
  }
  finally {
    endLoading()
  }
}

onMounted(() => {
  console.log('告警预览组件已加载')
  pagination.page = 1
  loadSelectOpts()
})
</script>

<template>
  <div class="mt-[8px] px-[160px]">
    <div class="flex flex-col gap-[24px] pt-[24px]" :style="{ height: containerHeight }">
      <!-- 上层控制区域 -->
      <div class="flex items-center justify-between">
        <!-- 左侧：告警任务下拉框 -->
        <div class="flex flex-wrap items-center gap-[24px]">
          <div class="flex items-center gap-[12px]">
            <span class="whitespace-nowrap text-[14px] text-white">告警任务：</span>
            <n-select
              v-model:value="selectedTask"
              :options="selectedTaskOpts"
              placeholder="请选择告警任务"
              class="w-[200px]"
              clearable
            />
          </div>
          <div class="flex items-center gap-[12px]">
            <span class="whitespace-nowrap text-[14px] text-white">监控通道：</span>
            <n-select
              v-model:value="selectedChannel"
              :options="selectedChannelOpts"
              placeholder="请选择监控通道"
              class="w-[200px]"
              clearable
            />
          </div>
          <div class="flex items-center gap-[12px]">
            <span class="whitespace-nowrap text-[14px] text-white">告警内容：</span>
            <n-select
              v-model:value="selectedAlarmType"
              :options="selectedAlarmTypeOpts"
              placeholder="请选择告警内容"
              class="w-[200px]"
              clearable
            />
          </div>
          <div class="flex items-center gap-[12px]">
            <span class="whitespace-nowrap text-[14px] text-white">告警日期：</span>
            <n-date-picker
              v-model:value="selectedTimeRange"
              type="datetimerange"
              clearable
              class="w-[360px]"
              format="yyyy-MM-dd HH:mm:ss"
            />
          </div>
        </div>

        <!-- 右侧：批量操作区域 -->
        <div class="flex items-center gap-[12px]">
          <!-- 非批量模式 -->
          <template v-if="!isBatchMode">
            <n-button type="primary" @click="toggleBatchMode">
              批量
            </n-button>
          </template>

          <!-- 批量模式 -->
          <template v-else>
            <n-checkbox
              :checked="isAllSelected"
              @update:checked="toggleSelectAll"
            >
              全选
            </n-checkbox>
            <n-button
              type="error"
              :disabled="selectedAlarmIds.length === 0"
              @click="deleteSelected"
            >
              删除 ({{ selectedAlarmIds.length }})
            </n-button>
            <n-button @click="cancelBatchMode">
              取消批量
            </n-button>
          </template>
        </div>
      </div>

      <!-- 下层卡片区域 -->
      <div class="flex-1 overflow-x-hidden overflow-y-auto">
        <div v-if="loading" class="h-full flex flex-col items-center justify-center gap-12px text-16px color-[rgba(255,255,255,0.65)] tracking-[0.5px]">
          <n-spin size="large" />
          <p>正在加载</p>
        </div>
        <div v-else-if="alarmData.length === 0" class="h-full flex flex-col items-center justify-center">
          <img :src="noData" alt="暂无数据" class="w-100px">
        </div>
        <div v-else>
          <n-grid x-gap="33" y-gap="33" :cols="3">
            <n-gi v-for="alarm in alarmData" :key="alarm.id">
              <AlarmCard
                :alarm="alarm"
                :is-batch-mode="isBatchMode"
                :is-selected="selectedAlarmIds.includes(alarm.id!)"
                @toggle-selection="toggleItemSelection"
                @click="handleAlarmCardClick"
                @delete="onDeleteClick"
                @detail="onDetailClick"
              />
            </n-gi>
          </n-grid>
        </div>
      </div>
      <!-- 分页组件 -->
      <div class="flex justify-end">
        <n-pagination
          v-model:page="pagination.page"
          v-model:page-size="pagination.pageSize"
          :item-count="pagination.itemCount"
          :page-sizes="pagination.pageSizes"
          :show-size-picker="pagination.showSizePicker"
          :show-quick-jumper="pagination.showQuickJumper"
          :prefix="pagination.prefix"
          @update:page="pagination.onChange"
          @update:page-size="pagination.onUpdatePageSize"
        />
      </div>
    </div>
  </div>
  <TableModal v-model:visible="visible" :detail-id="detailId" />
  <DeleteModal v-model:visible="deleteVisible" content="确认要删除选中记录？" @confirm="onConfirmDelete" />
</template>

<style lang="scss" scoped>
</style>
