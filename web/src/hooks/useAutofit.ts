import { nextTick, onMounted, onUnmounted, ref } from 'vue'

/**
 * useAutofitDrawer - 处理抽屉组件在autofit环境下的缩放适配
 * @param baseWidth 基础宽度（设计稿中的抽屉宽度）
 * @param designWidth 设计稿宽度，默认1920
 * @param minWidth 最小宽度限制，默认300
 * @param maxWidth 最大宽度限制，默认800
 * @returns 响应式的抽屉宽度和更新方法
 */
export function useAutofitDrawer(
  baseWidth: number = 730,
  designWidth: number = 1920,
  minWidth: number = 300,
  maxWidth: number = 800,
) {
  const drawerWidth = ref(baseWidth)

  /**
   * 更新抽屉宽度以适应当前缩放比例
   */
  function updateDrawerWidth() {
    const appEl = document.getElementById('app')
    if (!appEl) {
      drawerWidth.value = baseWidth
      return
    }

    const rect = appEl.getBoundingClientRect()
    const scaleX = rect.width / designWidth // 基于设计稿宽度计算缩放比例

    // 根据缩放比例调整抽屉宽度，确保视觉效果一致
    const calculatedWidth = baseWidth * scaleX
    drawerWidth.value = Math.max(minWidth, Math.min(maxWidth, calculatedWidth))
  }

  /**
   * 当抽屉显示时更新宽度
   * @param visible 抽屉显示状态
   */
  function onDrawerVisible(visible: boolean) {
    if (visible) {
      nextTick(() => {
        updateDrawerWidth()
      })
    }
  }

  // 组件挂载时初始化
  onMounted(() => {
    updateDrawerWidth()
    window.addEventListener('resize', updateDrawerWidth)
  })

  // 组件卸载时清理事件监听
  onUnmounted(() => {
    window.removeEventListener('resize', updateDrawerWidth)
  })

  return {
    drawerWidth: readonly(drawerWidth),
    updateDrawerWidth,
    onDrawerVisible,
  }
}

/**
 * useAutofitModal - 处理模态框组件在autofit环境下的缩放适配
 * @param baseWidth 基础宽度
 * @param baseHeight 基础高度（可选）
 * @param designWidth 设计稿宽度，默认1920
 * @param designHeight 设计稿高度，默认1080
 */
export function useAutofitModal(
  baseWidth: number,
  baseHeight?: number,
  designWidth: number = 1920,
  designHeight: number = 1080,
) {
  const modalWidth = ref(baseWidth)
  const modalHeight = ref(baseHeight)

  function updateModalSize() {
    const appEl = document.getElementById('app')
    if (!appEl) {
      modalWidth.value = baseWidth
      if (baseHeight)
        modalHeight.value = baseHeight
      return
    }

    const rect = appEl.getBoundingClientRect()
    const scaleX = rect.width / designWidth
    const scaleY = rect.height / designHeight

    modalWidth.value = baseWidth * scaleX
    if (baseHeight) {
      modalHeight.value = baseHeight * scaleY
    }
  }

  function onModalVisible(visible: boolean) {
    if (visible) {
      nextTick(() => {
        updateModalSize()
      })
    }
  }

  onMounted(() => {
    updateModalSize()
    window.addEventListener('resize', updateModalSize)
  })

  onUnmounted(() => {
    window.removeEventListener('resize', updateModalSize)
  })

  return {
    modalWidth: readonly(modalWidth),
    modalHeight: baseHeight ? readonly(modalHeight) : undefined,
    updateModalSize,
    onModalVisible,
  }
}
