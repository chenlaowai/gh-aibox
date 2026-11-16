<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as echarts from 'echarts'

interface WarningItem {
  name: string
  count: number
  color: string
}

interface Props {
  data?: WarningItem[]
}

const props = withDefaults(defineProps<Props>(), {
  data: () => [
    { name: '人员聚合', count: 25, color: 'linear-gradient(180deg, rgba(7, 91, 255, 1), rgba(48, 94, 250, 1))' },
    { name: '现场抽烟', count: 18, color: '#F50102FF' },
    { name: '未戴安全帽', count: 32, color: 'linear-gradient(136deg, rgba(8, 202, 90, 1), rgba(15, 194, 205, 1))' },
    { name: '未穿反光衣', count: 15, color: 'linear-gradient(225deg, rgba(245, 101, 1, 1), rgba(245, 162, 2, 1))' },
  ],
})

const chartRef = ref<HTMLElement>()
let chart: echarts.ECharts | null = null

// 跟踪图例选中状态
const legendSelected = ref<Record<string, boolean>>({})

// 计算总人次和百分比
const totalCount = computed(() => props.data.reduce((sum, item) => sum + item.count, 0))

// 计算当前选中项的总人次（根据图例选中状态）
const selectedCount = computed(() => {
  return props.data
    .filter(item => legendSelected.value[item.name] !== false) // 未设置或为true的项目
    .reduce((sum, item) => sum + item.count, 0)
})

// 计算当前实际在饼图中显示的条目数量（必须是选中状态且数量大于0）
const activeDataCount = computed(() => {
  return props.data.filter((item) => {
    return legendSelected.value[item.name] !== false && item.count > 0
  }).length
})

const dataWithPercentage = computed(() =>
  props.data.map(item => ({
    ...item,
    percentage: Math.round((item.count / totalCount.value) * 100),
  })),
)

// 创建渐变色函数
function createGradient(colorStr: string) {
  if (colorStr.startsWith('linear-gradient')) {
    // 解析渐变色字符串并转换为ECharts渐变
    if (colorStr.includes('180deg')) {
      return new echarts.graphic.LinearGradient(0, 0, 0, 1, [
        { offset: 0, color: 'rgba(7, 91, 255, 1)' },
        { offset: 1, color: 'rgba(48, 94, 250, 1)' },
      ])
    }
    else if (colorStr.includes('136deg')) {
      return new echarts.graphic.LinearGradient(0, 0, 1, 1, [
        { offset: 0, color: 'rgba(8, 202, 90, 1)' },
        { offset: 1, color: 'rgba(15, 194, 205, 1)' },
      ])
    }
    else if (colorStr.includes('225deg')) {
      return new echarts.graphic.LinearGradient(1, 0, 0, 1, [
        { offset: 0, color: 'rgba(245, 101, 1, 1)' },
        { offset: 1, color: 'rgba(245, 162, 2, 1)' },
      ])
    }
  }
  return colorStr
}

function updateChart() {
  if (!chart)
    return

  const option = {
    title: {
      text: `{count|${selectedCount.value}}\n{label|总人次}`,
      left: 65, // 圆环中心的水平位置
      top: 'center',
      textAlign: 'center', // 标题内部文字水平居中
      textStyle: {
        rich: {
          count: {
            fontSize: 36,
            fontWeight: 'bold',
            color: '#fff',
            lineHeight: 36,
            align: 'center', // 数字部分居中
          },
          label: {
            fontSize: 12,
            color: '#fff',
            lineHeight: 12,
            padding: [6, 0, 0, 0],
            align: 'center', // 文字部分居中
          },
        },
      },
    },
    tooltip: {
      trigger: 'item',
      backgroundColor: 'rgba(0, 0, 0, 0.8)',
      borderColor: '#1D73FEFF',
      borderWidth: 1,
      textStyle: {
        color: '#fff',
        fontSize: 12,
      },
      formatter: (params: any) => {
        const item = dataWithPercentage.value.find(item => item.name === params.name)

        if (item) {
          return `
            <div>
              <div style="display: flex; align-items: center; margin-bottom: 4px;">
                <span style="font-weight: bold;">${params.name}</span>
              </div>
              <div style="color: #ccc;">
                <div>人数: <span style="color: #fff; font-weight: bold;">${item.count}</span> 人</div>
                <div>占比: <span style="color: #fff; font-weight: bold;">${item.percentage}%</span></div>
              </div>
            </div>
          `
        }
        return `${params.name}: ${params.value}人 (${params.percent}%)`
      },
    },
    legend: {
      type: 'scroll',
      orient: 'vertical',
      right: 20,
      bottom: 150,
      top: 'center',
      pageTextStyle: {
        color: '#fff',
      },
      pageIconColor: '#fff',
      pageIconInactiveColor: '#666',
      itemWidth: 12,
      itemHeight: 12,
      itemGap: 15,
      selected: legendSelected.value, // 设置图例选中状态
      textStyle: {
        color: '#9AA5BEFF',
        fontSize: 12,
        rich: {
          name: {
            color: '#9AA5BEFF',
            width: 80, // 5-6个字的占位
            align: 'left',
          },
          countValue: {
            color: '#fff',
            fontWeight: 'normal',
          },
          countUnit: {
            color: '#9AA5BEFF',
            fontWeight: 'normal',
          },
          percentValue: {
            color: '#fff',
            fontWeight: 'normal',
          },
          percentUnit: {
            color: '#9AA5BEFF',
            fontWeight: 'normal',
          },
        },
      },
      formatter: (name: string) => {
        const item = dataWithPercentage.value.find(item => item.name === name)
        if (item) {
          return `{name|${name}} {countValue|${item.count}} {countUnit|人}  {percentValue|${item.percentage}} {percentUnit|%}`
        }
        return name
      },
    },
    series: [
      {
        name: 'mainPie',
        type: 'pie',
        radius: ['85%', '100%'],
        center: [70, '50%'],
        padAngle: activeDataCount.value > 1 ? 5 : 0, // 只有一个数据时，不需要间隔
        data: dataWithPercentage.value
          .filter(item => item.count > 0)
          .map(item => ({
            value: item.count,
            name: item.name,
            itemStyle: {
              color: createGradient(item.color),
            },
          })),
        itemStyle: {
          borderRadius: 2,
        },
        label: { show: false },
        emphasis: { disabled: true },
      },
      // 辅助系列：包含所有数据，用于强制显示图例，但通过设置 opacity: 0 隐藏实际渲染
      {
        name: 'ghostPie',
        type: 'pie',
        radius: ['0%', '0%'], // 极小半径
        center: [70, '50%'],
        label: { show: false },
        emphasis: { disabled: true },
        data: dataWithPercentage.value.map(item => ({
          value: item.count, // 即使是0也传入，保证图例存在
          name: item.name,
          itemStyle: {
            color: createGradient(item.color), // 保持颜色一致
          },
        })),
      },
    ],
  }
  chart.setOption(option)
}

onMounted(() => {
  if (!chartRef.value)
    return

  chart = echarts.init(chartRef.value)

  // 初始化图例选中状态（所有项目默认选中）
  legendSelected.value = props.data.reduce((acc, item) => {
    acc[item.name] = true
    return acc
  }, {} as Record<string, boolean>)

  updateChart()

  // 监听图例选择变化
  chart.on('legendselectchanged', (params: any) => {
    legendSelected.value = { ...params.selected }
    updateChart() // 重新渲染图表以更新中心数字
  })

  // 监听数据变化并更新图表
  watch(() => dataWithPercentage.value, () => {
    // 当数据变化时，重新初始化图例选中状态
    legendSelected.value = props.data.reduce((acc, item) => {
      acc[item.name] = true
      return acc
    }, {} as Record<string, boolean>)
    updateChart()
  }, { deep: true })

  // 监听选中状态变化并更新图表
  watch(() => selectedCount.value, updateChart)
})

// 组件卸载时清理图表资源
onBeforeUnmount(() => {
  if (chart) {
    chart.dispose()
    chart = null
  }
})

// 暴露更新方法给父组件
defineExpose({
  updateChart,
})
</script>

<template>
  <div ref="chartRef" class="h-[160px] w-[360px] pt-[35px]" />
</template>
