<script setup lang="ts">
import { computed } from 'vue'
import { NTag } from 'naive-ui'
import { useAppStore } from '@/store'

interface Props {
  type?: 'primary' | 'success' | 'warning' | 'error' | 'info' | 'default'
  size?: 'small' | 'medium' | 'large'
  round?: boolean
  closable?: boolean
}

const props = withDefaults(defineProps<Props>(), {
  type: 'default',
  size: 'medium',
  round: false,
  closable: false,
})

const emit = defineEmits<{
  close: []
}>()

const appStore = useAppStore()

// 从主题配置中获取颜色，作为备用方案
function getThemeColor(colorKey: string, fallback: string) {
  return appStore.theme?.common?.[colorKey] || fallback
}

// 定义颜色配置，可以从主题中动态获取
const colorSchemes = computed(() => ({
  primary: {
    color: 'rgba(32, 128, 240, 0.2)',
    textColor: getThemeColor('primaryColor', '#2080f0'),
    borderColor: 'rgba(32, 128, 240, 0.3)',
  },
  success: {
    color: 'rgba(24, 160, 88, 0.2)',
    textColor: getThemeColor('successColor', '#18a058'),
    borderColor: 'rgba(24, 160, 88, 0.3)',
  },
  warning: {
    color: 'rgba(240, 160, 32, 0.2)',
    textColor: getThemeColor('warningColor', '#f0a020'),
    borderColor: 'rgba(240, 160, 32, 0.3)',
  },
  error: {
    color: 'rgba(208, 48, 80, 0.2)',
    textColor: getThemeColor('errorColor', '#d03050'),
    borderColor: 'rgba(208, 48, 80, 0.3)',
  },
  info: {
    color: 'rgba(32, 128, 240, 0.2)',
    textColor: getThemeColor('infoColor', '#2080f0'),
    borderColor: 'rgba(32, 128, 240, 0.3)',
  },
  default: {
    color: 'rgba(255, 255, 255, 0.2)',
    textColor: 'rgba(255, 255, 255, 0.7)',
    borderColor: 'rgba(255, 255, 255, 0.2)',
  },
}))

// 计算当前类型的颜色方案
const currentColorScheme = computed(() => {
  return colorSchemes.value[props.type] || colorSchemes.value.default
})

function handleClose() {
  emit('close')
}
</script>

<template>
  <NTag
    :size="size"
    :round="round"
    :closable="closable"
    :color="{
      color: currentColorScheme.color,
      textColor: currentColorScheme.textColor,
      borderColor: `1px solid ${currentColorScheme.borderColor}`,
    }"
    @close="handleClose"
  >
    <slot />
  </NTag>
</template>
