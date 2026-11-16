<script setup lang="ts">
import { useRouter } from 'vue-router'
import { darkTheme } from 'naive-ui'
import { biTheme } from '@/store/app/biTheme'
import { useAppStore, useAuthStore } from '@/store'

const router = useRouter()
const route = useRoute()
const appStore = useAppStore()
const authStore = useAuthStore()

// 导航函数
function navigateTo(path: string) {
  router.push(`/bi${path}`)
}

// 匹配路由函数
function matchRoute(path: string) {
  const currentPath = route.path
  const targetPath = `/bi${path}`

  // 首先检查是否直接匹配
  if (currentPath === targetPath) {
    return true
  }

  // 检查当前路由的meta.activeMenu是否指向目标路径
  if (route.meta?.activeMenu === targetPath) {
    return true
  }

  // 检查是否是子路由（以目标路径开头）
  if (currentPath.startsWith(`${targetPath}/`)) {
    return true
  }

  return false
}

// 退出登录
function handleLogout() {
  authStore.logout()
}

const isHome = computed(() => route.name === 'home')
</script>

<template>
  <n-config-provider :theme-overrides="{ ...biTheme, ...appStore.theme }" :theme="darkTheme" style="width:100%;height: 100%">
    <div class="ai-dashboard flex flex-col">
      <template v-if="isHome">
        <!-- 左侧阴影遮罩 -->
        <div class="side-shadow left-shadow" />
        <!-- 右侧阴影遮罩 -->
        <div class="side-shadow right-shadow" />
        <!-- 视频背景动效 -->
        <video class="video-bg" autoplay muted loop preload="auto" playsinline>
          <source src="@/assets/video/home_bg.mp4" type="video/mp4">
        </video>
      </template>

      <!-- 顶部导航栏 -->
      <header class="dashboard-header">
        <div class="header-content">
          <!-- 左侧功能模块 -->
          <div class="header-left">
            <div class="nav-item" :class="{ active: matchRoute('/home') }" @click="navigateTo('/home')">
              首页
            </div>
            <div class="nav-item" :class="{ active: matchRoute('/channel-management') }" @click="navigateTo('/channel-management')">
              通道管理
            </div>
            <div class="nav-item" :class="{ active: matchRoute('/task-management') }" @click="navigateTo('/task-management')">
              任务管理
            </div>
            <div class="nav-item" :class="{ active: matchRoute('/current-preview') }" @click="navigateTo('/current-preview')">
              实时预览
            </div>
          </div>

          <!-- 中央标题 -->
          <div class="header-center">
            <h1 class="main-title ys">
              广航AI盒子展示平台
            </h1>
          </div>

          <!-- 右侧功能区 -->
          <div class="header-right">
            <div class="nav-item" :class="{ active: matchRoute('/alarm-preview') }" @click="navigateTo('/alarm-preview')">
              告警预览
            </div>
            <div class="nav-item" :class="{ active: matchRoute('/parameter-config') }" @click="navigateTo('/parameter-config')">
              参数配置
            </div>
            <div class="nav-item" :class="{ active: matchRoute('/other-config') }" @click="navigateTo('/other-config')">
              其他配置
            </div>
            <n-popover trigger="hover" placement="bottom-end">
              <template #trigger>
                <div class="user-avatar cursor-pointer">
                  <img src="@/assets/images/user_avatar.png" alt="用户头像">
                  <span class="ys ml-8px text-[16px] color-[#7297F5FF]">{{ authStore.userInfo.name }}</span>
                </div>
              </template>
              <div class="user-menu">
                <div class="menu-item" @click="handleLogout">
                  <span>退出登录</span>
                </div>
              </div>
            </n-popover>
          </div>
        </div>
      </header>

      <!-- 主体内容区 -->
      <div class="flex-1">
        <router-view />
      </div>

      <!-- 底部装饰线 -->
      <footer class="dashboard-footer" />
    </div>
  </n-config-provider>
</template>

<style lang="scss" scoped>
.ai-dashboard {
  width: 100%;
  height: 100%;
  color: #fff;
  overflow: hidden;
  position: relative;
  background-color: #020927ff; /* 兜底背景色，防止视频加载时闪烁 */
}

/* 两侧阴影遮罩 */
.side-shadow {
  position: absolute;
  top: 0;
  width: 635px;
  height: 100%;
  z-index: 3;
  pointer-events: none;
}

/* 2K屏幕下调整侧边遮罩宽度 */
@media screen and (min-width: 2560px) {
  .side-shadow {
    width: 825px; /* 2K屏幕下增加遮罩宽度 */
  }
}
.left-shadow {
  left: 0;
  background: linear-gradient(270deg, rgba(0, 0, 0, 0) 4%, #060913 48%, #00092c 100%);
}
.right-shadow {
  right: 0;
  background: linear-gradient(90deg, rgba(0, 0, 0, 0) 4%, #060913 48%, #00092c 100%);
}

.video-bg {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  object-fit: cover;
  z-index: 1;
  background-color: #020927ff; /* 兜底背景色，视频加载期间显示 */
  /* 强制覆盖全局CSS限制 */
  max-width: none !important;
  max-height: none !important;
}

.dashboard-header {
  height: 80px;
  position: relative;
  z-index: 10;
  backdrop-filter: blur(10px);
  flex-shrink: 0;

  .header-content {
    display: flex;
    justify-content: center;
    height: 100%;
    padding: 0 85px;
  }

  .header-left,
  .header-right {
    display: flex;
    align-items: center;
    gap: 12px;
    flex: 1;
  }

  .nav-item {
    height: 30px;
    width: 125px;
    display: flex;
    justify-content: center;
    align-items: center;
    background: url('@/assets/images/menu_bg.png');
    cursor: pointer;
    font-size: 14px;
    line-height: 20px;

    &.active,
    &:hover {
      background: url('@/assets/images/menu_bg_active.png');
    }
  }

  .header-center {
    background: url('@/assets/images/header_bg.png');
    height: 72px;
    width: 660px;
    margin: 0 10px;
    display: flex;
    justify-content: center;
    align-items: center;
    flex-shrink: 0;

    .main-title {
      font-size: 42px;
      color: #fff;
      margin: 0;
    }
  }

  .user-avatar {
    display: flex;
    flex: 1;
    justify-content: flex-end;
    align-items: center;
    flex-shrink: 0;
  }
}

.user-menu {
  .menu-item {
    padding: 4px 8px;
    cursor: pointer;
    transition: background-color 0.2s ease;
    border-radius: 4px;

    &:hover {
      background-color: rgba(114, 151, 245, 0.2);
    }

    span {
      color: #fff;
      font-size: 12px;
    }
  }
}

.dashboard-footer {
  background: url('@/assets/images/footer_bg.png');
  background-position: center;
  background-repeat: no-repeat;
  height: 38px;
  width: 100%;
  z-index: 10;
  position: absolute;
  bottom: 0;
}
</style>
