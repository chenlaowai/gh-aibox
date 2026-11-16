<script setup lang="ts">
import { ref } from 'vue'
import { NCarousel, NCarouselItem } from 'naive-ui'
import SmartImage from '@/components/common/SmartImage.vue'
import { AlarmReportStatusEnum } from '@/constants/index'

interface AlertItem {
  title: string
  reportStatus: string
  time: string
  image: string
}

interface Props {
  data?: AlertItem[]
}

const props = withDefaults(defineProps<Props>(), {
  data: () => [],
})

const currentIndex = ref(0)
const carouselRef = ref()

function handleIndexChange(index: number) {
  currentIndex.value = index
}

function goToSlide(index: number) {
  currentIndex.value = index
  carouselRef.value?.to(index)
}
</script>

<template>
  <div class="alert-carousel">
    <!-- 调试信息 -->
    <div v-if="!props.data || props.data.length === 0" class="mt-[30px] bg-[#ff6b6b1a] p-[20px] text-center text-[14px] text-[#ff6b6b] leading-[20px]">
      暂无内容
    </div>

    <NCarousel
      v-else
      ref="carouselRef"
      :current-index="currentIndex"
      :autoplay="true"
      :interval="5000"
      :show-dots="false"
      :show-arrow="false"
      @update:current-index="handleIndexChange"
    >
      <NCarouselItem v-for="(item, index) in props.data" :key="index">
        <div class="box-content h-[130px] flex gap-[12px] pl-[33px] pt-[33px]">
          <div class="desc relative aspect-ratio-[1/1] h-[100%] border-[1px] border-[#203E8AFF]">
            <!-- 四个角的白边修饰 -->
            <div class="pointer-events-none absolute bottom-0 left-0 right-0 top-0 z-[10]">
              <div class="corner-line right-0 top-[-1px]" />
              <div class="corner-line bottom-[-1px] left-0" />
              <div class="corner-line bottom-[-1px] right-0" />
            </div>

            <img class="absolute left-0 top-0 w-[83px]" src="@/assets/images/home_alert1.png" alt="告警">
            <!-- 图片容器 -->
            <div class="relative h-[100%] w-[100%]">
              <SmartImage
                :src="item.image"
                :alt="item.title"
                placeholder-icon="📷"
                class="h-[100%] w-[100%]"
              />
            </div>
            <div class="absolute bottom-0 left-0 h-[25px] w-[100%] flex items-center justify-center bg-[#12234D] text-[12px] text-white">
              {{ item.time }}
            </div>
          </div>
          <div class="w-[165px] flex flex-col gap-[7px]">
            <div class="mt-[6px] truncate text-[16px] text-[#FFFFFF] leading-[22px]">
              {{ item.title }}
            </div>
            <div class="line-clamp-4 text-[14px] text-[#FFFFFFCC] leading-[20px]">
              上报状态：
              <p class="mb-7px" />
              <CustomTag
                v-if="Number(item.reportStatus) === AlarmReportStatusEnum.pending"
                type="info"
              >
                待上传
              </CustomTag>
              <CustomTag
                v-else-if="Number(item.reportStatus) === AlarmReportStatusEnum.success"
                type="success"
              >
                上传成功
              </CustomTag>
              <CustomTag
                v-else-if="Number(item.reportStatus) === AlarmReportStatusEnum.failed"
                type="error"
              >
                上传失败
              </CustomTag>
              <CustomTag
                v-else
                type="warning"
              >
                未知状态
              </CustomTag>
            </div>
          </div>
        </div>
      </NCarouselItem>
    </NCarousel>

    <!-- 自定义指示器 -->
    <div v-if="props.data && props.data.length > 1" class="custom-dots">
      <div
        v-for="(_, index) in props.data"
        :key="index"
        class="dot" :class="[{ active: currentIndex === index }]"
        @click="goToSlide(index)"
      />
    </div>
  </div>
</template>

<style scoped lang="scss">
.alert-carousel {
  position: relative;
  min-height: 200px;
  height: 100%;
  width: 100%;
  max-width: 400px; /* 限制最大宽度 */
}

/* 四个角的白边修饰 */
.corner-line {
  position: absolute;
  background: #fff;
  width: 7px;
  height: 1px;
}

.custom-dots {
  position: absolute;
  bottom: 8px;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  gap: 8px;
}

.dot {
  width: 8px;
  height: 5px;
  background: #0060ff99;
  border-radius: 2.5px;
  cursor: pointer;
  transition: all 0.3s ease;

  &.active {
    width: 17px;
    background: #0060ffff;
  }

  &:hover {
    opacity: 0.8;
  }
}

// 覆盖naive-ui carousel的默认样式
:deep(.n-carousel) {
  height: 100%;

  .n-carousel__slides {
    height: 100%;
  }

  .n-carousel__slide {
    height: 100%;
  }
}
</style>
