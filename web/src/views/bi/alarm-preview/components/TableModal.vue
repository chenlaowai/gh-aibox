<script setup lang="tsx">
import { useAutofitDrawer, useLoading } from '@/hooks'
import DrawerHeader from '@/components/common/DrawerHeader.vue'
import SmartImage from '@/components/common/SmartImage.vue'
import { resolveAlarmMediaUrl } from '../media'

const props = withDefaults(defineProps<Props>(), {
  visible: false,
  detailId: null,
})

const emit = defineEmits<Emits>()

enum ReportStatusEnum {
  pending = 0,
  success,
  failed,
}

interface Props {
  visible: boolean
  detailId: string | null
}

interface Emits {
  (e: 'update:visible', visible: boolean): void
}

const modalData = ref<API.GetAlarmDetailResult | null>(null)
const { loading, startLoading, endLoading } = useLoading(false)

// 使用autofit适配hooks
const { drawerWidth, onDrawerVisible } = useAutofitDrawer(730)

const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

function closeModal(visible = false) {
  emit('update:visible', visible)
}

async function loadDetail() {
  if (props.detailId) {
    try {
      startLoading()
      const [error, res] = await utils.tryit(api.alarm.getAlarmGetAlarmDetailId)({ id: props.detailId })
      if (error || !res.isSuccess)
        return
      res.data.imgPath = resolveAlarmMediaUrl(res.data.imgPath)
      res.data.videoPath = resolveAlarmMediaUrl(res.data.videoPath)
      modalData.value = res.data
    }
    catch (error) {
      console.error('获取告警详情失败:', error)
    }
    finally {
      endLoading()
    }
  }
}

watch(
  () => props.visible,
  (newValue) => {
    // 使用hooks提供的方法处理抽屉显示时的宽度更新
    onDrawerVisible(newValue)
    if (newValue) {
      loadDetail()
    }
  },
)
</script>

<template>
  <n-drawer
    v-model:show="modalVisible"
    :width="drawerWidth"
    placement="right"
    :mask-closable="false"
    to="#app"
  >
    <n-drawer-content>
      <DrawerHeader class="pb-[30px]" title="告警详情" size="large" />
      <div v-if="modalData && !loading" class="flex-1 overflow-y-auto">
        <!-- 详情信息列表 -->
        <div class="flex flex-col gap-[16px]">
          <div class="detail-row">
            <span class="detail-label">任务编号：</span>
            <span class="detail-value">{{ modalData.taskCode }}</span>
          </div>

          <div class="detail-row">
            <span class="detail-label">类别：</span>
            <span class="detail-value">{{ modalData.alarmTypeRemark }}</span>
          </div>

          <div class="detail-row">
            <span class="detail-label">标识：</span>
            <span class="detail-value">{{ modalData.alarmTypeCode }}</span>
          </div>

          <div class="detail-row">
            <span class="detail-label">监控通道：</span>
            <span class="detail-value">通道 {{ modalData.channelAddress }}</span>
          </div>

          <div class="detail-row">
            <span class="detail-label">上报情况：</span>
            <CustomTag
              v-if="modalData.reportStatus === ReportStatusEnum.pending"
              type="info"
            >
              待上传
            </CustomTag>
            <CustomTag
              v-else-if="modalData.reportStatus === ReportStatusEnum.success"
              type="success"
            >
              上传成功
            </CustomTag>
            <CustomTag
              v-else-if="modalData.reportStatus === ReportStatusEnum.failed"
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
          <div class="detail-row">
            <span class="detail-label">上报地址：</span>
            <span class="detail-value">{{ modalData.reportAddress }}</span>
          </div>
          <div class="detail-row">
            <span class="detail-label">告警人数：</span>
            <span class="detail-value">{{ modalData.reportNums }}</span>
          </div>
          <div class="detail-row">
            <span class="detail-label">图片预览：</span>
            <div class="detail-value relative h-[200px] w-full">
              <SmartImage
                :src="modalData.imgPath || ''"
                :alt="modalData.alarmTypeRemark || ''"
                placeholder-icon="📷"
                fit="auto-width"
                placeholder-width="300px"
                class="h-full w-fit border border-gray-300 rounded"
              />
            </div>
          </div>
          <div class="detail-row">
            <span class="detail-label">视频预览：</span>
            <div class="detail-value relative h-[200px] w-full">
              <video :src="modalData.videoPath || ''" controls class="h-full" />
            </div>
          </div>
        </div>
      </div>

      <n-space justify="end" class="mt-[40px]">
        <custom-button class="h-[40px] w-[110px]" @click="closeModal()">
          关闭
        </custom-button>
      </n-space>
    </n-drawer-content>
  </n-drawer>
</template>

<style scoped>
/* 去掉drawer头部的border-bottom */
:deep(.n-drawer-content .n-drawer-content__header) {
  border-bottom: none !important;
  padding-bottom: 0 !important;
}

/* 调整drawer内容的间距 */
:deep(.n-drawer-content .n-drawer-content__body) {
  padding-top: 0 !important;
}

:deep(.n-drawer-body-content-wrapper) {
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  padding: 85px 70px 120px 50px !important;
  height: 100%;
}

/* 详情项样式 */

.detail-row {
  display: flex;
  align-items: flex-start;
  gap: 8px;
}

.detail-label {
  font-size: 16px;
  font-weight: 500;
  color: #ffffff;
  opacity: 0.8;
  min-width: 100px;
  flex-shrink: 0;
}

.detail-value {
  font-size: 16px;
  color: #ffffff;
  font-weight: 400;
  flex: 1;
}

/* 状态样式 */
.status-success {
  color: #00ff95ff !important;
  border: 1px solid #00ff95ff;
  padding: 4px 8px;
  border-radius: 4px;
  display: inline-block;
  font-size: 14px;
}

.status-failed {
  color: #f50102ff !important;
  border: 1px solid #f50102ff;
  padding: 4px 8px;
  border-radius: 4px;
  display: inline-block;
  font-size: 14px;
}

/* 图片预览样式 */
.detail-section h3 {
  margin: 0 0 16px 0;
  font-size: 16px;
  font-weight: 500;
  color: #ffffff;
}
</style>
