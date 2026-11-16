import type { Directive, DirectiveBinding } from 'vue'

interface AutofitOptions {
  baseWidth?: number
  baseHeight?: number
  designWidth?: number
  designHeight?: number
  minWidth?: number
  maxWidth?: number
  minHeight?: number
  maxHeight?: number
  properties?: string[] // 要缩放的CSS属性，如 ['width', 'height', 'font-size']
}

interface ElementWithAutofit extends HTMLElement {
  _autofitConfig?: AutofitOptions
  _autofitResizeHandler?: () => void
}

/**
 * v-autofit指令 - 让元素自动适应autofit缩放
 *
 * 使用示例：
 * <div v-autofit="{ baseWidth: 300, baseHeight: 200 }">内容</div>
 * <div v-autofit:width="300">只缩放宽度</div>
 * <div v-autofit:font-size="16">缩放字体大小</div>
 */
export const autofit: Directive<ElementWithAutofit, AutofitOptions | number> = {
  mounted(el, binding) {
    setupAutofit(el, binding)
  },

  updated(el, binding) {
    if (JSON.stringify(binding.value) !== JSON.stringify(binding.oldValue)) {
      cleanupAutofit(el)
      setupAutofit(el, binding)
    }
  },

  unmounted(el) {
    cleanupAutofit(el)
  },
}

function setupAutofit(el: ElementWithAutofit, binding: DirectiveBinding<AutofitOptions | number>) {
  // 解析配置
  const config = parseConfig(binding)
  el._autofitConfig = config

  // 创建更新函数
  const updateElement = () => {
    const appEl = document.getElementById('app')
    if (!appEl)
      return

    const rect = appEl.getBoundingClientRect()
    const scaleX = rect.width / config.designWidth!
    const scaleY = rect.height / config.designHeight!

    // 根据配置更新相应的CSS属性
    if (config.baseWidth && config.properties?.includes('width')) {
      const newWidth = config.baseWidth * scaleX
      const clampedWidth = Math.max(
        config.minWidth || 0,
        Math.min(config.maxWidth || Infinity, newWidth),
      )
      el.style.width = `${clampedWidth}px`
    }

    if (config.baseHeight && config.properties?.includes('height')) {
      const newHeight = config.baseHeight * scaleY
      const clampedHeight = Math.max(
        config.minHeight || 0,
        Math.min(config.maxHeight || Infinity, newHeight),
      )
      el.style.height = `${clampedHeight}px`
    }

    // 处理字体大小缩放
    if (config.properties?.includes('font-size')) {
      const baseFontSize = Number.parseFloat(getComputedStyle(el).fontSize) || 16
      const newFontSize = baseFontSize * Math.min(scaleX, scaleY)
      el.style.fontSize = `${newFontSize}px`
    }

    // 处理其他指定的属性
    config.properties?.forEach((prop) => {
      if (!['width', 'height', 'font-size'].includes(prop)) {
        const baseValue = (config as any)[`base${prop.charAt(0).toUpperCase() + prop.slice(1)}`]
        if (baseValue) {
          const scale = prop.includes('width') || prop.includes('x') ? scaleX : scaleY
          el.style.setProperty(prop, `${baseValue * scale}px`)
        }
      }
    })
  }

  // 保存处理函数引用
  el._autofitResizeHandler = updateElement

  // 初始更新
  updateElement()

  // 监听窗口大小变化
  window.addEventListener('resize', updateElement)
}

function cleanupAutofit(el: ElementWithAutofit) {
  if (el._autofitResizeHandler) {
    window.removeEventListener('resize', el._autofitResizeHandler)
    delete el._autofitResizeHandler
  }
  delete el._autofitConfig
}

function parseConfig(binding: DirectiveBinding<AutofitOptions | number>): AutofitOptions {
  const defaultConfig: AutofitOptions = {
    designWidth: 1920,
    designHeight: 1080,
    properties: ['width', 'height'],
  }

  if (typeof binding.value === 'number') {
    // v-autofit:width="300" 形式
    const property = binding.arg || 'width'
    return {
      ...defaultConfig,
      [`base${property.charAt(0).toUpperCase() + property.slice(1)}`]: binding.value,
      properties: [property],
    }
  }

  if (typeof binding.value === 'object' && binding.value !== null) {
    // v-autofit="{ baseWidth: 300 }" 形式
    return {
      ...defaultConfig,
      ...binding.value,
    }
  }

  return defaultConfig
}

// 默认导出指令对象
export default autofit
