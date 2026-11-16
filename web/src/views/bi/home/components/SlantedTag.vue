<script setup lang="ts">
interface Props {
  variant?: 'status' | 'version'
}

const props = withDefaults(defineProps<Props>(), {
  variant: 'version',
})
</script>

<template>
  <div class="slanted-tag" :class="props.variant">
    <div class="relative z-[1] whitespace-nowrap">
      <slot />
    </div>
    <img
      class="absolute bottom-[-21px] left-1/2 -translate-x-1/2"
      src="@/assets/images/down_arrow.png"
      alt=""
    >
  </div>
</template>

<style scoped lang="scss">
.slanted-tag {
  position: relative;
  display: inline-block;
  padding: 4px 21px;
  color: white;
  font-size: 18px;
  line-height: 23px;
  min-width: 120px;
  text-align: center;

  // 创建五边形形状 - 边框层（长方形 + 左下角切角）
  &::after {
    content: '';
    position: absolute;
    top: -1px;
    left: -1px;
    right: -1px;
    bottom: -1px;
    background: linear-gradient(135deg, rgba(255, 255, 255, 1) 0%, rgba(1, 78, 213, 1) 100%);
    clip-path: polygon(0 0, 100% 0, 100% 100%, 15px 100%, 0 calc(100% - 15px));
    z-index: -2;
  }

  // 创建五边形形状 - 主体层（长方形 + 左下角切角）
  &::before {
    content: '';
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    background: linear-gradient(93deg, #c50c1d 0%, #202b47 100%);
    clip-path: polygon(0 0, 100% 0, 100% 100%, 15px 100%, 0 calc(100% - 15px));
    z-index: -1;
  }
}

// 浮动动画
.slanted-tag {
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
