<script setup lang="ts">
interface Props {
  visible: boolean
  content?: string
  subContent?: string
}

interface Emits {
  (e: 'update:visible', value: boolean): void
  (e: 'confirm'): void
}

const props = withDefaults(defineProps<Props>(), {
  visible: false,
  content: '确定要删除该记录?',
  subContent: '删除后无法恢复',
})

const emit = defineEmits<Emits>()

const visible = computed({
  get: () => props.visible,
  set: value => emit('update:visible', value),
})

function handleCancel() {
  visible.value = false
}

function handleConfirm() {
  emit('confirm')
  visible.value = false
}
</script>

<template>
  <n-modal v-if="visible" v-model:show="visible" display-directive="if" :mask-closable="false" :closable="true">
    <div class="relative mx-auto h-[330px] w-[600px]">
      <!-- 背景边框 -->
      <div class="absolute left-0 top-0 h-full w-full bg-[url('@/assets/images/channel-tips.png')] bg-cover bg-center" />
      <!-- 内容区域 -->
      <div class="relative z-2 h-full w-full flex flex-col px-82px">
        <!-- 主体内容 -->
        <div class="mt-127px flex flex-1 flex-col items-start justify-start gap-16px">
          <div class="text-24px text-white leading-[34px]">
            {{ props.content }}
          </div>
          <div class="text-18px text-[#9AA5BEFF] leading-[25px]">
            {{ props.subContent }}
          </div>
        </div>

        <!-- 按钮组 -->
        <div class="mb-50px flex justify-end gap-32px">
          <custom-button class="h-[40px] w-[110px]" type="info" @click="handleCancel">
            取消
          </custom-button>
          <custom-button class="h-[40px] w-[110px]" type="danger" @click="handleConfirm">
            确认
          </custom-button>
        </div>
      </div>
      <!-- 隐藏的关闭按钮 -->
      <div class="absolute right-0 top-0 z-3 h-[33px] w-[33px] cursor-pointer" @click="handleCancel" />
    </div>
  </n-modal>
</template>

<style scoped>
</style>
