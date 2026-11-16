<script setup lang="ts">
import SmartImage from '@/components/common/SmartImage.vue'
import { AlarmReportStatusEnum } from '@/constants/index'

// 定义Props接口
interface AlarmCardProps {
  /** 告警数据对象 */
  alarm: API.QueryAlarmPageResult
  /** 是否为批量模式 */
  isBatchMode?: boolean
  /** 是否被选中 */
  isSelected?: boolean
}

// 定义Props
const props = withDefaults(defineProps<AlarmCardProps>(), {
  isBatchMode: false,
  isSelected: false,
})

// 定义事件
const emit = defineEmits<{
  toggleSelection: [id: string]
  click: [alarm: API.QueryAlarmPageResult]
  delete: [alarm: API.QueryAlarmPageResult]
  detail: [alarm: API.QueryAlarmPageResult]
}>()

// 切换选择状态
function handleToggleSelection(_checked: boolean) {
  emit('toggleSelection', props.alarm.id!)
}

// 阻止复选框区域的点击事件冒泡
function stopPropagation(event: Event) {
  event.stopPropagation()
}

// 卡片点击事件
function handleCardClick() {
  if (props.isBatchMode) {
    emit('toggleSelection', props.alarm.id!)
  }
  else {
    emit('click', props.alarm)
  }
}
</script>

<template>
  <div
    class="alarm-card"
    :class="{ 'selected': isBatchMode && isSelected, 'cursor-pointer': isBatchMode }"
    @click="handleCardClick"
  >
    <!-- 图片 -->
    <div class="relative h-[302px] flex justify-center overflow-hidden px-[2px] pt-[2px]">
      <SmartImage
        :src="alarm.imgPath || ''"
        :alt="alarm.alarmTypeRemark || ''"
        placeholder-icon="📷"
        class="h-full w-full"
        :preview-disabled="true"
      />

      <div class="absolute bottom-0 left-0 h-[40px] w-full flex items-center bg-[rgba(0,0,0,0.3)] pl-[33px]">
        <span>{{ alarm.taskCode }}</span>
      </div>
    </div>

    <!-- 左上角徽标 -->
    <div class="absolute left-0 top-0 z-2 h-[25px] w-[83px] flex items-center bg-[url('@/assets/images/current-preview/top_left.png')]">
      <span class="ml-[35px] text-[14px] leading-[20px]">通道</span>
    </div>

    <!-- 右上角蓝色三角形装饰 -->
    <div class="top-right-triangle" />

    <!-- 右上角时间 -->
    <div class="absolute right-34px top-15px z-5 text-[14px] leading-[20px]">
      {{ alarm.alarmTime }}
    </div>

    <!-- 内容信息 -->
    <div class="msg-info mb-[14px] h-[180px] flex flex-col justify-center gap-9px pl-[33px] pr-[25px]">
      <div class="flex justify-between">
        <span>监控通道：{{ alarm.channelName }}</span>
        <!-- 批量模式选择框 -->
        <div v-if="isBatchMode" @click="stopPropagation">
          <n-checkbox
            :checked="isSelected"
            @update:checked="handleToggleSelection"
          />
        </div>
      </div>
      <div>
        警告日期：{{ alarm.alarmTime }}
      </div>
      <div>
        上报状态：
        <CustomTag
          v-if="alarm.reportStatus === AlarmReportStatusEnum.pending"
          type="info"
        >
          待上传
        </CustomTag>
        <CustomTag
          v-else-if="alarm.reportStatus === AlarmReportStatusEnum.success"
          type="success"
        >
          上传成功
        </CustomTag>
        <CustomTag
          v-else-if="alarm.reportStatus === AlarmReportStatusEnum.failed"
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
      <div class="flex justify-between" @click="stopPropagation">
        <span>告警内容：
          <custom-tag type="warning">
            {{ alarm.alarmTypeRemark }}
          </custom-tag></span>
        <div class="flex justify-center gap-2">
          <n-button
            size="small"
            quaternary
            type="primary"
            @click="() => {
              emit('detail', alarm)
            }"
          >
            详情
          </n-button>
          <n-button
            size="small"
            quaternary
            type="error"
            @click="() => {
              emit('delete', alarm)
            }"
          >
            删除
          </n-button>
        </div>
      </div>
    </div>
  </div>
</template>

<style lang="scss" scoped>
.alarm-card {
  position: relative;
  width: 510px;
  height: 492px;
  overflow: hidden;
  background-image: url('@/assets/images/alarm_item_bg.png');
  background-size: 100% 100%;
  background-repeat: no-repeat;
  background-position: center;

  &.selected {
    background-image: url('@/assets/images/alarm_item_bg_selected.png');
  }
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

.msg-info > div {
  line-height: 24px;
}
</style>
