<script setup lang="ts">
import bottomLeft from '@/assets/images/current-preview/bottom_left.png'
import bottomRight from '@/assets/images/current-preview/bottom_right.png'

// 定义Props接口
interface PreviewCardProps {
  /** 预览卡片ID */
  id: number
  /** 时间 */
  time: string
  /** 标题 */
  title: string
  /** 图片URL */
  imageUrl: string
  /** 描述信息 */
  description?: string
}

// 定义Props
const props = withDefaults(defineProps<PreviewCardProps>(), {
  description: '',
})

// 定义事件
const emit = defineEmits<{
  click: [item: PreviewCardProps]
}>()

// 点击事件处理
function handleClick() {
  emit('click', props)
}
</script>

<template>
  <div class="preview-card" @click="handleClick">
    <img class="h-full w-full object-cover" :src="imageUrl" :alt="title">

    <!-- 左上角徽标 -->
    <div class="absolute left-0 top-0 z-2 h-[25px] w-[83px] flex items-center bg-[url('@/assets/images/current-preview/top_left.png')]">
      <span class="ml-[35px] text-[14px] leading-[20px]">通道</span>
    </div>

    <!-- 右上角蓝色三角形装饰 -->
    <div class="top-right-triangle" />

    <!-- 内阴影层 -->
    <div class="inner-shadow" />

    <!-- 右上角时间 -->
    <div class="absolute right-34px top-15px z-5 text-[14px] leading-[20px]">
      {{ props.time }}
    </div>

    <!-- 底部蒙层和标题 -->
    <div class="absolute bottom-0 left-0 right-0 h-[50px] flex items-center bg-[#00000066] px-[46px]">
      <div class="w-full truncate font-size-[16px] leading-[22px]">
        {{ props.title }}
      </div>
    </div>

    <!-- 底部装饰 -->
    <div class="absolute bottom-0 left-0 right-0 z-4 h-[50px]">
      <img class="absolute bottom-0 left-0 max-h-full" :src="bottomLeft" alt="bottom-left">
      <img class="absolute bottom-0 right-0 max-h-full" :src="bottomRight" alt="bottom-right">
    </div>
  </div>
</template>

<style lang="scss" scoped>
.preview-card {
  position: relative;
  width: 510px;
  height: 312px;
  border: 1px solid #0047ffff;
  clip-path: polygon(
    0 0,
    /* 左上角 */ 100% 0,
    /* 右上角 */ 100% calc(100% - 22px),
    /* 右侧到右下角三角形起点 */ calc(100% - 22px) 100%,
    /* 右下角三角形终点到底边 */ 22px 100%,
    /* 底边到左下角三角形起点 */ 0 calc(100% - 22px) /* 左下角三角形终点到左侧 */
  );
}

.top-right-triangle {
  position: absolute;
  top: 0;
  right: 0;
  width: 24px;
  height: 24px;
  background-color: #2059ffff;
  clip-path: polygon(100% 0, 0 0, 100% 100%);
  z-index: 3;
}

.inner-shadow {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  pointer-events: none;
  box-shadow: inset 0px 0px 14px 0px #0044ff;
  z-index: 1;
}
</style>
