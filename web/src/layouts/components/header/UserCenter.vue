<script setup lang="ts">
import { useAuthStore } from '@/store'
import IconLogout from '~icons/icon-park-outline/logout'

const { userInfo, logout } = useAuthStore()

const options = computed(() => {
  return [
    // {
    //   label: t('app.userCenter'),
    //   key: 'userCenter',
    //   icon: () => h(IconUser),
    // },
    // {
    //   type: 'divider',
    //   key: 'd1',
    // },
    // {
    //   label: 'Github',
    //   key: 'guthub',
    //   icon: () => h(IconGithub),
    // },
    // {
    //   label: 'Gitee',
    //   key: 'gitee',
    //   icon: renderIcon('simple-icons:gitee'),
    // },
    // {
    //   label: 'Docs',
    //   key: 'docs',
    //   icon: () => h(IconBookOpen),
    // },
    // {
    //   type: 'divider',
    //   key: 'd1',
    // },
    {
      label: '退出登录',
      key: 'loginOut',
      icon: () => h(IconLogout),
    },
  ]
})
function handleSelect(key: string | number) {
  if (key === 'loginOut') {
    window.$dialog?.info({
      title: '退出登录',
      content: '确认退出当前账号？',
      positiveText: '确认',
      negativeText: '取消',
      onPositiveClick: () => {
        logout()
      },
    })
  }
}
</script>

<template>
  <n-dropdown
    trigger="click"
    :options="options"
    @select="handleSelect"
  >
    <n-avatar
      round

      :src="userInfo?.avatar"
    >
      <template #fallback>
        <div class="wh-full flex-center">
          <icon-park-outline-user />
        </div>
      </template>
    </n-avatar>
  </n-dropdown>
</template>

<style scoped></style>
