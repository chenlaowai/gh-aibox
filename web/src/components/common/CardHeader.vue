<script setup lang="ts">
interface Props {
  title: string
  clickable?: boolean
}
const props = withDefaults(defineProps<Props>(), {
  title: '默认标题',
  clickable: false,
})

const emit = defineEmits<{
  (e: 'headerClick'): void
}>()

function onHeaderClick() {
  if (props.clickable) {
    emit('headerClick')
  }
}
</script>

<template>
  <div class="card-header-container">
    <div class="flex justify-between">
      <div
        class="relative h-[37px] w-[344px] flex items-center justify-center"
        :class="{ 'cursor-pointer': props.clickable }"
        @click="onHeaderClick"
      >
        <img
          class="absolute left-0 top-0 z-[1] h-full w-full object-cover"
          src="@/assets/images/card_header_bg.png" alt="header-bg"
        >
        <div class="relative z-[2] h-full w-full flex items-center justify-between">
          <img class="ml-[31px] h-[14px] w-[23px]" src="@/assets/images/card_header_start.png" alt="start-icon">
          <span class="ys ml-[13px] flex-1 text-[18px]">{{ props.title }}</span>
          <img class="mr-[79px] h-[22px] w-[17px]" src="@/assets/images/card_header_end.png" alt="end-icon">
        </div>
      </div>
      <div>
        <slot name="header-extra" />
      </div>
    </div>
    <slot />
  </div>
</template>

<style scoped lang="scss">
.card-header-container {
  width: 100%;
  position: relative;
  margin-bottom: 15px;
}

.card-header-container::after {
  content: '';
  position: absolute;
  top: 37px;
  left: 0px;
  width: 47px;
  height: 1px;
  background: #3a61ffff;
}

::v-deep(.header-title) {
  color: #00f7ff;
  font-size: 14px;
  font-weight: bold;
  white-space: nowrap;
}
</style>
