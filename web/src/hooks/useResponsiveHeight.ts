import { computed, onMounted, onUnmounted, ref } from 'vue'

/**
 * 响应式高度计算hook - 适配autofit缩放环境
 * @param fixedHeight 需要减去的固定高度（设计稿尺寸）
 * @param _designHeight 设计稿高度，默认1080
 * @returns 响应式的高度值
 */
export function useResponsiveHeight(fixedHeight: number = 124, _designHeight: number = 1080) {
  const height = ref('100vh')
  const scale = ref(1)

  /**
   * 计算当前缩放比例和响应式高度
   */
  function updateHeight() {
    const appEl = document.getElementById('app')
    if (!appEl) {
      // 如果没有找到app元素，使用传统的calc方式
      height.value = `calc(100vh - ${fixedHeight}px)`
      return
    }

    // 获取视口尺寸
    const viewportHeight = window.innerHeight

    // 获取App元素的实际状态
    const rect = appEl.getBoundingClientRect()
    const computedStyle = window.getComputedStyle(appEl)

    // 检查是否有实际的transform
    let actualScale = 1
    let hasTransform = false

    if (computedStyle.transform && computedStyle.transform !== 'none') {
      const matrix = computedStyle.transform.match(/matrix\(([^)]+)\)/)
      if (matrix) {
        const values = matrix[1].split(',').map(v => Number.parseFloat(v.trim()))
        actualScale = Math.min(values[0], values[3]) // 取X和Y缩放的较小值
        hasTransform = true
      }
    }

    scale.value = actualScale

    // 情况1: 有实际的transform缩放
    if (hasTransform && Math.abs(actualScale - 1) > 0.01) {
      // 在缩放环境下，内部像素空间被拉伸
      const scaledViewportHeight = viewportHeight / actualScale
      const availableHeight = scaledViewportHeight - fixedHeight
      height.value = `${Math.max(0, availableHeight)}px`
      return
    }

    // 情况2: 没有缩放transform，但可能有其他定位
    const appHeight = rect.height

    // 如果App元素的高度不等于视口高度，说明可能有特殊布局
    if (Math.abs(appHeight - viewportHeight) > 10) {
      // 使用App元素的实际可用高度
      const availableHeight = appHeight - fixedHeight
      height.value = `${Math.max(0, availableHeight)}px`
      return
    }

    // 情况3: 标准情况，使用传统calc方式
    height.value = `calc(100vh - ${fixedHeight}px)`
  }

  /**
   * 防抖函数
   */
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

  const debouncedUpdate = debounce(updateHeight, 100)

  onMounted(() => {
    // 初始计算
    updateHeight()

    // 监听窗口大小变化
    window.addEventListener('resize', debouncedUpdate)

    // 监听DOM变化（autofit可能会异步更新）
    const observer = new MutationObserver(() => {
      debouncedUpdate()
    })

    const appEl = document.getElementById('app')
    if (appEl) {
      observer.observe(appEl, {
        attributes: true,
        attributeFilter: ['style'],
      })
    }

    // 延迟执行一次，确保autofit已经完成初始化
    setTimeout(updateHeight, 100)
  })

  onUnmounted(() => {
    window.removeEventListener('resize', debouncedUpdate)
  })

  return {
    height: computed(() => height.value),
    scale: computed(() => scale.value),
    updateHeight,
  }
}

/**
 * 获取适配后的尺寸值
 * @param baseValue 基础值（设计稿尺寸）
 * @param type 尺寸类型
 * @param designWidth 设计稿宽度
 * @param designHeight 设计稿高度
 */
export function useResponsiveSize(
  baseValue: number,
  type: 'width' | 'height' = 'width',
  designWidth: number = 1920,
  designHeight: number = 1080,
) {
  const size = ref(baseValue)

  function updateSize() {
    const appEl = document.getElementById('app')
    if (!appEl) {
      size.value = baseValue
      return
    }

    const rect = appEl.getBoundingClientRect()
    const scale = type === 'width'
      ? rect.width / designWidth
      : rect.height / designHeight

    size.value = baseValue * scale
  }

  const debouncedUpdate = debounce(updateSize, 100)

  onMounted(() => {
    updateSize()
    window.addEventListener('resize', debouncedUpdate)
    setTimeout(updateSize, 100)
  })

  onUnmounted(() => {
    window.removeEventListener('resize', debouncedUpdate)
  })

  return {
    size: computed(() => size.value),
    updateSize,
  }
}

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
