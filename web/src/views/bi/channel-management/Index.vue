<script setup lang="tsx">
import { onMounted, onUnmounted, ref } from 'vue'
import TableModal from './components/TableModal.vue'
import { useBoolean, useLoading, usePagination, useResponsiveHeight } from '@/hooks'
import { useAuthStore } from '@/store/auth'
import CustomTag from '@/components/common/CustomTag.vue'
import { compactStreamError, fetchAiTaskHealth, probeAiChannel, type AiTaskHealth } from '@/services/aiTask'

enum ChannelStatusType {
  Online = 1,
  Offline = 2,
}

const { height: containerHeight } = useResponsiveHeight(124)
const { bool: visible, setBool: setVisible } = useBoolean(false)
const { bool: deleteVisible, setBool: setDeleteVisible } = useBoolean(false)
const { loading, startLoading, endLoading } = useLoading(true)

const detailTarget = ref<string | null>(null)
const deleteTarget = ref<any | null>(null)

const channelData = ref<API.GetChannelPageResult[]>([])
const authStore = useAuthStore()
const aiByAddress = ref<Record<string, AiTaskHealth>>({})
const probeByAddress = ref<Record<string, any>>({})
const probeBusy = ref<Record<string, boolean>>({})
let healthTimer: ReturnType<typeof setInterval> | null = null
let lastProbeClear = 0

async function refreshAiHealth() {
  if (Date.now() - lastProbeClear > 30000) {
    lastProbeClear = Date.now()
    probeByAddress.value = {}
  }
  const list = await fetchAiTaskHealth(authStore.rawAccessToken)
  const map: Record<string, AiTaskHealth> = {}
  for (const item of list) {
    if (item.channelAddress)
      map[item.channelAddress] = item
  }
  aiByAddress.value = map
  void probeIdleChannels()
}

async function probeIdleChannels() {
  const queue = channelData.value.filter((row: any) => {
    const addr = row.address || ''
    return addr
      && row.channelStatus === ChannelStatusType.Online
      && !aiByAddress.value[addr]
      && !probeByAddress.value[addr]
      && !probeBusy.value[addr]
  })
  if (!queue.length)
    return
  const worker = async () => {
    while (queue.length) {
      const row: any = queue.shift()
      const addr = row.address || ''
      if (!addr || probeBusy.value[addr])
        continue
      probeBusy.value[addr] = true
      const res = await probeAiChannel(addr, authStore.rawAccessToken)
      probeBusy.value[addr] = false
      if (res) {
        probeByAddress.value = {
          ...probeByAddress.value,
          [addr]: { ...res, lastFrameAgeMs: 0 },
        }
      }
    }
  }
  await Promise.all([worker(), worker()])
}

function channelHealthDisplay(row: API.GetChannelPageResult) {
  const addr = row.address || ''
  const h = aiByAddress.value[addr]
  if (h) {
    if (h.health === 'ok') {
      return {
        text: h.resolution ? `正常 ${h.resolution}` : '正常',
        type: 'success' as const,
      }
    }
    if (h.health === 'stalled')
      return { text: `已断开 ${Math.max(1, Math.floor((h.lastFrameAgeMs || 0) / 1000))}s`, type: 'error' as const }
    if (h.lastError)
      return { text: compactStreamError(h.lastError), type: 'warning' as const }
    return { text: '正在连接', type: 'warning' as const }
  }
  const p = probeByAddress.value[addr]
  if (p) {
    if (p.status === 'ok')
      return { text: p.resolution ? `正常 ${p.resolution}` : '正常', type: 'success' as const }
    return { text: compactStreamError(p.lastError) || 'Camera not found', type: 'warning' as const }
  }
  if (probeBusy.value[addr])
    return { text: '正在探测', type: 'info' as const }
  return row.channelStatus === ChannelStatusType.Online
    ? { text: '未接入', type: 'info' as const }
    : { text: '离线', type: 'error' as const }
}

function onAddOrEdit(row: API.GetChannelPageResult) {
  detailTarget.value = row.id || null
  setVisible(true)
}

function onDeleteClick(row: API.GetChannelPageResult) {
  deleteTarget.value = row
  setDeleteVisible(true)
}

async function onConfirmDelete() {
  if (!deleteTarget.value?.id) {
    return window.$message.warning('ID不能为空')
  }

  try {
    startLoading()
    const [error, res] = await utils.tryit(api.channel.deleteChannelDeleteChannelId)({ id: deleteTarget.value.id })
    if (error || !res.isSuccess)
      return
    window.$message.success('删除成功')
    getList()
    deleteTarget.value = null
  }
  catch (error) {
    console.error('删除通道失败:', error)
  }
  finally {
    endLoading()
  }
}

const pagination = usePagination({ fn: getList })
async function getList() {
  try {
    startLoading()
    const [error, r1]: any = await utils.tryit(api.channel.getChannelGetChannelsPage)({
      PageIndex: pagination.page,
      PageSize: pagination.pageSize,
    })
    if (error || !r1.isSuccess)
      return
    channelData.value = r1.data.list || []
    pagination.itemCount = r1.data.totalCount
  }
  catch (error) {
    console.error('获取通道列表失败:', error)
  }
  finally {
    endLoading()
  }
}

const columns = [
  { title: '通道名称', key: 'name', width: '18em' },
  { title: '监控编码', key: 'code', width: '18em' },
  { title: '视频地址', key: 'address' },
  { title: '通道状态', key: 'streamStatus', width: '14em', render: (row) => {
    const display = channelHealthDisplay(row)
    return <CustomTag type={display.type}>{display.text}</CustomTag>
  } },
  {
    title: '操作',
    align: 'center' as const,
    key: 'actions',
    width: '15em',
    render: (row) => {
      return (
        <div class="flex justify-center gap-2">
          <n-button
            size="small"
            quaternary
            type="primary"
            onClick={() => onAddOrEdit(row)}
          >
            编辑
          </n-button>
          <n-button
            size="small"
            quaternary
            type="error"
            onClick={() => onDeleteClick(row)}
          >
            删除
          </n-button>
        </div>
      )
    },
  },
]

function init() {
  pagination.page = 1
  getList()
}

onMounted(() => {
  init()
  refreshAiHealth()
  healthTimer = setInterval(refreshAiHealth, 5000)
})

onUnmounted(() => {
  if (healthTimer)
    clearInterval(healthTimer)
})
</script>

<template>
  <div class="mt-[8px] px-[74px]">
    <CardHeader title="通道管理" class="flex flex-col" :style="{ height: containerHeight }">
      <template #header-extra>
        <NButton type="info" @click="onAddOrEdit({})">
          <template #icon>
            <icon-park-outline-plus />
          </template>
          新增
        </NButton>
      </template>
      <n-data-table
        class="mt-[17px] flex-1 overflow-x-hidden overflow-y-auto"
        :columns="columns"
        :data="channelData"
        :pagination="pagination"
        :loading="loading"
        remote
        flex-height
      />
    </CardHeader>
  </div>
  <TableModal :id="detailTarget" v-model:visible="visible" @refresh="getList" />
  <DeleteModal v-model:visible="deleteVisible" @confirm="onConfirmDelete" />
</template>

<style lang="scss" scoped>
</style>
