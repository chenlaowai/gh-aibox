<script setup lang="ts">
import Login from './components/Login.vue'
</script>

<template>
  <div class="login-wrap">
    <div class="login-header">
      <svg-icons-logo class="mr-8px h-44px w-44px" />
      中交华南勘察测绘科技有限公司
    </div>
    <!-- 视频背景 -->
    <video class="video-bg" autoplay muted loop>
      <source src="@/assets/video/login_bg.mp4" type="video/mp4">
    </video>
    <div class="mask-left" />
    <div class="mask-top" />
    <!-- 登录容器 -->
    <div class="login-container">
      <!-- 标题 -->
      <h1 class="login-title ys">
        广航AI盒子展示平台
      </h1>

      <!-- 登录框 -->
      <div class="login-box">
        <Login class="w-full" />
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss">
.login-wrap {
  position: fixed;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  overflow: hidden;
  background: #000;
}

.login-header {
  position: absolute;
  top: 32px;
  left: 36px;
  display: flex;
  align-items: center;
  color: #fff;
  font-size: 20px;
  line-height: 28px;
  z-index: 3;
}

.video-bg {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  object-fit: cover;
  z-index: 1;
  /* 强制覆盖全局CSS限制 */
  max-width: none !important;
  max-height: none !important;
}

.mask-left {
  position: absolute;
  top: 0;
  left: 0;
  width: 1132px;
  height: 100%;
  background: linear-gradient(270deg, rgba(0, 0, 0, 0) 4%, #060913 74%, #000000 100%);
  z-index: 2;
}

.mask-top {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 280px;
  background: linear-gradient(0deg, rgba(0, 0, 0, 0) 4%, #060913 74%, #181a1f 100%);
  z-index: 2;
}

.login-container {
  position: relative;
  z-index: 2;
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  justify-content: center;
  padding-left: 294px; /* 距左侧294px */
}

.login-title {
  font-size: 46px;
  line-height: 60px;
  color: #fff;
  margin: 0 0 20px 20px;
}

.login-box {
  width: 541px;
  height: 393px;
  padding: 48px 90px 60px 90px;
  background-image: url(@/assets/images/login_content_bg.png);
  background-size: 100% 100%;
  background-repeat: no-repeat;
  backdrop-filter: blur(10px);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
}
</style>
