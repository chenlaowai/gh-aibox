<script setup lang="ts">
interface Props {
  title: string
  size?: 'default' | 'large'
}
const props = withDefaults(defineProps<Props>(), {
  title: '默认标题',
  size: 'default',
})
</script>

<template>
  <div class="drawer-header-container" :class="[props.size]">
    <div class="flex justify-between">
      <div class="relative h-[37px] w-full flex items-center justify-center">
        <div class="relative z-[2] h-full w-full flex items-center justify-between">
          <img class="h-[14px] w-[23px]" src="@/assets/images/drawer_header_start.png" alt="start-icon">
          <span class="ys ml-[13px] flex-1 text-[18px] leading-[23px]">{{ props.title }}</span>
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
.drawer-header-container {
  width: 100%;
  position: relative;
  margin-bottom: 15px;
}

.drawer-header-container::before {
  content: '';
  position: absolute;
  top: 42px;
  left: 18px;
  width: 67px;
  height: 6px;
  background: #3a61ffff;
  clip-path: polygon(0 0, calc(100% - 3px) 0, 100% 100%, 0 100%);
}

.drawer-header-container.large::before {
  width: 97px;
}

.drawer-header-container::after {
  content: '';
  position: absolute;
  top: 48px;
  left: 18px;
  width: 100%;
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
