<script setup lang="tsx">
import { computed, h, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { type MenuInst, type MenuOption, NEllipsis } from 'naive-ui'
import { useDocumentVisibility, useTimeoutFn } from '@vueuse/core'
import { usePreviewWebSocket, useResponsiveHeight } from '@/hooks/index'
// import ScaleDebugInfo from '@/components/debug/ScaleDebugInfo.vue'

const menuInstRef = ref<MenuInst | null>(null)
const selectedKey = ref<string>('')
const expandedKeys = ref<string[]>([])
const visibility = useDocumentVisibility()
let disconnectTimer: (() => void) | null = null

const {
  imageData,
  isConnected,
  connect,
  sendKey,
  disconnect,
} = usePreviewWebSocket()

// 监听页面可见性变化
watch(visibility, (current) => {
  if (current === 'hidden') {
    // 页面不可见时发送暂停指令
    if (isConnected.value) {
      sendKey('pause')
      // 启动30秒断开连接计时器
      const { stop } = useTimeoutFn(() => {
        disconnect()
      }, 30000)
      disconnectTimer = stop
    }
  }
  else if (current === 'visible') {
    // 页面可见时
    if (disconnectTimer) {
      disconnectTimer()
      disconnectTimer = null
    }

    if (!isConnected.value) {
      // 如果连接已断开，重新连接
      connect(selectedKey.value)
    }
    else {
      // 如果连接仍保持，重新发送当前选中的key以恢复传输
      sendKey(selectedKey.value)
    }
  }
})

onBeforeUnmount(() => {
  if (disconnectTimer) {
    disconnectTimer()
    disconnectTimer = null
  }
})

// 响应式高度计算 - 适配autofit缩放
const { height: containerHeight } = useResponsiveHeight(124)

// 菜单选项
const options = ref<MenuOption[]>([])
const pendingHolder = ref<string>('网络正在连接，请稍候…')
let pendingTimers: NodeJS.Timeout | null = null
const abnormalDetected = ref(false) // 标记是否已检测到异常

// 添加锁状态
const isLocked = ref(false)
const lockedTimer = ref<NodeJS.Timeout | null>(null)
const safeImageData = ref<string>('')

// 监听 imageData，只有在非锁定状态下才更新 safeImageData
watch(imageData, (newVal) => {
  if (!isLocked.value) {
    safeImageData.value = newVal
  }
})

const hasImage = computed(() => Boolean(safeImageData.value))
const displayImage = computed(() => (safeImageData.value))

// 启动异常检测定时器
function startAbnormalDetection() {
  clearTimeout(pendingTimers!)
  abnormalDetected.value = false

  pendingTimers = setTimeout(() => {
    if (!hasImage.value) {
      pendingHolder.value = '监控画面可能异常，建议重试…'
      abnormalDetected.value = true
      console.warn('监控画面异常：超过10秒未接收到图像数据')
    }
  }, 10000)
}
// 重置状态和定时器
function resetPendingState() {
  clearTimeout(pendingTimers!)
  pendingHolder.value = '网络正在连接，请稍候…'
  abnormalDetected.value = false
}
// Re-issue the current key without reloading the page; the AI service now
// sends a fresh 1 Hz reconnect/status frame so the user can see progress.
function retryCurrent() {
  if (!selectedKey.value || !isConnected.value)
    return
  imageData.value = ''
  safeImageData.value = ''
  resetPendingState()
  sendKey(selectedKey.value)
  startAbnormalDetection()
}

// 监听菜单选择变化
watch(selectedKey, (newKey) => {
  if (newKey && isConnected.value) {
    imageData.value = ''
    safeImageData.value = ''

    // 开启锁，200ms内不接收新的图片数据，防止显示上一路视频的残余帧
    isLocked.value = true
    if (lockedTimer.value)
      clearTimeout(lockedTimer.value)
    lockedTimer.value = setTimeout(() => {
      isLocked.value = false
    }, 200)

    resetPendingState()
    sendKey(newKey)
    startAbnormalDetection()
  }
})

// 监听图像数据变化
watch(hasImage, (newVal) => {
  if (newVal) {
    // 有图像时清除定时器并重置提示
    resetPendingState()
  }
  else {
    // 无图像时开始异常检测
    if (isConnected.value && !abnormalDetected.value) {
      startAbnormalDetection()
    }
  }
})

// 监听连接状态变化
watch(isConnected, (newVal) => {
  if (newVal) {
    // 连接建立时重置状态
    resetPendingState()
  }
  else {
    // 连接断开时重置状态
    resetPendingState()
  }
})

onMounted(async () => {
  try {
    const [error, r1]: any = await utils.tryit(api.task.getTaskGetTaskDropdown)()
    if (error || !r1.isSuccess)
      return

    const mapOptions = (items: API.GetTaskDropdownResult[], depth = 0): MenuOption[] => {
      return items.map(item => ({
        label: depth === 0 ? item.code : () => h(NEllipsis, null, { default: () => item.code }),
        key: item.id!,
        children: item.children ? mapOptions(item.children, depth + 1) : undefined,
      }))
    }

    // The AI detector already owns four 3x3 merge groups (all_1..all_4).
    // Older backends derive the menu from VideoRoute/9 and may return only
    // three; keep the Web UI usable by appending the missing channel.
    const merge = (r1.data || []).find((item: API.GetTaskDropdownResult) => item.id === 'root_merge')
    if (merge) {
      merge.children = merge.children || []
      const mergeIds = new Set(merge.children.map((item: API.GetTaskDropdownResult) => item.id))
      for (let i = 1; i <= 4; i++) {
        if (!mergeIds.has(`all_${i}`))
          merge.children.push({ id: `all_${i}`, code: `通道${i}` } as API.GetTaskDropdownResult)
      }
    }

    options.value = mapOptions(r1.data)
    if (options.value.length > 0) {
      const firstOption = options.value[0]
      expandedKeys.value = [firstOption.key as string]
      if (firstOption.children?.length) {
        selectedKey.value = firstOption.children[0].key as string
      }
      else {
        selectedKey.value = firstOption.key as string
      }
      // 选中菜单后，手动触发一次连接
      if (!isConnected.value) {
        connect(selectedKey.value)
      }
    }
  }
  catch (error) {
    console.error('获取菜单失败:', error)
  }
})
</script>

<template>
  <div class="mt-[8px] pl-[74px] pr-[47px]">
    <CardHeader title="实时预览">
      <div class="flex gap-32px pt-[24px]" :style="{ height: containerHeight }">
        <!-- 左侧菜单区域 -->
        <div class="h-full w-[184px] overflow-x-hidden overflow-y-auto pr-2">
          <n-menu
            ref="menuInstRef"
            v-model:value="selectedKey"
            v-model:expanded-keys="expandedKeys"
            :options="options"
            :accordion="true"
            :root-indent="32"
            :indent="16"
          />
        </div>
        <!-- 右侧预览区域 -->
        <div class="h-full flex flex-1 flex-col pb-[40px]">
          <div class="h-full overflow-x-hidden overflow-y-auto">
            <template v-if="hasImage">
              <img class="h-full w-full object-contain object-left-top" :src="displayImage" alt="实时预览画面">
            </template>
            <div v-else class="h-full flex flex-col items-center justify-center gap-12px text-16px color-[rgba(255,255,255,0.65)] tracking-[0.5px]">
              <n-spin size="large" />
              <p>{{ pendingHolder }}</p>
              <n-button v-if="abnormalDetected" size="small" secondary @click="retryCurrent">
                重新连接
              </n-button>
            </div>
          </div>
        </div>
      </div>
    </CardHeader>
  </div>

  <!-- 调试信息组件 -->
  <!-- <ScaleDebugInfo /> -->
</template>

<style lang="scss" scoped>
.preview-panel__placeholder {
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  color: rgba(255, 255, 255, 0.65);
  font-size: 16px;
  letter-spacing: 0.5px;
}
</style>
