<script setup lang="ts">
import { onMounted, ref, watch } from 'vue'
import * as echarts from 'echarts'

interface UsageData {
  months: string[]
  fullTimes: string[]
  series: { name: string, data: number[] }[]
  maxValue?: number
}

interface Props {
  title?: string
  data?: UsageData
}

const props = withDefaults(defineProps<Props>(), {
  title: 'CORE使用率',
  data: () => ({
    months: [],
    fullTimes: [],
    series: [],
    maxValue: 100,
  }),
})

const chartRef = ref<HTMLElement>()
let chart: echarts.ECharts | null = null

function updateChart() {
  if (!chart)
    return

  const colors = ['#005BFF', '#08CA5A', '#F56501', '#F50102']

  const series = props.data?.series || []
  const seriesList = series.map((item, index) => {
    const color = colors[index % colors.length]
    return {
      name: item.name,
      type: 'line',
      data: item.data,
      lineStyle: { color, width: 2 },
      itemStyle: { color },
      symbol: 'circle',
      symbolSize: 5,
      areaStyle: {
        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
          { offset: 0, color: `${color}80` },
          { offset: 0.36, color: `${color}1A` },
          { offset: 1, color: `${color}00` },
        ]),
      },
    }
  })

  const option = {
    legend: {
      show: true,
      top: 0,
      icon: 'circle',
      itemWidth: 12,
      itemHeight: 12,
      textStyle: {
        color: '#9AA5BEFF',
      },
    },
    grid: { top: 60, right: 20, bottom: 20, left: 30 },
    tooltip: {
      trigger: 'axis',
      backgroundColor: 'rgba(0, 0, 0, 0.8)',
      borderColor: '#1D73FEFF',
      borderWidth: 1,
      textStyle: {
        color: '#FFFFFF',
      },

      formatter(params: any) {
        if (!params || params.length === 0)
          return ''
        let result = `${props.data.fullTimes[params[0].dataIndex] || params[0].name}<br/>`
        params.forEach((param: any) => {
          const value = typeof param.value === 'number' ? param.value.toFixed(2) : '0.00'
          result += `${param.marker} <span style="display:inline-block;width:60px;">${param.seriesName}</span> ${value}%<br/>`
        })
        return result
      },
    },
    xAxis: {
      type: 'category',
      data: props.data.months,
      axisLine: { lineStyle: { color: 'rgba(255,255,255, 0.12)', type: 'solid' } },
      axisLabel: {
        color: '#9AA5BEFF',
        fontSize: 12,
        formatter(value: string, index: number) {
          return (index === 0 || index === 2 || index === 4) ? value : ''
        },
      },
      axisTick: { show: false },
      boundaryGap: false,
    },
    yAxis: {
      type: 'value',
      max: props.data.maxValue || 100,
      axisLine: { show: false },
      axisTick: { show: false },
      axisLabel: { color: '#676F86FF', fontSize: 12 },
      splitLine: { lineStyle: { color: 'rgba(255,255,255,0.12)', type: 'dashed' } },
    },
    series: seriesList,
  }
  chart.setOption(option)
}

onMounted(() => {
  if (!chartRef.value)
    return

  chart = echarts.init(chartRef.value)

  // 监听窗口大小变化
  window.addEventListener('resize', () => chart?.resize())

  updateChart()

  // 监听数据变化并更新图表
  watch(() => props.data, () => {
    updateChart()
  }, { deep: true })
})

// 暴露更新方法给父组件
defineExpose({
  updateChart,
})
</script>

<template>
  <div ref="chartRef" class="h-[220px] w-[344px] pt-[10px]" />
</template>
