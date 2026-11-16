<script setup lang="ts">
import type { ImageRenderToolbarProps } from 'naive-ui'

interface Props {
  /** 图片地址 */
  src: string
  /** 图片描述 */
  alt?: string
  /** 容器类名 */
  class?: string
  /** 图片类名 */
  imgClass?: string
  /** 占位符文本 */
  placeholder?: string
  /** 占位符图标 */
  placeholderIcon?: string
  /** 是否显示加载状态 */
  loading?: boolean
  /** 图片适应方式 */
  fit?: 'cover' | 'contain' | 'fill' | 'scale-down' | 'none' | 'auto-width'
  /** 占位符宽度 (仅在 fit='auto-width' 时生效) */
  placeholderWidth?: string
  /** 错误回调 */
  onError?: (event: Event) => void
  /** 加载完成回调 */
  onLoad?: (event: Event) => void
  /** 禁止预览 */
  previewDisabled?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  alt: '',
  class: '',
  imgClass: '',
  placeholder: '图片加载失败',
  placeholderIcon: '📷',
  loading: false,
  fit: 'cover',
  placeholderWidth: '300px',
  previewDisabled: false,
})

const emit = defineEmits<{
  error: [event: Event]
  load: [event: Event]
}>()

const isLoading = ref(props.loading)
const hasError = ref(false)
const imageId = ref(`smart-image-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`)

// 根据fit属性计算图片样式类
const imageClasses = computed(() => {
  const baseClasses = ['transition-opacity', 'duration-300', 'justify-center']

  if (props.fit === 'auto-width') {
    baseClasses.push('h-full', 'w-auto', 'object-contain')
  }
  else {
    baseClasses.push('h-full', 'w-full', `object-${props.fit}`)
  }

  if (hasError.value) {
    baseClasses.push('opacity-0')
  }

  return baseClasses
})

// 计算占位符样式和类名
const placeholderClasses = computed(() => {
  if (props.fit === 'auto-width') {
    return 'flex items-center justify-center from-[#1a2332] to-[#0f1419] bg-gradient-to-br'
  }
  return 'absolute inset-0 flex items-center justify-center from-[#1a2332] to-[#0f1419] bg-gradient-to-br'
})

const placeholderStyle = computed(() => {
  if (props.fit === 'auto-width') {
    return {
      width: props.placeholderWidth,
      height: '100%',
    }
  }
  return {}
})

function handleImageLoad(event: Event) {
  isLoading.value = false
  hasError.value = false
  emit('load', event)
  props.onLoad?.(event)
}

function handleImageError(event: Event) {
  isLoading.value = false
  hasError.value = true

  // 隐藏原图片
  const img = event.target as HTMLImageElement
  img.style.display = 'none'

  emit('error', event)
  props.onError?.(event)
}

function renderToolbar({ nodes }: ImageRenderToolbarProps) {
  return [
    nodes.rotateCounterclockwise,
    nodes.rotateClockwise,
    nodes.resizeToOriginalSize,
    nodes.zoomOut,
    nodes.zoomIn,
    nodes.close,
  ]
}
// 重置状态，当src变化时
watch(() => props.src, () => {
  isLoading.value = true
  hasError.value = false
}, { immediate: true })
</script>

<template>
  <div class="smart-image-container relative overflow-hidden" :class="[props.class]">
    <!-- 原始图片 -->
    <n-image
      :id="imageId"
      :class="[...imageClasses, props.imgClass]"
      :src="props.src"
      :alt="props.alt"
      :render-toolbar="renderToolbar"
      :show-toolbar-tooltip="true"
      :preview-disabled="props.previewDisabled"
      @load="handleImageLoad"
      @error="handleImageError"
    />

    <!-- 加载状态 -->
    <div
      v-if="isLoading && !hasError"
      :class="placeholderClasses"
      :style="placeholderStyle"
    >
      <div class="text-center text-[#666]">
        <div class="mb-2 animate-pulse text-[24px]">
          ⏳
        </div>
        <div class="text-[12px]">
          加载中...
        </div>
      </div>
    </div>

    <!-- 错误占位符 -->
    <div
      v-if="hasError"
      :class="placeholderClasses"
      :style="placeholderStyle"
    >
      <div class="text-center text-[#666]">
        <div class="mb-2 text-[24px]">
          {{ props.placeholderIcon }}
        </div>
        <div class="text-[12px]">
          {{ props.placeholder }}
        </div>
      </div>
    </div>

    <!-- 默认插槽，用于覆盖内容 -->
    <slot />
  </div>
</template>

<style scoped>
/* 图片样式由计算属性动态控制，这里只保留必要的容器样式 */
</style>
