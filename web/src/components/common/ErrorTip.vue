<script setup lang="ts">
const props = defineProps<{
  /** 异常类型 403 404 500 */
  type: '403' | '404' | '500'
}>()
const router = useRouter()

function handleBackHome() {
  const authStore = store.useAuthStore()
  if(props.type === '403') {
    authStore.clear()
  }
  router.replace(import.meta.env.VITE_BASE_URL)
}
</script>

<template>
  <div class="h-full flex-col-center">
    <img v-if="type === '403'" src="@/assets/svg/error-403.svg" alt="" class="w-1/3">
    <img v-if="type === '404'" src="@/assets/svg/error-404.svg" alt="" class="w-1/3">
    <img v-if="type === '500'" src="@/assets/svg/error-500.svg" alt="" class="w-1/3">
    <n-button type="primary" @click="handleBackHome">
      {{ type === '403' ? '切换账号' : '回到首页' }}
    </n-button>
  </div>
</template>
