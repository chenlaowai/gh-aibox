<script setup lang="ts">
interface Props {
  /** 当前值 */
  value: number
  /** 是否处于编辑状态 */
  editing?: boolean
  /** 最小值 */
  min?: number
  /** 最大值 */
  max?: number
  /** 步长 */
  step?: number
  /** 精度（小数位数） */
  precision?: number
  /** 输入框宽度 */
  width?: string
  /** 组件尺寸 */
  size?: 'small' | 'medium' | 'large'
  /** 是否禁用 */
  disabled?: boolean
  /** 显示文本的自定义类名 */
  textClass?: string
}

interface Emits {
  /** 值变更事件 */
  (e: 'update:value', value: number): void
  /** 编辑状态变更事件 */
  (e: 'update:editing', editing: boolean): void
  /** 保存事件 */
  (e: 'save', value: number): void
}

const props = withDefaults(defineProps<Props>(), {
  editing: false,
  min: 0,
  max: 100,
  step: 0.1,
  precision: 1,
  width: '80px',
  size: 'small',
  disabled: false,
  textClass: 'min-w-[40px] text-white font-medium',
})

const emit = defineEmits<Emits>()

// 更新数值
function updateValue(increment: number) {
  const newValue = Number((props.value + increment).toFixed(props.precision))
  const clampedValue = Math.max(props.min, Math.min(props.max, newValue))
  emit('update:value', clampedValue)
}

// 直接设置数值
function setValue(value: number | null) {
  if (value === null)
    return
  const clampedValue = Math.max(props.min, Math.min(props.max, value))
  emit('update:value', clampedValue)
}

// 切换编辑状态
function toggleEdit() {
  if (props.editing) {
    // 保存
    emit('save', props.value)
  }
  emit('update:editing', !props.editing)
}
</script>

<template>
  <div class="flex items-center gap-2">
    <template v-if="editing">
      <!-- 编辑状态：减号 + 数值输入 + 加号 + 保存按钮 -->
      <n-button
        :size="size"
        type="primary"
        quaternary
        circle
        :disabled="disabled || value <= min"
        @click="updateValue(-step)"
      >
        -
      </n-button>

      <n-input-number
        :value="value"
        :step="step"
        :precision="precision"
        :min="min"
        :max="max"
        :style="{ width }"
        :size="size"
        :show-button="false"
        :disabled="disabled"
        @update:value="setValue"
      />

      <n-button
        :size="size"
        type="primary"
        quaternary
        circle
        :disabled="disabled || value >= max"
        @click="updateValue(step)"
      >
        +
      </n-button>

      <n-button
        :size="size"
        type="warning"
        quaternary
        :disabled="disabled"
        @click="toggleEdit"
      >
        保存
      </n-button>
    </template>

    <template v-else>
      <!-- 默认状态：文本展示 + 编辑按钮 -->
      <span :class="textClass">{{ value }}</span>
      <n-button
        :size="size"
        type="primary"
        quaternary
        :disabled="disabled"
        @click="toggleEdit"
      >
        编辑
      </n-button>
    </template>
  </div>
</template>

<style scoped>
/* 可以添加自定义样式 */
</style>
