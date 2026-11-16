<script setup lang="tsx">
import { h } from 'vue'
import { useAutofitDrawer, useLoading, useNaiveForm } from '@/hooks'
import DrawerHeader from '@/components/common/DrawerHeader.vue'
import { useAppStore } from '@/store'
import { useAuthStore } from '@/store/auth'
import { uploadAiMedia } from '@/services/aiTask'

interface Props {
  visible: boolean
  id: string | null
}

interface Emits {
  (e: 'update:visible', visible: boolean): void
  (e: 'refresh'): void
}

const props = withDefaults(defineProps<Props>(), {
  visible: false,
  id: null,
})

const emit = defineEmits<Emits>()

// 使用autofit适配hooks
const { drawerWidth, onDrawerVisible } = useAutofitDrawer(730)
const { loading, startLoading, endLoading } = useLoading(false)
const appStore = useAppStore()
const authStore = useAuthStore()

const formModel = ref<API.UpdateChannelRequest>({
  name: '',
  id: '',
  protocolType: 0,
  address: '',
  code: '',
  remark: '',
})

const modalVisible = computed({
  get() {
    return props.visible
  },
  set(visible) {
    closeModal(visible)
  },
})

const rules = {
  name: [
    { required: true, message: '请输入通道名称' },
    { message: '名称不超过20个字符', trigger: ['input', 'blur'], max: 20 },
  ],
  code: [
    { required: true, message: '请输入监控编码' },
    { message: '编码不超过50个字符', trigger: ['input', 'blur'], max: 50 },
  ],
  remark: [
    { required: true, message: '请输入监控描述' },
    { message: '描述不超过100个字符', trigger: ['input', 'blur'], max: 100 },
  ],
  protocolType: [{ required: true, message: '请选择探测协议' }],
  address: [
    { required: true, message: '请输入视频地址' },
    { message: 'URL不超过255个字符', trigger: ['input', 'blur'], max: 255 },
  ],
}

const { formRef, validate } = useNaiveForm()

function closeModal(visible = false) {
  emit('update:visible', visible)
}

async function onSave() {
  try {
    await validate()
    startLoading()
    // The backend rejects a duplicate channel name with a raw 500. Resolve it
    // before saving so the operator gets a usable unique name instead of
    // "服务器错误，请重试".
    if (!formModel.value.id) {
      const [listErr, listRes]: any = await utils.tryit(
        api.channel.getChannelGetChannelsPage,
      )({ PageIndex: 1, PageSize: 500 })
      if (!listErr && listRes?.isSuccess) {
        const channels = listRes.data?.list || []
        const names = new Set(channels.map((c: any) => c.name))
        if (names.has(formModel.value.name)) {
          const base = formModel.value.name
          let n = 2
          while (names.has(`${base}_${n}`))
            n++
          formModel.value.name = `${base}_${n}`
          window.$message.warning(`通道名称已存在，已自动改为 ${formModel.value.name}`)
        }
      }
    }
    const fn = formModel.value.id
      ? api.channel.putChannelUpdateChannel
      : api.channel.postChannelAddChannel
    // The backend protocol enum only accepts 0..5.  MP4 is a frontend-only
    // option; persist it as "手动填写" (0) and let the .mp4 address identify
    // the file source.
    const payload: any = { ...formModel.value }
    if (Number(payload.protocolType) === 99)
      payload.protocolType = 0
    const [error, res]: any = await utils.tryit(fn)(payload)
    if (error || !res.isSuccess)
      throw error
    closeModal()
    emit('refresh')
    window.$message.success(formModel.value.id ? '编辑成功' : '新增成功')
  }
  catch (error) {
    console.error('表单验证失败:', error)
  }
  finally {
    endLoading()
  }
}

/**
 * 编辑详情查询
 */
async function loadDetail() {
  if (props.id) {
    startLoading()
    const [error, res]: any = await utils.tryit(api.channel.getChannelGetChannelDetailId)({
      id: props.id,
    })
    endLoading()
    if (error || !res.isSuccess)
      return
    formModel.value = { ...res.data, appType: res.data.appType } as any
    if (/\.mp4(?:$|\?)/i.test(formModel.value.address || ''))
      formModel.value.protocolType = 99 as any
  }
}

// 获取协议类型选项；MP4 是本地测试视频，固定值 99（后端只存储）。
const protocalTypeOptions = computed(() => {
  const options = [...(appStore.protocolTypeEnum || [])]
  if (!options.some((x: any) => Number(x.value) === 99)) {
    options.push({
      label: 'MP4',
      value: 99,
      remark: '本地视频循环播放（不上报告警）',
    })
  }
  return options
})

const isMp4 = computed(() => Number(formModel.value.protocolType) === 99)
const uploading = ref(false)

async function uploadFile({ file, onFinish, onError }: any) {
  const raw = file?.file
  if (!raw)
    return
  uploading.value = true
  try {
    const path = await uploadAiMedia(raw, authStore.rawAccessToken)
    formModel.value.address = path
    onFinish()
    window.$message.success('视频上传成功')
  }
  catch (e: any) {
    onError()
    window.$message.error(e?.message || '视频上传失败')
  }
  finally {
    uploading.value = false
  }
}

// 初始化系统字典
onMounted(async () => {
  await appStore.loadSystemDict()
})

watch(
  () => props.visible,
  (newValue) => {
    if (!newValue) {
      formModel.value = {
        name: '',
        id: '',
        protocolType: 0,
        address: '',
        code: '',
        remark: '',
      }
    }
    else {
      loadDetail()
    }
    // 使用hooks提供的方法处理抽屉显示时的宽度更新
    onDrawerVisible(newValue)
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
      <DrawerHeader class="pb-[30px]" :title="`${formModel.id ? '编辑' : '新增'}`" />
      <n-form
        ref="formRef"
        :rules="rules"
        label-placement="left"
        :model="formModel"
        label-align="right"
        :label-width="80"
        class="flex-1"
        :loading="loading"
      >
        <n-grid :cols="1" :y-gap="16">
          <n-form-item-grid-item label="通道名称" path="name">
            <n-input
              v-model:value="formModel.name"
              v-trim
              placeholder="请输入通道名称"
            />
          </n-form-item-grid-item>
          <n-form-item-grid-item label="监控编码" path="code">
            <n-input
              v-model:value="formModel.code"
              v-trim
              placeholder="请输入监控编码"
              :disabled="!!formModel.id"
            />
          </n-form-item-grid-item>

          <n-form-item-grid-item label="监控描述" path="remark">
            <n-input
              v-model:value="formModel.remark"
              v-trim
              placeholder="请输入监控描述" type="textarea"
              rows="4"
            />
          </n-form-item-grid-item>

          <n-form-item-grid-item label="探测协议" path="protocolType">
            <n-select
              v-model:value="formModel.protocolType"
              :options="protocalTypeOptions"
              placeholder="请选择探测协议"
              :render-label="(option: any) => `${option.label} - ${option.remark}`"
              :render-tag="({ option }: any) => h('span', option.label)"
            />
          </n-form-item-grid-item>

          <n-form-item-grid-item :label="isMp4 ? 'MP4 视频' : '视频地址'" path="address">
            <div v-if="isMp4" class="w-full">
              <n-upload
                :show-file-list="false"
                :custom-request="uploadFile"
                accept="video/mp4"
                action="#"
              >
                <n-button :loading="uploading" type="primary" ghost>
                  选择并上传 MP4
                </n-button>
              </n-upload>
              <div v-if="formModel.address" class="mt-2 break-all text-12px text-[#8aa4c8]">
                {{ formModel.address }}
              </div>
              <div class="mt-1 text-12px text-[#8aa4c8]">
                完成后任务通道循环播放该视频，检测照常运行，但不会上传告警事件。
              </div>
            </div>
            <n-input
              v-else
              v-model:value="formModel.address"
              v-trim
              placeholder="请输入视频地址"
            />
          </n-form-item-grid-item>
        </n-grid>
      </n-form>
      <n-space justify="end" class="mt-[20px]">
        <custom-button class="h-[40px] w-[110px]" @click="closeModal()">
          取消
        </custom-button>
        <custom-button class="h-[40px] w-[110px]" type="success" @click="onSave">
          保存
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
}
</style>
