<script setup lang="ts">
import { NEmpty, NSelect, dateZhCN, zhCN } from 'naive-ui'
import autofit from 'autofit.js' // scale方案适配
import noData from '@/assets/images/no_data.png'

NEmpty.render = () => {
  return h('img', { src: noData })
}

NEmpty.render()

// 全局修改 n-select 的默认 virtual-scroll， 默认true使用sacle加载不出
NSelect.props.virtualScroll.default = false

function init() {
  autofit.init({
    dh: 1080, // 设计稿高度
    dw: 1920, // 设计稿宽度
    resize: true,
    transition: 0, // 关闭动画，避免闪烁
    el: '#app', // 指定缩放容器
    ignore: [
      {
        el: '.n-drawer .v-binder-follower-container:has(.n-select-menu)', // 必填(消除select偏移)
        height: '100vh',
        width: '100vw',
      },
      {
        el: '.center-view', // 忽略中央展示区的缩放
        height: '100vh',
        width: '100vw',
      },
      {
        el: '.video-bg', // 忽略背景视频的缩放
        height: '100vh',
        width: '100vw',
      },
    ],
  })
}

function compatiblePhoneView() {
  // 横竖屏切换时重新应用样式
  window.addEventListener('orientationchange', () => {
    document.documentElement.style.webkitTextSizeAdjust = '100%'
  })
  // 检测手机设备
  const isMobile = /Mobi|Android|iPhone|iPad|iPod/i.test(navigator.userAgent)
  const rotatePrompt: any = document.getElementById('rotatePrompt')
  console.log('isMobile', isMobile)

  if (isMobile) {
    // 初始检查方向
    checkOrientation()
    // 监听旋转事件
    window.addEventListener('orientationchange', checkOrientation)
  }

  function checkOrientation() {
    if (window.orientation === 0 || window.orientation === 180 || screen.orientation.angle === 0 || screen.orientation.angle === 180) {
      rotatePrompt.style.display = 'flex' // 显示提示
    }
    else {
      rotatePrompt.style.display = 'none' // 隐藏提示
    }
  }
}

onMounted(() => {
  init()
  compatiblePhoneView()
  document.documentElement.style.setProperty('-webkit-text-size-adjust', '100%')
  document.documentElement.style.setProperty('text-size-adjust', '100%')
})
</script>

<template>
  <Teleport to="body">
    <div id="rotatePrompt">
      请将手机横屏浏览
    </div>
  </Teleport>
  <n-config-provider
    :date-locale="dateZhCN" :locale="zhCN" style="width:100%;height: 100%"
  >
    <router-view />
  </n-config-provider>
</template>

<style scoped>
#rotatePrompt {
  display: none;
  position: fixed;
  top: 0px;
  bottom: 0px;
  left: 0px;
  right: 0px;
  background: #000;
  justify-content: center;
  align-items: center;
  color: white;
  font-size: 40px;
  z-index: 9999;
}
</style>
