import * as echarts from 'echarts/core'
import {
  BarChart,
  GaugeChart,
  LineChart,
  PieChart,
  RadarChart,
} from 'echarts/charts'

// 系列类型的定义后缀都为 SeriesOption
import type {
  BarSeriesOption,
  GaugeSeriesOption,
  LineSeriesOption,
  PieSeriesOption,
  RadarSeriesOption,
} from 'echarts/charts'

// 组件类型的定义后缀都为 ComponentOption
import type {
  DatasetComponentOption,
  GraphicComponentOption,
  GridComponentOption,
  LegendComponentOption,
  TitleComponentOption,
  ToolboxComponentOption,
  TooltipComponentOption,
} from 'echarts/components'
import {
  DatasetComponent, // 数据集组件
  GridComponent,
  LegendComponent,
  TitleComponent,
  ToolboxComponent,
  TooltipComponent,
  TransformComponent, // 内置数据转换器组件 (filter, sort)
  GraphicComponent,
  MarkLineComponent,
  PolarComponent,
} from 'echarts/components'

import { LabelLayout, UniversalTransition } from 'echarts/features'
import { CanvasRenderer } from 'echarts/renderers'

// 通过 ComposeOption 来组合出一个只有必须组件和图表的 Option 类型
export type ECOption = echarts.ComposeOption<
  | BarSeriesOption
  | PieSeriesOption
  | LineSeriesOption
  | TitleComponentOption
  | TooltipComponentOption
  | GridComponentOption
  | LegendComponentOption
  | DatasetComponentOption
  | ToolboxComponentOption
  | RadarSeriesOption
  | GaugeSeriesOption
  | GraphicComponentOption
>

// 注册必须的组件
echarts.use([
  TitleComponent,
  TooltipComponent,
  GridComponent,
  LegendComponent,
  DatasetComponent,
  TransformComponent,
  BarChart,
  PieChart,
  LineChart,
  LabelLayout,
  UniversalTransition,
  CanvasRenderer,
  ToolboxComponent,
  RadarChart,
  GraphicComponent,
  MarkLineComponent,
  PolarComponent,
  GaugeChart,
])

/**
 * Echarts hooks函数
 * @description 按需引入图表组件，没注册的组件需要先引入
 */
export function useEcharts(
  el: Ref<HTMLElement | null>,
  chartOptions: Ref<ECOption>,
  ready?: (chart: echarts.ECharts) => void,
  legendClickHandler?: (params: any) => void,
) {
  let chart: echarts.ECharts | null = null

  const { width, height } = useElementSize(el)

  const isRendered = computed(() => Boolean(el && chart))

  async function render() {
    // 宽或高不存在时不渲染
    if (!width || !height)
      return

    const chartTheme = 'dark'

    if (el.value) {
      chart = echarts.init(el.value, chartTheme)
      ready && ready(chart)
      update(chartOptions.value)
      // 如果提供了legend点击事件处理函数，则绑定点击事件
      if (legendClickHandler) {
        chart.on('legendselectchanged', legendClickHandler)
      }
    }
  }

  function update(updateOptions: ECOption) {
    if (isRendered.value)
      chart!.setOption({ ...updateOptions, backgroundColor: 'transparent' })
  }

  function destroy() {
    chart?.dispose()
    chart = null
  }

  watch([width, height], async ([newWidth, newHeight]) => {
    // 排查后发现这段代码影响图表首次动画，加个延迟执行
    setTimeout(() => {
      if (isRendered.value && newWidth && newHeight)
        chart?.resize()
    }, 1000)
  })

  watch(chartOptions, update)

  onMounted(render)
  onUnmounted(destroy)

  return {
    destroy,
    update,
  }
}
