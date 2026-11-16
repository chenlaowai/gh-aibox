<script setup lang="tsx">
import { onMounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useLoading, useNaiveForm } from '@/hooks'
import { useResponsiveHeight } from '@/hooks/useResponsiveHeight'
import CardHeader from '@/components/common/CardHeader.vue'
import { useAuthStore } from '@/store/auth'
import { triggerAiTaskReload } from '@/services/aiTask'

const route = useRoute()
const router = useRouter()
const authStore = useAuthStore()
const { loading, startLoading, endLoading } = useLoading(false)
const { formRef, validate } = useNaiveForm()

// 响应式高度计算 - 适配autofit缩放 (278px是表单上方的固定高度)
const { height: formHeight } = useResponsiveHeight(278)

enum AuthorizationStatus {
  Unauthorized = 0,
  Authorized = 1,
}

const channelOpts = ref<{ label: string, value: string }[]>([])
const algoModeList = ref<API.GetAlgoModeDropdownResult[]>([])
const authorizedAlgorithms = computed(() => algoModeList.value.filter(item => item.authorizationStatus === AuthorizationStatus.Authorized))
const unauthorizedAlgorithms = computed(() => algoModeList.value.filter(item => item.authorizationStatus === AuthorizationStatus.Unauthorized))

// 任务编码（用于编辑模式）
const taskId = ref(route.params.id as string || '')
const taskDetail = ref<API.GetTaskDetailResult | null>(null)

interface TaskForm {
  code: string
  remark: string
  channelId?: string
  reportAddress: string
  frameInterval: number
  detectThreshold: number
  cooldownTime: number
  videoDuration: number
  detectSection: number
}
// 表单数据
const formModel = ref<TaskForm>({
  code: '',
  remark: '',
  channelId: undefined,
  reportAddress: '',
  frameInterval: 1,
  detectThreshold: 30,
  cooldownTime: 3,
  videoDuration: 10,
  detectSection: 30,
})

interface AlgoConfigVO {
  algoId: string
  algoName: string
  configs: Array<API.TaskAlgoConfig & {
    step?: number
    min?: number
    max?: number
    options?: { label: string, value: string }[]
  }>
}

// 缓存管理器
const algoConfigCache = reactive(new Map<string, any[]>())

const selectedAlgoIds = ref<string[]>([]) // 选中的算法ID数组（用于界面绑定）
const currentConfigs = ref<AlgoConfigVO[]>([]) // 当前选中的配置项

// 监听选中算法的变化，自动转换数据格式
watch(selectedAlgoIds, async (newIds, oldIds) => {
  const added = newIds.filter(id => !oldIds.includes(id))

  for (const algoId of added) {
    await loadAlgoConfig(algoId)
  }

  updateCurrentConfigs()
}, { deep: true })

// Production stores each parameter's type/range/options in tb_algo_config:
// value_type = input-number | switch | select, and remark is a JSON object
// {min,max,step,options}. Flatten it for the renderer.
function normalizeConfig(c: any) {
  let meta: any = {}
  if (c?.remark && typeof c.remark === 'string' && c.remark.trim().startsWith('{')) {
    try { meta = JSON.parse(c.remark) } catch {}
  }
  const out = { ...c, ...meta }
  if (out.valueType === 'select') {
    const n = Number(out.configValue)
    out.configValue = Number.isFinite(n) ? String(n) : String(out.configValue ?? '')
    out.options = (out.options || []).map((o: any) => ({ ...o, value: String(o.value) }))
  }
  return out
}

async function loadAlgoConfig(algoId: string) {
  if (algoConfigCache.has(algoId)) {
    console.log(`从缓存加载算法${algoId}的配置`)
    return
  }
  try {
    console.log(`请求算法${algoId}的配置`)
    const [error, res]: any = await utils.tryit(api.algoMode.getAlgoModeGetAlgoModelConfig)({
      algoId,
    })
    if (error || !res.isSuccess)
      throw error
    const defaults = (res.data || []).map(normalizeConfig)
    // Existing tasks may still carry only the old three parameters. Merge by
    // code so newly-added production parameters appear with their defaults
    // while the task's saved values win.
    // Add mode has no taskDetail yet.  The old `idx !== -1` check treated
    // undefined as true and then dereferenced `taskDetail.value!`, so every
    // algorithm checked on the 新增任务 page threw and left the config area
    // blank.  Only read saved values when the detail really exists.
    const taskAlgo = taskDetail.value?.taskAlgoResult
    const idx = taskAlgo ? taskAlgo.findIndex(item => item.algoId === algoId) : -1
    const saved = (idx >= 0 && taskAlgo?.[idx])
      ? (taskAlgo[idx].valueConfig || [])
      : []
    if (saved.length > 0) {
      const byCode = new Map<string, any>(saved.map((x: any) => [x.code, x]))
      algoConfigCache.set(algoId, defaults.map((d: any) => {
        const s = byCode.get(d.code)
        return s ? normalizeConfig({ ...d, configValue: s.configValue }) : d
      }))
    }
    else {
      algoConfigCache.set(algoId, defaults)
    }
  }
  catch (error) {
    console.error(`加载算法${algoId}配置失败`, error)
  }
}

function updateCurrentConfigs() {
  currentConfigs.value = []
  selectedAlgoIds.value.forEach((algoId) => {
    const algoName = algoModeList.value.find(algo => algo.id === algoId)?.name
    const configs = algoConfigCache.get(algoId)
    if (configs) {
      currentConfigs.value.push({
        algoId,
        algoName: algoName || '',
        configs,
      })
    }
  })
}

// 表单验证规则
const rules = {
  code: [{ required: true, message: '请输入任务编号' }, { message: '编码不超过50个字符', trigger: ['input', 'blur'], max: 50 }],
  remark: [{ required: true, message: '请输入任务描述' }, { message: '描述不超过100个字符', trigger: ['input', 'blur'], max: 100 }],
  channelId: [{ required: true, message: '请输入视频编号' }],
  reportAddress: [{ required: true, message: '请输入上报地址URL' }, { message: 'URL不超过255个字符', trigger: ['input', 'blur'], max: 255 }],
  frameInterval: [{ required: true, message: '请输入间隔帧数' }, { type: 'number' as const, min: 2, message: '间隔帧数必须大于等于2', trigger: ['input', 'blur'] }],
  detectThreshold: [{ required: true, message: '请输入触发帧数' }, { type: 'number' as const, min: 1, message: '触发帧数必须大于等于1', trigger: ['input', 'blur'] }],
  cooldownTime: [{ required: true, message: '请输入上报间隔' }, { type: 'number' as const, min: 3, message: '上报间隔必须大于等于3', trigger: ['input', 'blur'] }],
  videoDuration: [{ required: true, message: '请输入告警视频时长' }, { type: 'number' as const, min: 5, max: 20, message: '告警视频时长必须在5到20之间', trigger: ['input', 'blur'] }],
  detectSection: [{ required: true, message: '请输入触发区间' }, { type: 'number' as const, min: 1, max: 20, message: '触发区间必须在1到20之间', trigger: ['input', 'blur'] }],
}

// 是否为编辑模式
const isEditMode = computed(() => !!taskId.value)
const pageTitle = computed(() => isEditMode.value ? '编辑任务' : '新增任务')

// 初始化表单数据
async function initFormData() {
  if (isEditMode.value) {
    try {
      startLoading()
      const [error, res1]: any = await utils.tryit(api.task.getTaskGetTaskDetailId)({
        id: taskId.value,
      })
      if (error || !res1.isSuccess)
        return
      const taskData = res1.data || {}
      taskDetail.value = taskData
      formModel.value = {
        code: taskData.code || '',
        remark: taskData.remark || '',
        channelId: taskData.channelId || '',
        reportAddress: taskData.reportAddress || '',
        frameInterval: 1,
        detectThreshold: 30,
        detectSection: 30,
        videoDuration: 10,
        cooldownTime: 3,
      }

      // 初始化选中的算法ID数组
      if (taskData.taskAlgoResult && Array.isArray(taskData.taskAlgoResult)) {
        selectedAlgoIds.value = taskData.taskAlgoResult.map((algo: API.TaskAlgoResult) => algo.algoId).filter(Boolean)
      }
    }
    catch (error) {
      console.error('获取任务详情失败:', error)
    }
    finally {
      endLoading()
    }
  }
}

// 将选中的算法ID转换为TaskAlgoRequest格式
function convertSelectedAlgosToRequest(algoIds: string[]): API.TaskAlgoRequest[] {
  return algoIds.map(algoId => ({
    algoId,
    valueConfig: currentConfigs.value.find(item => item.algoId === algoId)?.configs || [],
  }),
  )
}

// 保存任务
async function handleSave() {
  try {
    await validate()
    startLoading()
    // 任务级时间参数不再由用户编辑：只保留参数配置里的全局上报间隔/视频时长，
    // 其余按生产蓝框倒计时逻辑内部固定。
    try {
      const [e, r]: any = await utils.tryit(api.paramConfig.getParamConfigGetParamConfigPage)({ PageIndex: 1, PageSize: 100 })
      if (!e && r?.isSuccess) {
        const rows = r.data.list || []
        const interval = rows.find((x: any) => x.code === 'AlarmReportIntervalMinutes')
        const duration = rows.find((x: any) => x.code === 'AlarmVideoDurationSeconds')
        if (interval) formModel.value.cooldownTime = Number(interval.paramValue) || 3
        if (duration) formModel.value.videoDuration = Number(duration.paramValue) || 10
      }
    }
    catch {}
    formModel.value.frameInterval = 1
    formModel.value.detectThreshold = 30
    formModel.value.detectSection = 30
    const taskAlgoRequest = convertSelectedAlgosToRequest(selectedAlgoIds.value)
    const fn = isEditMode.value
      ? api.task.putTaskUpdateTask
      : api.task.postTaskAddTask
    const [error, res]: any = await utils.tryit(fn)({
      id: isEditMode.value ? taskId.value : '',
      ...formModel.value,
      channelId: formModel.value.channelId!,
      taskAlgoRequest,
    })
    if (error || !res.isSuccess)
      throw error
    window.$message.success(isEditMode.value ? '任务更新成功' : '任务创建成功')
    if (isEditMode.value)
      void triggerAiTaskReload(taskId.value, authStore.rawAccessToken)
    router.push({ name: 'taskManagement' })
  }
  catch (error) {
    console.error('表单验证失败:', error)
  }
  finally {
    endLoading()
  }
}

// 取消操作
function handleCancel() {
  router.back()
}

// 移除未使用的重置函数
onMounted(async () => {
  utils.tryit(api.algoMode.getAlgoModeGetAlgoModeDropdown)().then(([error, res]) => {
    if (error || !res.isSuccess)
      return
    algoModeList.value = res.data || []
    // selectedAlgoIds may have been initialized before this request finished;
    // refresh the card headings with the real algorithm names.
    updateCurrentConfigs()
  }).catch((error) => {
    console.error('获取算法配置列表失败', error)
  })
  utils.tryit(api.channel.getChannelGetChannelDropdown)().then(([error, res]) => {
    if (error || !res.isSuccess)
      return
    channelOpts.value = res.data?.map((item: API.GetChannelDropdownResult) => ({ label: item.name!, value: item.id! })) || []
  }).catch((error) => {
    console.error('获取通道下拉列表失败', error)
  })
  initFormData()
})
</script>

<template>
  <div class="mt-[8px] px-[74px]">
    <CardHeader :title="pageTitle">
      <template #prefix>
        <!-- 返回按钮 -->
        <n-button
          text
          type="primary"
          size="medium"
          class="return-button"
          @click="handleCancel"
        >
          <template #icon>
            <icon-park-outline-return />
          </template>
          返回
        </n-button>
      </template>
      <n-form
        ref="formRef"
        :model="formModel"
        :rules="rules"
        label-placement="left"
        label-width="130px"
        require-mark-placement="left"
        size="medium"
        class="mt-25px overflow-y-auto"
        :style="{ height: formHeight }"
      >
        <n-grid :cols="4" :x-gap="24" :y-gap="16">
          <!-- 任务编号 -->
          <n-gi :span="2">
            <n-form-item label="任务编号" path="code">
              <n-input
                v-model:value="formModel.code"
                v-trim
                :disabled="isEditMode"
                placeholder="请输入任务编号"
              />
            </n-form-item>
          </n-gi>
          <!-- 任务名称 -->
          <n-gi :span="2">
            <n-form-item label="任务描述" path="remark">
              <n-input
                v-model:value="formModel.remark"
                v-trim
                placeholder="请输入任务名称"
              />
            </n-form-item>
          </n-gi>
          <!-- 视频地址 -->
          <n-gi :span="4">
            <n-form-item label="通道名称" path="channelId">
              <n-select
                v-model:value="formModel.channelId"
                :options="channelOpts"
                placeholder="请选择通道"
              />
            </n-form-item>
          </n-gi>
          <!-- 上报地址 -->
          <n-gi :span="4">
            <n-form-item label="上报地址" path="reportAddress">
              <n-input
                v-model:value="formModel.reportAddress"
                v-trim
                placeholder="请输入上报地址URL"
              />
            </n-form-item>
          </n-gi>
          <!-- 算法配置 -->
          <n-gi :span="4">
            <CardHeader title="算法配置">
              <n-form-item path="configItems" class="mt-[20px]">
                <n-tabs type="line" animated>
                  <!-- 已授权标签页 -->
                  <n-tab-pane name="authorizedAlgorithms" tab="已授权-可完全使用">
                    <n-checkbox-group v-model:value="selectedAlgoIds">
                      <n-grid :cols="8" :x-gap="16" :y-gap="12">
                        <n-gi v-for="item in authorizedAlgorithms" :key="item.id">
                          <n-checkbox :value="item.id" class="w-full">
                            <span class="text-center text-sm">{{ item.name }}</span>
                          </n-checkbox>
                        </n-gi>
                      </n-grid>
                    </n-checkbox-group>
                  </n-tab-pane>

                  <!-- 未授权标签页 -->
                  <n-tab-pane name="unauthorized" tab="未授权-可体验使用">
                    <n-checkbox-group v-model:value="selectedAlgoIds">
                      <n-grid :cols="8" :x-gap="16" :y-gap="12">
                        <n-gi v-for="item in unauthorizedAlgorithms" :key="item.id">
                          <n-checkbox :value="item.id" class="w-full">
                            <span class="text-center text-sm">{{ item.name }}</span>
                          </n-checkbox>
                        </n-gi>
                      </n-grid>
                    </n-checkbox-group>
                  </n-tab-pane>
                </n-tabs>
              </n-form-item>
            </CardHeader>
          </n-gi>

          <n-gi v-if="currentConfigs.length" :span="4">
            <div>
              <div
                v-for="algo in currentConfigs"
                :key="algo.algoName"
                class="algo-config-card"
              >
                <div>
                  <h4 class="mb-4 text-lg text-white font-medium">
                    {{ algo.algoName || '算法配置' }}
                  </h4>
                </div>
                <n-grid :cols="4" :y-gap="12">
                  <n-gi
                    v-for="config in algo.configs"
                    :key="config.configId || config.code"
                  >
                    <n-form-item :label="config.name" label-placement="left">
                      <n-input-number
                        v-if="config.valueType === 'input-number'"
                        :value="Number(config.configValue)"
                        :precision="2"
                        :step="config.step !== undefined ? Number(config.step) : 0.1"
                        :placeholder="`请输入${config.name}`"
                        @update:value="val => config.configValue = val === null ? '' : String(val)"
                      />
                      <n-switch
                        v-else-if="config.valueType === 'switch'"
                        :value="String(config.configValue) === 'true' || String(config.configValue) === '1'"
                        @update:value="val => config.configValue = val ? '1' : '0'"
                      />
                      <n-select
                        v-else-if="config.valueType === 'select'"
                        v-model:value="config.configValue"
                        :options="config.options"
                        :placeholder="`请选择${config.name}`"
                      />
                      <n-slider
                        v-else-if="config.valueType === 'slider'"
                        :value="config.configValue === '' || config.configValue === null || config.configValue === undefined ? 0.5 : Number(config.configValue)"
                        :min="config.min !== undefined ? Number(config.min) : 0"
                        :max="config.max !== undefined ? Number(config.max) : 1"
                        :step="config.step !== undefined ? Number(config.step) : 0.1"
                        @update:value="val => config.configValue = String(val)"
                      />
                      <n-checkbox
                        v-else-if="config.valueType === 'checkbox'"
                        :checked="String(config.configValue) === 'true'"
                        @update:checked="val => config.configValue = String(val)"
                      />
                      <n-radio-group
                        v-else-if="config.valueType === 'radio'"
                        v-model:value="config.configValue"
                        :name="config.code"
                      >
                        <n-space>
                          <n-radio
                            v-for="opt in config.options"
                            :key="opt.value"
                            :value="opt.value"
                          >
                            {{ opt.label }}
                          </n-radio>
                        </n-space>
                      </n-radio-group>
                      <n-input
                        v-else-if="config.valueType === 'input'"
                        v-model:value="config.configValue"
                        v-trim
                        :placeholder="`请输入${config.name}`"
                      />
                    </n-form-item>
                  </n-gi>
                  <n-gi v-if="!algo.configs.length">
                    <n-empty description="该算法暂无可配置参数" />
                  </n-gi>
                </n-grid>
              </div>
            </div>
          </n-gi>
        </n-grid>
      </n-form>
      <!-- 操作按钮 -->
      <div class="mt-8 flex justify-end gap-5">
        <CustomButton class="h-[40px] w-[110px]" @click="handleCancel">
          取消
        </CustomButton>
        <CustomButton
          class="h-[40px] w-[110px]"
          type="warn"
          :loading="loading"
          @click="handleSave"
        >
          {{ isEditMode ? '更新' : '保存' }}
        </CustomButton>
      </div>
    </CardHeader>
  </div>
</template>

<style lang="scss" scoped>
// 返回按钮样式
.return-button {
  font-size: 16px;
  font-weight: 500;

  :deep(.n-button__content) {
    display: flex;
    align-items: center;
    gap: 8px;
  }

  :deep(.n-button__icon) {
    font-size: 18px;
  }

  &:hover {
    color: #409eff !important;
    background-color: rgba(64, 158, 255, 0.1);
    border-radius: 6px;
  }
}

:deep(.n-checkbox) {
  margin: 0;
  padding: 12px;
  border: 1px solid transparent;
  transition: all 0.2s;

  &.n-checkbox--checked {
    border-color: #0c5cffff;
    background-color: rgba(24, 56, 160, 0.1);
  }
}
</style>
