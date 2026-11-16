<script setup lang="ts">
interface DataItem {
  label: string
  value: string | number
}

interface Props {
  title?: string
  data?: DataItem[]
}

const props = withDefaults(defineProps<Props>(), {
  title: '资源信息',
  data: () => [
    { label: '存储剩余空间', value: '8.9' },
    { label: '芯片温度', value: '36' },
  ],
})
</script>

<template>
  <div class="resource-panel">
    <div class="panel-container">
      <!-- 毛玻璃效果层 -->
      <div class="glassmorphism-layer" />
      <!-- 原SVG背景图片 -->
      <div class="svg-background" />
      <!-- 内容区域 -->
      <div class="panel-content tracking-widest">
        <div
          v-for="(item, index) in props.data"
          :key="index"
          class="whitespace-nowrap text-[16px] text-[#FFFFFFCC] leading-[20px]"
        >
          <span>{{ item.label }}：</span>
          <span class="text-[20px] text-[#00f7ff] font-bold leading-[24px]">
            {{ item.value }}
          </span>
          <span>{{ item.label === '芯片温度' ? '℃' : 'GB' }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss">
.resource-panel {
  position: relative;
  width: 255px;
  height: 192px;

  .panel-container {
    width: 100%;
    height: 100%;
    position: relative;
  }

  // 原SVG背景图片
  .svg-background {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: url('@/assets/images/home_item.svg') no-repeat center center;
    background-size: cover;
    transform: rotateY(25deg);
    z-index: 1;
  }

  // 毛玻璃效果层
  .glassmorphism-layer {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 96, 255, 0.15);
    backdrop-filter: blur(10px) saturate(1.2);
    -webkit-backdrop-filter: blur(10px) saturate(1.2);
    // 使用SVG路径作为剪切形状，并调整为翻转后的坐标
    clip-path: polygon(
      97.8% 53%,
      97.8% 66.1%,
      98% 67%,
      95.1% 71.3%,
      95.1% 95.7%,
      90.3% 99%,
      43.2% 79.8%,
      40.1% 74.9%,
      7.5% 60.5%,
      2.6% 56.3%,
      2.6% 31.6%,
      4.4% 28.2%,
      4.4% 6.5%,
      6.8% 4.2%,
      23.8% 10.1%,
      25.7% 15.6%,
      92.8% 43%,
      97.4% 51.7%
    );
    transform: rotateY(25deg);
    z-index: 2;
  }
}

.panel-content {
  position: absolute;
  top: 50%;
  left: 50%;
  width: fit-content;
  // 让文字与背景图片保持在同一平面，并考虑Y轴翻转
  transform: translate(-50%, -50%) rotateX(33deg) rotateY(33deg);
  z-index: 3; // 确保文字在所有层上方
}

// 浮动动画
.resource-panel {
  animation: float3d 4s ease-in-out infinite;
}

@keyframes float3d {
  0%,
  100% {
    transform: translateY(0px);
  }
  50% {
    transform: translateY(-16px);
  }
}
</style>
