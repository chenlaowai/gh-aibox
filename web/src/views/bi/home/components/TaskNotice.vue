<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'

interface TaskNoticeItem {
  type: 'success' | 'error'
  title: string
  time: string
}

interface Props {
  data: TaskNoticeItem[]
}

const props = withDefaults(defineProps<Props>(), {
  data: () => [],
})

// 任务公告滚动相关
const containerRef = ref<HTMLElement | null>(null)
const scrollTimer = ref<NodeJS.Timeout | null>(null)
const scrollTop = ref(0)
const isResetting = ref(false)
const itemHeight = 50 // 42px height + 8px gap

const displayTaskNotices = computed(() => {
  if (props.data.length <= 4)
    return props.data
  return [...props.data, ...props.data]
})

function startScroll() {
  stopScroll()
  if (props.data.length <= 4)
    return

  scrollTimer.value = setInterval(() => {
    scrollTop.value += itemHeight
    if (scrollTop.value >= props.data.length * itemHeight) {
      setTimeout(() => {
        isResetting.value = true
        scrollTop.value = 0
        requestAnimationFrame(() => {
          requestAnimationFrame(() => {
            isResetting.value = false
          })
        })
      }, 500)
    }
  }, 3000)
}

function stopScroll() {
  if (scrollTimer.value) {
    clearInterval(scrollTimer.value)
    scrollTimer.value = null
  }
}

function pauseScroll() {
  stopScroll()
}

function resumeScroll() {
  startScroll()
}

// 监听数据变化重新开始滚动
watch(() => props.data, () => {
  scrollTop.value = 0
  startScroll()
})

onMounted(() => {
  startScroll()
})

onBeforeUnmount(() => {
  stopScroll()
})
</script>

<template>
  <div
    ref="containerRef"
    class="notice-list-container"
    @mouseenter="pauseScroll"
    @mouseleave="resumeScroll"
  >
    <div
      class="notice-list-wrapper"
      :style="{
        transform: `translateY(-${scrollTop}px)`,
        transition: isResetting ? 'none' : 'transform 0.5s ease-in-out',
      }"
    >
      <div v-for="(item, idx) in displayTaskNotices" :key="idx" class="notice-item">
        <div class="notice-icon" :class="item.type" />
        <div class="notice-content">
          <n-tooltip trigger="hover" placement="top">
            <template #trigger>
              <div class="notice-text">
                {{ item.title }}
              </div>
            </template>
            {{ item.title }}
          </n-tooltip>
          <div class="notice-time">
            {{ item.time }}
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss">
.notice-list-container {
  padding-top: 11px;
  padding-left: 15px;
  height: 200px;
  overflow: hidden;
  position: relative;
  /* 允许y轴滚动但不显示滚动条 */
  // &:hover {
  //   overflow-y: auto;
  // }
}

.notice-list-wrapper {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.notice-item {
  width: 382px;
  height: 42px;
  padding: 0 23px;
  background: #0539bd33;
  color: #9aa5beff;
  display: flex;
  align-items: center;
  flex-shrink: 0; /* 防止被压缩 */
  .notice-icon {
    width: 17.5px;
    height: 17.5px;

    &.success {
      background: url('@/assets/images/task_success.png') no-repeat center center;
    }

    &.warning {
      background: url('@/assets/images/task_warn.png') no-repeat center center;
    }

    &.error {
      background: url('@/assets/images/task_danger.png') no-repeat center center;
    }
  }

  .notice-content {
    flex: 1;
    display: flex;
    align-items: center;

    .notice-text {
      flex: 1;
      font-size: 12px;
      line-height: 20px;
      margin-left: 7px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    .notice-time {
      font-size: 12px;
      line-height: 17px;
    }
  }
}

/* 自定义滚动条 */
::-webkit-scrollbar {
  width: 2px;
}
</style>
