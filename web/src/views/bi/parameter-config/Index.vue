<script setup lang="tsx">
import { onMounted, ref } from 'vue'
import { useLoading, usePagination, useResponsiveHeight } from '@/hooks'
import NumberAdjuster from '@/components/common/NumberAdjuster.vue'

const { height: containerHeight } = useResponsiveHeight(124)
const { loading, startLoading, endLoading } = useLoading(true)
const pagination = usePagination({ fn: getList })

const paramData = ref<API.GetParamConfigPageResult & { editing: boolean }[]>([])

async function getList() {
  try {
    startLoading()
    const [error, r1]: any = await utils.tryit(api.paramConfig.getParamConfigGetParamConfigPage)({
      PageIndex: pagination.page,
      PageSize: pagination.pageSize,
    })
    if (error || !r1.isSuccess)
      return
    paramData.value = r1.data.list.map(item => ({ ...item, paramValue: Number(item.paramValue), editing: false })) || []
    pagination.itemCount = r1.data.totalCount
  }
  catch (error) {
    console.error('获取参数配置失败:', error)
  }
  finally {
    endLoading()
  }
}

// 处理数值更新
function handleValueUpdate(row: any, value: number) {
  row.paramValue = value
}

// 处理编辑状态更新
function handleEditingUpdate(row: any, editing: boolean) {
  row.editing = editing
}

// 处理保存
async function handleSave(row: any, value: number) {
  try {
    startLoading()
    const [error, res]: any = await utils.tryit(api.paramConfig.postParamConfigUpdateParamConfig)({
      id: row.id,
      paramValue: value.toString(),
    })
    if (error || !res.isSuccess)
      throw error
    window.$message.success('参数配置更新成功')
    getList()
  }
  catch (error) {
    console.error('保存参数配置失败:', error)
  }
  finally {
    endLoading()
  }
}

const columns = [
  { title: '名称', key: 'name', width: '18em' },
  { title: '标识', key: 'code', width: '18em' },
  { title: '类型', key: 'paramType', width: '12em' },
  {
    title: '参数值',
    key: 'paramValue',
    width: '18em',
    render: (row: any) => (
      <NumberAdjuster
        value={row.paramValue}
        editing={row.editing}
        min={0}
        max={1000}
        step={0.1}
        precision={1}
        onUpdate:value={(value: number) => handleValueUpdate(row, value)}
        onUpdate:editing={(editing: boolean) => handleEditingUpdate(row, editing)}
        onSave={(value: number) => handleSave(row, value)}
      />
    ),
  },
  { title: '描述', key: 'remark' },
]

function init() {
  pagination.page = 1
  getList()
}

onMounted(() => {
  init()
})
</script>

<template>
  <div class="mt-[8px] px-[74px]">
    <CardHeader title="参数配置" class="flex flex-col" :style="{ height: containerHeight }">
      <n-data-table
        class="mt-[17px] flex-1 overflow-x-hidden overflow-y-auto"
        :columns="columns"
        :data="paramData"
        :pagination="pagination"
        :loading="loading"
        remote
        flex-height
      />
    </CardHeader>
  </div>
</template>

<style lang="scss" scoped>
</style>
