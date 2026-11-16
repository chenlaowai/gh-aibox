<script setup lang="ts">
import { useAppStore } from '@/store'

const appStore = useAppStore()
</script>

<template>
  <n-tooltip placement="bottom" trigger="hover">
    <template #trigger>
      <CommonWrapper @click="appStore.showSetting = !appStore.showSetting">
        <div>
          <icon-park-outline-setting-two />
        </div>
      </CommonWrapper>
    </template>
    <span>系统设置</span>
  </n-tooltip>
</template>
