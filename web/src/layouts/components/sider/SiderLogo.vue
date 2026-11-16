<script setup lang="ts">
import { useAppStore } from '@/store'
import Logo from '@/assets/images/logo.png'

const router = useRouter()
const appStore = useAppStore()

const name = import.meta.env.VITE_APP_NAME
</script>

<template>
  <div
    class="h-60px text-xl flex-center cursor-pointer gap-2 p-x-2"
    @click="router.push('/')"
  >
    <img :src="Logo" alt="logo" class="h-30px">
    <span
      v-show="!appStore.collapsed"
      class="text-ellipsis overflow-hidden whitespace-nowrap font-size-[16px]"
    >{{ name }}</span>
  </div>
</template>

<style scoped></style>
