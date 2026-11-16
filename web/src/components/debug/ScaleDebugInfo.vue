<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useResponsiveHeight } from '@/hooks/useResponsiveHeight'

const screenResolution = ref('0 x 0')
const viewportSize = ref('0 x 0')
const appSize = ref('0 x 0')
const appPosition = ref('0, 0')
const actualTransform = ref('none')
const scaleInfo = ref('1.0')
const designRatio = ref('16:9')
const screenRatio = ref('16:9')
const theoreticalScaleX = ref('1.0')
const theoreticalScaleY = ref('1.0')
const autofitShouldScale = ref('1.0')
const debugNote = ref('')
const adaptationCase = ref('')

// 使用我们的响应式高度hook
const { height: containerHeight, scale } = useResponsiveHeight(124)

function updateDebugInfo() {
  // 屏幕分辨率
  screenResolution.value = `${window.screen.width} x ${window.screen.height}`

  // 视口尺寸
  viewportSize.value = `${window.innerWidth} x ${window.innerHeight}`

  // 计算屏幕比例
  const screenW = window.innerWidth
  const screenH = window.innerHeight
  const screenGCD = gcd(screenW, screenH)
  screenRatio.value = `${screenW / screenGCD}:${screenH / screenGCD}`

  // 设计稿比例
  const designGCD = gcd(1920, 1080)
  designRatio.value = `${1920 / designGCD}:${1080 / designGCD}`

  // 理论缩放比例
  const scaleX = screenW / 1920
  const scaleY = screenH / 1080
  theoreticalScaleX.value = scaleX.toFixed(3)
  theoreticalScaleY.value = scaleY.toFixed(3)

  // autofit.js 应该应用的缩放
  const shouldScale = Math.min(scaleX, scaleY, 1)
  autofitShouldScale.value = shouldScale.toFixed(3)

  // 调试说明
  if (screenW > 1920 && screenH > 1080) {
    debugNote.value = '屏幕大于设计稿，autofit可能不缩放'
  }
  else if (screenW < 1920 || screenH < 1080) {
    debugNote.value = '屏幕小于设计稿，应该缩放'
  }
  else {
    debugNote.value = '屏幕等于设计稿尺寸'
  }

  // App元素信息
  const appEl = document.getElementById('app')
  if (appEl) {
    const rect = appEl.getBoundingClientRect()
    const computedStyle = window.getComputedStyle(appEl)

    appSize.value = `${Math.round(rect.width)} x ${Math.round(rect.height)}`
    appPosition.value = `${Math.round(rect.left)}, ${Math.round(rect.top)}`
    actualTransform.value = computedStyle.transform || 'none'

    // 分析实际的缩放比例和适配情况
    if (computedStyle.transform && computedStyle.transform !== 'none') {
      const matrix = computedStyle.transform.match(/matrix\(([^)]+)\)/)
      if (matrix) {
        const values = matrix[1].split(',').map(v => Number.parseFloat(v.trim()))
        const actualScaleX = values[0]
        const actualScaleY = values[3]
        scaleInfo.value = `X:${actualScaleX.toFixed(3)} Y:${actualScaleY.toFixed(3)}`
        adaptationCase.value = 'Transform缩放'
      }
    }
    else {
      scaleInfo.value = 'no transform'

      // 检查是否有其他形式的适配
      if (Math.abs(rect.height - screenH) > 10) {
        adaptationCase.value = '元素高度适配'
      }
      else if (rect.top > 5 || rect.left > 5) {
        adaptationCase.value = '元素定位适配'
      }
      else {
        adaptationCase.value = '标准布局'
      }
    }
  }
}

function gcd(a: number, b: number): number {
  return b === 0 ? a : gcd(b, a % b)
}

const debouncedUpdate = debounce(updateDebugInfo, 100)

function debounce(func: (...args: any[]) => void, wait: number) {
  let timeout: NodeJS.Timeout
  return function executedFunction(...args: any[]) {
    const later = () => {
      clearTimeout(timeout)
      func(...args)
    }
    clearTimeout(timeout)
    timeout = setTimeout(later, wait)
  }
}

onMounted(() => {
  updateDebugInfo()
  window.addEventListener('resize', debouncedUpdate)

  // 定期更新以捕获autofit的变化
  const interval = setInterval(updateDebugInfo, 1000)

  onUnmounted(() => {
    window.removeEventListener('resize', debouncedUpdate)
    clearInterval(interval)
  })
})
</script>

<template>
  <div class="fixed bottom-4 right-4 z-50 max-w-sm rounded-lg bg-black bg-opacity-75 p-4 text-sm text-white font-mono">
    <div class="mb-2 font-bold">
      🔍 缩放调试信息
    </div>
    <div>屏幕分辨率: {{ screenResolution }}</div>
    <div>视口尺寸: {{ viewportSize }}</div>
    <div>App元素尺寸: {{ appSize }}</div>
    <div>App元素位置: {{ appPosition }}</div>
    <div>实际Transform: {{ actualTransform }}</div>
    <div class="text-yellow-300">
      实际缩放: {{ scaleInfo }}
    </div>
    <div class="text-green-300">
      容器高度: {{ containerHeight }}
    </div>
    <div>设计稿比例: {{ designRatio }}</div>
    <div>屏幕比例: {{ screenRatio }}</div>
    <div class="mt-2 text-blue-300">
      <div>理论缩放X: {{ theoreticalScaleX }}</div>
      <div>理论缩放Y: {{ theoreticalScaleY }}</div>
      <div>autofit应该: {{ autofitShouldScale }}</div>
    </div>
    <div class="mt-2 text-xs text-gray-300">
      {{ debugNote }}
    </div>
    <div class="mt-1 text-xs text-orange-300">
      适配情况: {{ adaptationCase }}
    </div>
  </div>
</template>
