<script setup lang="ts">
import { onMounted, ref, watch } from 'vue'
import * as echarts from 'echarts'

interface TaskData {
  categories: string[]
  values: number[]
  percentage: number[]
  maxValue?: number
}

interface Props {
  title?: string
  data?: TaskData
}

const props = withDefaults(defineProps<Props>(), {
  title: '项目',
  data: () => ({
    categories: ['山西项目', '九安项目', '沙特项目', '海外作业', '湛江徐闻'],
    values: [20, 15, 25, 30, 18],
    percentage: [20.00, 15.00, 25.00, 30.00, 18.00],
    maxValue: 40,
  }),
})

const chartRef = ref<HTMLElement>()
let chart: echarts.ECharts | null = null

function updateChart() {
  if (!chart)
    return

  const option = {
    text: `{icon|}{text|${props.title}}`,
    textStyle: {
      color: '#4E5969FF',
      rich: {
        icon: {
          backgroundColor: '#1D73FEFF',
          width: 6,
          height: 6,
          borderRadius: 1,
          padding: [0, 0, 0, 0],
          verticalAlign: 'middle',
        },
        text: {
          color: '#4E5969FF',
          fontSize: 12,
          padding: [0, 0, 0, 10],
          lineHeight: 15,
          verticalAlign: 'bottom',
        },
      },
    },
    grid: { top: 30, right: 10, bottom: 10, left: 10, containLabel: true },
    tooltip: {
      trigger: 'axis',
      backgroundColor: 'rgba(0, 0, 0, 0.8)',
      borderColor: '#1D73FEFF',
      borderWidth: 1,
      textStyle: {
        color: '#FFFFFF',
      },
      formatter(params: any) {
        const param = params[0]
        const rawValue = props.data.percentage[param.dataIndex]
        const percentage = rawValue.toFixed(2) || '0.00'
        return `${param.name}<br/>告警次数：${param.data}<br/>告警占比：${percentage}%`
      },
    },
    xAxis: {
      type: 'category',
      data: props.data.categories,
      axisLabel: {
        color: '#9AA5BEFF',
        fontSize: 12,
        interval: 0,
        rotate: 30,
        formatter: (value: string) => {
          return value.length > 6 ? `${value.slice(0, 6)}...` : value
        },
      },
      axisLine: { lineStyle: { color: 'rgba(255,255,255,0.12)', type: 'solid' } },
    },
    yAxis: {
      type: 'value',
      max: props.data.maxValue || 40,
      axisLabel: { color: '#676F86FF', fontSize: 12 },
      splitLine: { lineStyle: { color: 'rgba(255,255,255,0.12)', type: 'dashed' } },
    },
    series: [
      {
        type: 'bar',
        data: props.data.values,
        itemStyle: {
          color: '#0C5CFFFF',
        },
        barWidth: 15,
      },
    ],
  }
  chart.setOption(option)
}

onMounted(() => {
  if (!chartRef.value)
    return

  chart = echarts.init(chartRef.value)
  updateChart()

  // 监听数据变化并更新图表
  watch(() => props.data, updateChart, { deep: true })
})

// 暴露更新方法给父组件
defineExpose({
  updateChart,
})
</script>

<template>
  <div ref="chartRef" class="h-[216px] w-[370px] pt-[10px]" />
</template>

<style scoped>
.task-chart {
  height: 216px;
}
</style>
