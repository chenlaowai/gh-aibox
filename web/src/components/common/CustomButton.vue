<script setup lang="ts">
import { NButton } from 'naive-ui'

export interface CustomButtonProps {
  /** 按钮类型 */
  type?: 'info' | 'warn' | 'danger' | 'success'
  /** 是否禁用 */
  disabled?: boolean
  /** 是否加载中 */
  loading?: boolean
  /** 按钮尺寸 */
  size?: 'tiny' | 'small' | 'medium' | 'large'
}

export interface CustomButtonEmits {
  (e: 'click', event: MouseEvent): void
}

const props = withDefaults(defineProps<CustomButtonProps>(), {
  type: 'info',
  disabled: false,
  loading: false,
  size: 'medium',
})

const emit = defineEmits<CustomButtonEmits>()

function handleClick(event: MouseEvent) {
  if (!props.disabled && !props.loading) {
    emit('click', event)
  }
}
</script>

<template>
  <NButton
    class="custom-button" :class="[
      `custom-button--${type}`,
      { 'custom-button--disabled': disabled },
    ]"
    v-bind="$attrs"
    :disabled="disabled"
    :loading="loading"
    :size="size"
    @click="handleClick"
  >
    <slot />
  </NButton>
</template>

<style scoped>
.custom-button {
  position: relative;
  border: 2px solid transparent;
  border-radius: 4px;
  transition: all 0.3s ease;
  background: transparent;
  color: #fff !important;
}

.custom-button:deep(.n-button__content) {
  font-weight: 500;
}

/* Info 类型样式 */
.custom-button--info {
  background: rgba(5, 19, 49, 0.3) !important;
  border-image: radial-gradient(
      circle,
      rgba(0, 140.25000303983688, 255, 1),
      rgba(25.000000409781933, 138.00000697374344, 255, 0),
      rgba(191.8141633272171, 204.96050029993057, 255, 0.09466663002967834),
      rgba(120.73008567094803, 144.99573558568954, 255, 0.10000000149011612),
      rgba(0, 123.24999645352364, 255, 0),
      rgba(6.769911479204893, 126.74777716398239, 255, 0.6000000238418579)
    )
    1 1;
  color: #9aa5beff !important;
}

/* Warn 类型样式 */
.custom-button--warn {
  background: linear-gradient(180deg, rgba(23, 203, 62, 0) 0%, rgba(2, 131, 251, 0.102) 100%);
  border-image: radial-gradient(
      circle,
      rgba(245.00000059604645, 119.00000050663948, 1.0000000591389835, 1),
      rgba(245.00000059604645, 119.00000050663948, 1.0000000591389835, 0.10000000149011612),
      rgba(245.00000059604645, 119.00000050663948, 1.0000000591389835, 0.10000000149011612),
      rgba(245.00000059604645, 119.00000050663948, 1.0000000591389835, 0.30000001192092896),
      rgba(245.00000059604645, 119.00000050663948, 1.0000000591389835, 1),
      rgba(245.00000059604645, 119.00000050663948, 1.0000000591389835, 1)
    )
    1 1;
}

/* Danger 类型样式 */
.custom-button--danger {
  background: linear-gradient(180deg, rgba(255, 255, 0, 0) 0%, rgba(255, 0, 1, 0.102) 100%);
  border-image: radial-gradient(
      circle,
      rgba(245.00000059604645, 1.0000000591389835, 2.000000118277967, 1),
      rgba(245.00000059604645, 1.0000000591389835, 2.000000118277967, 0.10196078568696976),
      rgba(245.00000059604645, 1.0000000591389835, 2.000000118277967, 0.10196078568696976),
      rgba(245.00000059604645, 1.0000000591389835, 2.000000118277967, 1),
      rgba(245.00000059604645, 1.0000000591389835, 2.000000118277967, 1),
      rgba(245.00000059604645, 1.0000000591389835, 2.000000118277967, 1)
    )
    1 1;
}

/* Success 类型样式 */
.custom-button--success {
  background: linear-gradient(180deg, rgba(255, 255, 0, 0) 0%, rgba(255, 255, 0, 0.2) 100%) !important;
  border-image: radial-gradient(
      circle,
      rgba(247.00000047683716, 247.00000047683716, 0, 1),
      rgba(255, 253.65247070789337, 19.18141633272171, 0.10000000149011612),
      rgba(255, 255, 172.6327469944954, 0.10000000149011612),
      rgba(254.00000005960464, 255, 0, 1),
      rgba(254.00000005960464, 255, 0, 1),
      rgba(255, 255, 0, 1)
    )
    1 1;
}

/* 禁用状态 */
.custom-button--disabled {
  opacity: 0.5;
  cursor: not-allowed;
  transform: none !important;
  box-shadow: none !important;
}

.custom-button--disabled:hover {
  background: transparent !important;
  transform: none !important;
  box-shadow: none !important;
}

/* 尺寸调整 */
.custom-button:deep(.n-button) {
  min-height: var(--custom-button-height);
  padding: var(--custom-button-padding);
  font-size: var(--custom-button-font-size);
}

/* 不同尺寸的CSS变量 */
.custom-button {
  --custom-button-height: 32px;
  --custom-button-padding: 0 15px;
  --custom-button-font-size: 14px;
}

.custom-button:deep(.n-button--tiny) {
  --custom-button-height: 22px;
  --custom-button-padding: 0 8px;
  --custom-button-font-size: 12px;
}

.custom-button:deep(.n-button--small) {
  --custom-button-height: 28px;
  --custom-button-padding: 0 12px;
  --custom-button-font-size: 13px;
}

.custom-button:deep(.n-button--large) {
  --custom-button-height: 40px;
  --custom-button-padding: 0 20px;
  --custom-button-font-size: 16px;
}
</style>
