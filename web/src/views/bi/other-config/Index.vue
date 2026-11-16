<script setup lang="tsx">
import { computed, onMounted, reactive, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import NumberAdjuster from '@/components/common/NumberAdjuster.vue'
import { usePagination, useResponsiveHeight } from '@/hooks'

const DefaultTab = 'model-info'
const { height: containerHeight } = useResponsiveHeight(124)
const route = useRoute()
const router = useRouter()

enum TabKey {
  ModelInfo = 'model-info',
  AlertCategory = 'alert-category',
  ThresholdConfig = 'threshold-config',
}

// Tab状态管理
const activeTab = ref(DefaultTab)

// 模型信息数据
const modelData = ref<API.GetAlgoModePageResult[]>([])
// 告警类别数据
const alertData = ref<API.GetAlarmTypePageResult[]>([])
// 阈值配置数据
const thresholdData = ref<API.GetAlgoConfigPageResult[]>([])

const tabStates = {
  [TabKey.ModelInfo]: {
    fn: api.algoMode.getAlgoModeGetAlgoModePage,
    dataRef: modelData,
    loading: ref(true),
    pagination: usePagination({
      fn: () => getList(TabKey.ModelInfo),
    }),
  },
  [TabKey.AlertCategory]: {
    fn: api.alarmType.getAlarmTypeGetAlarmTypePage,
    dataRef: alertData,
    loading: ref(true),
    pagination: usePagination({
      fn: () => getList(TabKey.AlertCategory),
    }),
  },
  [TabKey.ThresholdConfig]: {
    fn: api.algoConfig.getAlgoConfigGetAlgoConfigPage,
    dataRef: thresholdData,
    loading: ref(true),
    pagination: usePagination({
      fn: () => getList(TabKey.ThresholdConfig),
    }),
  },
}

// 当前激活Tab的状态
const currentTabState = computed(() => tabStates[activeTab.value as TabKey])

// 请求取消控制
let currentRequest: AbortController | null = null

const tabs = [
  { key: TabKey.ModelInfo, label: '模型信息' },
  { key: TabKey.AlertCategory, label: '告警类别' },
  { key: TabKey.ThresholdConfig, label: '阈值配置' },
]

function switchTab(tabKey: string) {
  activeTab.value = tabKey
  // 更新URL锚点
  router.replace({
    ...route,
    hash: `#${tabKey}`,
  })
}

// 从URL锚点初始化activeTab
function initTabFromHash() {
  const hash = route.hash.replace('#', '')
  if (hash && tabs.some(tab => tab.key === hash)) {
    activeTab.value = hash
  }
  getList(activeTab.value as TabKey)
}

// 监听路由变化
watch(() => route.hash, (newHash) => {
  const hash = newHash.replace('#', '')
  if (hash && tabs.some(tab => tab.key === hash)) {
    activeTab.value = hash
  }
  else if (!hash) {
    activeTab.value = DefaultTab
  }
})

async function getList(newTab: TabKey, page?: number) {
  try {
    // 取消之前的请求
    if (currentRequest) {
      currentRequest.abort()
    }

    // 创建新的请求控制器
    currentRequest = new AbortController()

    const { fn, dataRef, loading, pagination } = tabStates[newTab]

    // 如果指定了页码，更新对应Tab的分页状态
    if (page !== undefined) {
      pagination.page = page
    }

    // 开始加载
    loading.value = true

    const [error, r1]: any = await utils.tryit(fn)({
      PageIndex: pagination.page,
      PageSize: pagination.pageSize,
    })

    // 检查请求是否被取消
    if (currentRequest?.signal.aborted) {
      return
    }

    if (error || !r1.isSuccess) {
      console.error('获取数据失败:', error)
      return
    }

    // 直接给对应的响应式属性赋值。阈值配置页只展示数值型参数；
    // 开关/下拉型参数在任务编辑页按算法展示。
    const nonNumericCodes = new Set([
      'behavior_enable_calling', 'behavior_enable_smoking', 'behavior_enable_playing',
      'crowd_status_mod',
    ])
    const list = (r1.data.list || []).filter((item: any) =>
      newTab !== TabKey.ThresholdConfig || !nonNumericCodes.has(item.code))
    dataRef.value = list.map(item => ({
      ...item,
      editing: false,
      originalValue: Number(item.configValue), // 保存原始值用于比较
    }))

    pagination.itemCount = r1.data.totalCount
  }
  catch (error) {
    // 如果是请求被取消，不显示错误
    if (error?.name !== 'AbortError') {
      console.error('获取数据异常:', error)
    }
  }
  finally {
    // 只有当前请求没有被取消时才结束loading
    if (!currentRequest?.signal.aborted) {
      tabStates[newTab].loading.value = false
    }
  }
}

// 监听activeTab变化
watch(() => activeTab.value, (newTab: TabKey) => {
  getList(newTab, 1)
})

// 处理数值更新
function handleValueUpdate(row: any, value: number) {
  row.configValue = value
}

// 处理编辑状态更新
function handleEditingUpdate(row: any, editing: boolean) {
  row.editing = editing
}

// 处理保存
async function handleSave(row: any, value: number) {
  try {
    // 如果值没变，则不发送请求
    if (row.originalValue === value) {
      return
    }
    tabStates[activeTab.value as TabKey].loading.value = true
    const [error, res]: any = await utils.tryit(api.algoConfig.putAlgoConfigUpdateAlgoConfig)({
      id: row.id,
      configValue: String(value),
    })
    if (error || !res.isSuccess)
      throw error
    window.$message.success('阈值配置更新成功')
    getList(activeTab.value as TabKey)
  }
  catch (error) {
    console.error('保存阈值配置失败:', error)
  }
  finally {
    tabStates[activeTab.value as TabKey].loading.value = false
  }
}

// 模型信息列配置
const modelColumns = [
  { title: '文件路径', key: 'filePath', width: '300px' },
  { title: '涉及算法', key: 'name' },
  { title: '版本日期', key: 'versionDate' },
  { title: '算法描述', key: 'remark', width: '400px' },
]

// 告警类别列配置
const alertColumns = [
  { title: '所属算法', key: 'algoModeName' },
  { title: '告警类别', key: 'code' },
  { title: '告警描述', key: 'remark', width: '400px' },
]

// 阈值配置列配置
const thresholdColumns = [
  { title: '模型名称', key: 'algoModeName', width: '240px' },
  { title: '参数描述', key: 'name', width: '300px' },
  {
    title: '参数值',
    key: 'configValue',
    width: '250px',
    render: (row: any) => (
      <NumberAdjuster
        value={Number(row.configValue)}
        editing={row.editing}
        min={0}
        max={100}
        step={0.1}
        precision={1}
        onUpdate:value={(value: number) => handleValueUpdate(row, value)}
        onUpdate:editing={(editing: boolean) => handleEditingUpdate(row, editing)}
        onSave={(value: number) => handleSave(row, value)}
      />
    ),
  },
]

onMounted(() => {
  // 从URL锚点初始化tab状态
  initTabFromHash()
})
</script>

<template>
  <div class="mt-[8px] flex px-[74px]">
    <!-- 左侧固定宽度的标题和Tabs区域 -->
    <CardHeader title="其他配置">
      <!-- Tabs 选项卡 -->
      <div class="ml-[30px] mt-[12px] flex flex-col gap-6px">
        <div
          v-for="tab in tabs"
          :key="tab.key"
          class="tab-item"
          :class="{ 'tab-active': activeTab === tab.key }"
          @click="switchTab(tab.key)"
        >
          {{ tab.label }}
        </div>
      </div>
    </CardHeader>

    <!-- 右侧内容区域 -->
    <div class="w-full">
      <!-- 模型信息 -->
      <div v-if="activeTab === TabKey.ModelInfo" class="fadeIn w-full flex flex-col" :style="{ height: containerHeight }">
        <div class="mb-[16px] h-[37px] flex items-center">
          <img class="mx-[11px]" src="@/assets/images/other_title_logo1.png" alt="模型信息">
          <span class="text-[18px] leading-[25px]">模型信息</span>
        </div>
        <n-data-table
          class="flex-1 overflow-x-hidden overflow-y-auto"
          :columns="modelColumns"
          :data="modelData"
          :pagination="tabStates[TabKey.ModelInfo].pagination"
          :loading="tabStates[TabKey.ModelInfo].loading.value"
          remote
          flex-height
        />
      </div>

      <!-- 告警类别 -->
      <div v-else-if="activeTab === TabKey.AlertCategory" class="fadeIn w-full flex flex-col" :style="{ height: containerHeight }">
        <div class="mb-[16px] h-[37px] flex items-center">
          <img class="mx-[11px]" src="@/assets/images/other_title_logo2.png" alt="告警类别">
          <span class="text-[18px] leading-[25px]">告警类别</span>
        </div>
        <n-data-table
          class="flex-1 overflow-x-hidden overflow-y-auto"
          :columns="alertColumns"
          :data="alertData"
          :pagination="tabStates[TabKey.AlertCategory].pagination"
          :loading="tabStates[TabKey.AlertCategory].loading.value"
          remote
          flex-height
        />
      </div>

      <!-- 阈值配置 -->
      <div v-else-if="activeTab === TabKey.ThresholdConfig" class="fadeIn w-full flex flex-col" :style="{ height: containerHeight }">
        <div class="mb-[16px] h-[37px] flex items-center">
          <img class="mx-[11px]" src="@/assets/images/other_title_logo3.png" alt="阈值配置">
          <span class="text-[18px] leading-[25px]">阈值配置</span>
        </div>
        <n-data-table
          class="flex-1 overflow-x-hidden overflow-y-auto"
          :columns="thresholdColumns"
          :data="thresholdData"
          :pagination="tabStates[TabKey.ThresholdConfig].pagination"
          :loading="tabStates[TabKey.ThresholdConfig].loading.value"
          remote
          flex-height
        />
      </div>
    </div>
  </div>
</template>

<style lang="scss" scoped>
.tab-item {
  width: 184px;
  height: 40px;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  font-size: 14px;
  font-weight: 500;
  color: #fff;
  border: 1px solid transparent;

  &:hover {
    background-color: rgba(9, 72, 255, 0.3);
  }

  &.tab-active {
    background-color: #0948ffff;
    border-color: #0948ffff;
  }
}

/* 覆盖CardHeader的宽度限制 */
:deep(.card-header-container) {
  width: 344px !important;
}
</style>
