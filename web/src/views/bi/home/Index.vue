<script lang="tsx" setup>
import { onBeforeUnmount, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import WarnPieChart from './components/WarnPieChart.vue'
import TaskLineChart from './components/TaskLineChart.vue'
import UsageLineChart from './components/UsageLineChart.vue'
import AlertCarousel from './components/AlertCarousel.vue'
import SlantedTag from './components/SlantedTag.vue'
import ResourcePanel from './components/ResourcePanel.vue'
import TaskNotice from './components/TaskNotice.vue'
import { AlarmReportStatusEnum, TaskLogStatusEnum } from '@/constants/index'
import CardHeader from '@/components/common/CardHeader.vue'
import * as utils from '@/utils'
import api from '@/services/api'

// 导入图片
import { useLoading } from '@/hooks'

const router = useRouter()
const { startLoading: startBasicInfoLoading, endLoading: endBasicInfoLoading } = useLoading(false)
const { startLoading: startHomeExtraInfoLoading, endLoading: endHomeExtraInfoLoading } = useLoading(false)

const basicInfo = ref<API.DeviceInfoResult>({})
const occupancyInfo = ref<Array<{ name: string, value: number }>>([])
const warningDataRaw = ref<any[]>([]) // 原始"告警类别比重"数据
const taskDataRaw = ref<any[]>([]) // 原始"任务告警比重"数据
const taskNoticesRaw = ref<any[]>([]) // 原始"任务公告"数据
const alertDataRaw = ref<any[]>([]) // 原始"现场告警管理"数据

// 定时器引用
let timer: NodeJS.Timeout | null = null
const anthStatus = ['未授权', '已授权']
const colors = [
  'linear-gradient(180deg, rgba(7, 91, 255, 1), rgba(48, 94, 250, 1))',
  '#F50102FF',
  'linear-gradient(136deg, rgba(8, 202, 90, 1), rgba(15, 194, 205, 1))',
  'linear-gradient(225deg, rgba(245, 101, 1, 1), rgba(245, 162, 2, 1))',
]

// 使用率数据存储历史记录
const usageHistory = ref<Array<{
  boxTime: string
  fullTime: string
  items: Array<{ name: string, value: number }>
}>>([])

// 使用率图表数据
const usageData = ref({
  months: [] as string[],
  fullTimes: [] as string[],
  series: [] as { name: string, data: number[] }[],
  maxValue: 100,
})

// 警告类别比重
const warningData = computed(() => {
  const sortData = warningDataRaw.value?.slice().sort((a, b) => (b.alertCount || 0) - (a.alertCount || 0)) || []
  return sortData.map((item, index) => ({
    name: item.alertType || '',
    count: item.alertCount || 0,
    color: colors[index % colors.length],
  })) || []
})

// 任务警告比重
const taskData = computed(() => {
  const categories: string[] = []
  const values: number[] = []
  const percentage: number[] = []
  const totalCount = taskDataRaw.value.reduce((total, item) => total + (item.taskAlarmCount || 0), 0) || 0
  taskDataRaw.value.forEach((item) => {
    categories.push(item.taskCode || '')
    values.push(item.taskAlarmCount || 0)
    percentage.push((item.taskAlarmCount || 0) / totalCount * 100)
  })
  return {
    categories,
    values,
    percentage,
    maxValue: taskDataRaw.value.reduce((max, item) => Math.max(max, item.taskAlarmCount || 0), 0) || 0,
  }
})

// 任务公告
const taskNotices = computed(() => {
  return taskNoticesRaw.value.map(item => ({
    type: item.status === TaskLogStatusEnum.success ? 'success' : 'error',
    title: item.remark || '',
    time: item.created,
  })).slice(0, 10) || []
})

// 现场告警管理
const alertData = computed(() => {
  return alertDataRaw.value.map(item => ({
    title: item.title || '',
    reportStatus: item.reportStatus || AlarmReportStatusEnum.pending,
    time: item.time || '',
    image: `${import.meta.env.VITE_BASE_API}${item.image}`,
  })).slice(0, 10) || []
})

// 资源信息数据
const resourceData = computed(() => [
  { label: '存储剩余空间', value: basicInfo.value.resourceInformation || '0' },
  { label: '芯片温度', value: basicInfo.value.chipTemperature || '0' },
])

async function loadHomeExtraInfo() {
  try {
    startHomeExtraInfoLoading()
    const [r0, r1, r2, r3]: any = await Promise.allSettled([
      utils.tryit(api.device.getDeviceGetAlogAlarmStats)(),
      utils.tryit(api.device.getDeviceGetTopTaskAlarm)(), // 任务告警比重
      utils.tryit(api.device.getDeviceGetTaskLogs)(), // 任务公告
      utils.tryit(api.device.getDeviceGetDeviceAlarms)(), // 现场告警管理
    ])
    const [error0, res0] = r0.value
    const [error1, res1] = r1.value
    const [error2, res2] = r2.value
    const [error3, res3] = r3.value
    if (error0 && error1 && error2 && error3)
      throw new Error(`${error0}\n${error1}\n${error2}\n${error3}`)

    if (!error0 && res0)
      warningDataRaw.value = res0.data
    if (!error1 && res1)
      taskDataRaw.value = res1.data
    if (!error2 && res2)
      taskNoticesRaw.value = res2.data
    if (!error3 && res3)
      alertDataRaw.value = res3.data
  }
  catch (error) {
    console.error('获取首页信息失败:', error)
  }
  finally {
    endHomeExtraInfoLoading()
  }
}

async function loadHomeBasicInfo() {
  try {
    startBasicInfoLoading()
    const [r0]: any = await Promise.allSettled([
      await utils.tryit(api.device.getDeviceGetOccupancy)(), // 占用率与home信息
    ])
    const [error0, res0] = r0.value
    if (error0)
      throw new Error(`${error0}`)
    if (!error0 && res0) {
      const rawData = res0?.data?.series?.[0]?.data || []
      occupancyInfo.value = Array.isArray(rawData) ? rawData : []
      const home = res0?.data?.series?.[0]?.home || {}

      basicInfo.value = {
        ...basicInfo.value,
        chipTemperature: home.chip_temp ?? basicInfo.value.chipTemperature,
        graphicsCardTemperature: home.board_temp ?? basicInfo.value.graphicsCardTemperature,
        resourceInformation: home.disk_free ?? basicInfo.value.resourceInformation,
      }
    }

    // 记录 usages 数据
    if (occupancyInfo.value.length > 0) {
      const currentTime = new Date()
      const boxTime = `${currentTime.getHours().toString().padStart(2, '0')}:${currentTime.getMinutes().toString().padStart(2, '0')}:${currentTime.getSeconds().toString().padStart(2, '0')}`
      const fullTime = `${currentTime.getFullYear()}-${(currentTime.getMonth() + 1).toString().padStart(2, '0')}-${currentTime.getDate().toString().padStart(2, '0')} ${boxTime}`

      // 添加新数据到历史记录
      usageHistory.value.push({
        boxTime,
        fullTime,
        items: occupancyInfo.value.map(item => ({
          name: item.name || '未知',
          value: item.value || 0,
        })),
      })

      // 只保留最近6条数据
      if (usageHistory.value.length > 6) {
        usageHistory.value = usageHistory.value.slice(-6)
      }

      // 更新图表数据
      updateUsageChartData()
    }
    else {
      // 即使没有数据，也尝试更新图表（清空）
      updateUsageChartData()
    }
  }
  catch (error) {
    console.error('获取基础信息失败:', error)
  }
  finally {
    endBasicInfoLoading()
  }
}

async function loadInitialHomeBasicInfo() {
  try {
    startBasicInfoLoading()
    const [r0]: any = await Promise.allSettled([
      await utils.tryit(api.device.getDeviceHomeBasicInfo)(),
    ])
    const [error0, res0] = r0.value
    if (!error0 && res0)
      basicInfo.value = res0.data
  }
  catch (error) {
    console.error('获取基础信息失败:', error)
  }
  finally {
    endBasicInfoLoading()
  }
}

// 更新图表数据
function updateUsageChartData() {
  // 只显示最近6条数据的时间和值
  const recentData = usageHistory.value.slice(-6)
  usageData.value.months = recentData.map(item => item.boxTime)
  usageData.value.fullTimes = recentData.map(item => item.fullTime)

  if (recentData.length > 0) {
    const names = recentData[0].items.map(item => item.name)
    usageData.value.series = names.map(name => ({
      name,
      data: recentData.map(d => d.items.find(i => i.name === name)?.value || 0),
    }))
  }
  else {
    usageData.value.series = []
  }
}

// 启动定时器
function startTimer() {
  // 清除现有定时器
  if (timer) {
    clearInterval(timer)
  }

  // 每5秒调用一次接口
  timer = setInterval(() => {
    loadHomeBasicInfo()
  }, 5000)
}

// 停止定时器
function stopTimer() {
  if (timer) {
    clearInterval(timer)
    timer = null
  }
}

function init() {
  loadInitialHomeBasicInfo()
  loadHomeBasicInfo()
  loadHomeExtraInfo()
  startTimer()
}

onMounted(() => {
  init()
})

onBeforeUnmount(() => {
  stopTimer()
})
</script>

<template>
  <main class="dashboard-main">
    <!-- 左侧数据面板 -->
    <div class="left-panel">
      <!-- 基础信息面板 -->
      <CardHeader title="基础信息">
        <div>
          <div class="mt-[25px] w-[440px] flex gap-[16px]">
            <div
              class="h-[79px] flex flex-1 flex-col items-center justify-center bg-[url(@/assets/images/item_bg1.png)] bg-cover bg-center bg-no-repeat"
            >
              <div class="text-[14px] leading-[20px]">
                芯片温度
              </div>
              <div>
                <span class="text-[32px] font-bold leading-[40px]">{{ basicInfo.chipTemperature }}&nbsp;</span>
                <span class="text-[14px] leading-[20px]">℃</span>
              </div>
            </div>
            <div class="h-[79px] flex flex-1 flex-col items-center justify-center bg-[url(@/assets/images/item_bg2.png)] bg-cover bg-center bg-no-repeat">
              <div class="stat-label">
                板卡温度
              </div>
              <div>
                <span class="text-[32px] font-bold leading-[40px]">{{ basicInfo.graphicsCardTemperature }}&nbsp;</span>
                <span class="text-[14px] leading-[20px]">℃</span>
              </div>
            </div>
          </div>
          <div class="mt-[14px] h-[78px] w-[440px] flex flex-col items-start justify-center bg-[url(@/assets/images/item_bg3.png)] bg-cover bg-center bg-no-repeat">
            <div class="px-[85px] px-[85px] text-[14px] leading-[20px]">
              <p>设备标识</p>
              <p>{{ basicInfo.deviceIdentification }}</p>
            </div>
          </div>
        </div>
      </CardHeader>

      <!-- 系统使用率面板 -->
      <CardHeader title="系统资源使用率（%）">
        <UsageLineChart :data="usageData" :title="basicInfo.usages?.[0]?.name || 'CORE使用率'" />
      </CardHeader>

      <!-- 数据统计面板 -->
      <CardHeader title="警告类别比重">
        <WarnPieChart :data="warningData" />
      </CardHeader>
    </div>

    <!-- 中央占位区域(保持flex布局结构) -->
    <div class="flex-1" />

    <!-- 中央3D展示区(固定定位，不影响flex布局) -->
    <div class="center-view">
      <!-- 标签 -->
      <div class="absolute left-[45%] top-[26%]">
        <SlantedTag variant="status">
          <span>状态: {{ anthStatus[basicInfo.authorizationStatus || 0] }}</span>
        </SlantedTag>
      </div>
      <div class="absolute left-[50%] top-[35%]">
        <SlantedTag variant="version">
          系统版本: {{ basicInfo.systemVersion }}
        </SlantedTag>
      </div>
      <!-- 资源信息面板 -->
      <div class="resource-panel">
        <ResourcePanel
          title="资源信息"
          :data="resourceData"
        />
      </div>
    </div>

    <!-- 右侧面板 -->
    <div class="right-panel">
      <!-- 任务统计 -->
      <CardHeader title="任务警告比重">
        <TaskLineChart :data="taskData" />
      </CardHeader>

      <!-- 任务公告 -->
      <CardHeader title="任务公告" clickable @header-click="router.push({ name: 'taskManagement' })">
        <TaskNotice :data="taskNotices" />
      </CardHeader>

      <!-- 现场告警管理 -->
      <CardHeader title="现场告警管理" clickable @header-click="router.push({ name: 'alarmPreview' })">
        <div class="h-[200px] min-h-[200px]">
          <AlertCarousel :data="alertData" />
        </div>
      </CardHeader>
    </div>
  </main>
</template>

<style scoped lang="scss">
.dashboard-main {
  display: flex;
  height: 100%;
  padding: 8px 74px 20px;
  gap: 20px;
  position: relative;
  z-index: 5;
  box-sizing: border-box;
}

.left-panel,
.right-panel {
  display: flex;
  flex-direction: column;
  gap: 40px;
  width: 400px; /* 固定宽度 */
  flex-shrink: 0; /* 防止被挤压 */
  min-height: 0; /* 允许flex子项缩小 */
}
.right-panel {
  gap: 30px !important;
}

.center-view {
  position: fixed;
  top: 0;
  left: 0;
  height: 100%;
  width: 100%;
  z-index: 1;
  pointer-events: none;
}

.info-stats {
  display: flex;
  gap: 30px;
  margin-bottom: 20px;

  .stat-item {
    text-align: center;

    .stat-label {
      font-size: 12px;
      color: #8cc8ff;
      margin-bottom: 5px;
    }

    .stat-value {
      font-size: 24px;
      font-weight: bold;
      color: #00f7ff;

      &.red {
        color: #ff6b6b;
      }
    }
  }
}

/* 自定义滚动条 */
::-webkit-scrollbar {
  width: 2px;
}

.resource-panel {
  position: absolute;
  left: 50%;
  top: 11%;
}

/* 2K屏幕下调整资源信息面板 */
@media screen and (min-width: 2560px) and (min-height: 1340px) {
  .resource-panel {
    left: 49%;
    top: 10%;
    transform: scale(1.6);
  }
}
</style>
