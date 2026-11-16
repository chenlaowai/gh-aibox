<script setup lang="ts">
import { useRoute } from 'vue-router'
import { createDiscreteApi } from 'naive-ui'

const { message } = createDiscreteApi(['message'])
const route = useRoute()
const state = (route.redirectedFrom?.query?.state || route.query?.state) as string

function toLogin() {
  const authStore = store.useAuthStore()
  if (authStore.state && state !== authStore.state) {
    return message.error('当前访问不安全!')
  }

  authStore.$patch({ state: undefined })
  const url = import.meta.env.VITE_APP_LOGIN_URL
  window.location.replace(url)
}

onMounted(() => {
  toLogin()
})
</script>

<template>
  <div />
</template>
