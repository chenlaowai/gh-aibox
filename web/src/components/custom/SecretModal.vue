<script setup lang="tsx">
import { useBoolean } from '@/hooks'
import { useAuthStore } from '@/store'
import { local } from '@/utils'

const authStore = useAuthStore()
const { isLogin } = storeToRefs(authStore)
const { bool, setBool } = useBoolean(false)

// 检查是否已显示过机密信息
function hasShownSecret(): boolean {
  return local.get('secret_modal_shown') ?? false
}

// 标记已显示机密信息
function markSecretAsShown(): void {
  local.set('secret_modal_shown', true)
}

// 设置退出登录标识
function setLogoutFlag(): void {
  local.set('user_logout_flag', true)
}

// 清除退出登录标识
function clearLogoutFlag(): void {
  local.remove('user_logout_flag')
}

// 检查是否有退出登录标识
function hasLogoutFlag(): boolean {
  return local.get('user_logout_flag') ?? false
}

// 清除机密信息显示标记
function clearSecretShown(): void {
  local.remove('secret_modal_shown')
}

// 监听登录状态变化，只在每次登录时显示一次
watch(isLogin, (isLoggedIn, wasLoggedIn) => {
  // 从未登录变为已登录时显示机密信息
  if (isLoggedIn && !wasLoggedIn) {
    // 检查是否有退出登录标识，如果有则清除显示状态
    if (hasLogoutFlag()) {
      clearSecretShown()
      clearLogoutFlag()
    }
    // 如果未显示过机密信息，则显示
    if (!hasShownSecret()) {
      setBool(true)
      markSecretAsShown()
    }
  }
  // 登出时设置退出登录标识
  if (!isLoggedIn && wasLoggedIn) {
    setLogoutFlag()
  }
}, { immediate: true })
</script>

<template>
  <n-modal v-model:show="bool" :mask-closable="false">
    <div class="relative h-[400px] w-[678px] select-none outline-none">
      <img src="https://hnkancha.com/img/warning_sign/warning_sign.png" class="block h-auto w-full" alt="warning">
      <div
        class="absolute right-2 top-2 cursor-pointer p-2 text-white/60 transition-colors hover:text-white"
        @click="bool = false"
      >
        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <line x1="18" y1="6" x2="6" y2="18" />
          <line x1="6" y1="6" x2="18" y2="18" />
        </svg>
      </div>
    </div>
  </n-modal>
</template>
