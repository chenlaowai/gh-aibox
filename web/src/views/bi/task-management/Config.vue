<script setup lang="tsx">
import { computed, nextTick, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { NButton, NPopconfirm } from 'naive-ui'
import * as fabric from 'fabric'
import PreviewCard from './components/PreviewCard.vue'
import { useLoading, useNaiveForm } from '@/hooks'
import { useResponsiveHeight } from '@/hooks/useResponsiveHeight'
import { useAuthStore } from '@/store/auth'
import { triggerAiTaskReload } from '@/services/aiTask'

const authStore = useAuthStore()

enum RegionTypeEnum {
  AUXILIARY = 0,
  INTEREST_REGION = 1,
  SPECIAL_REGION = 2,
}

enum ToolTypeEnum {
  AUXILIARY = 0,
  RECTANGLE = 1,
  POLYGON = 2,
}

// 任务编码（用于编辑模式）
const route = useRoute()
const router = useRouter()
const { loading, startLoading, endLoading } = useLoading(false)
const taskId = ref(route.params.id as string || '')
const detail = ref<API.GetTaskAreaResult | null>(null)

// 响应式高度计算 - 适配autofit缩放
const { height: containerHeight } = useResponsiveHeight(124)

// Canvas相关变量
const previewAreaRef = ref<HTMLDivElement>()
const canvasRef = ref<HTMLCanvasElement>()
let canvas: fabric.Canvas | null = null
let previewResizeObserver: ResizeObserver | null = null
const selectedTool = ref<string>('')
const isDrawing = ref(false)
let currentShape: fabric.Object | null = null
const imageSize = ref({ width: 1440, height: 810 })
const previewFrameSize = ref({ width: 1440, height: 810 })
// 扩展 API.AreaRule 类型，添加 rawPoints 字段
type ExtendedAreaRule = API.AreaRule & {
  rawPoints?: Array<{ x: number, y: number }>
}

const previewFrameStyle = computed(() => ({
  width: `${previewFrameSize.value.width}px`,
  height: `${previewFrameSize.value.height}px`,
}))

// 鼠标跟随提示相关
const cursorTip = ref('')
const cursorPosition = ref({ x: 0, y: 0 })
const showCursorTip = ref(false)

const drawingHistory = ref<ExtendedAreaRule[]>([])
const polygonPoints = ref<Array<{ x: number, y: number }>>([])
const auxiliaryPoints = ref<Array<{ x: number, y: number }>>([])
const rectangleStartPoint = ref<{ x: number, y: number } | null>(null)
// 选中图形的ID
const selectedDrawingId = ref<string>('')

// 预览图形相关变量
let previewShape: fabric.Object | null = null

// 渲染函数
const renderSelectTag = ({ option }: any) => <span>{option.label}</span>

// 表格列定义
const tableColumns = computed(() => [
  { title: '标识', key: 'name', render: (row: ExtendedAreaRule) => (
    <NButton onClick={() => selectDrawing(row)} text type="info">
      {row.name || ''}
    </NButton>
  ) },
  { title: '区域类型', key: 'regionType', render: (row: ExtendedAreaRule) => (
    <n-tag type="warning">
      {getTypeDisplayName(row.regionType || RegionTypeEnum.AUXILIARY)}
    </n-tag>
  ) },
  { title: '关联算法', key: 'algoModeId', render: (row: ExtendedAreaRule) => (
    <n-space>
      {
        row.algoModeId?.map(id => (
          <n-tag type="primary">
            {getAlgoModeDisplayName(id || '')}
          </n-tag>
        ))
      }
    </n-space>
  ) },
  {
    title: '操作',
    key: 'actions',
    width: 120,
    render: (row: ExtendedAreaRule) => (
      <div class="flex gap-2">
        <NPopconfirm
          onPositiveClick={() => deleteDrawing(row.id || '')}
          positiveText="确认"
          negativeText="取消"
          v-slots={{
            trigger: () => (
              <NButton quaternary size="small" type="error">
                删除
              </NButton>
            ),
            default: () => '确定删除这个区域吗?',
          }}
        />
      </div>
    ),
  },
])

// 表单弹窗相关
const showFormDialog = ref(false)
const currentDrawingData = ref<ExtendedAreaRule | null>(null)
const formData = ref<{ name: string, regionType: RegionTypeEnum | null, algoModeId?: string[] }>({ name: '', regionType: null, algoModeId: undefined })
const { formRef, validate } = useNaiveForm()

// 跟踪当前编辑的图形是否有修改
const hasModifications = ref(false)
// 备份原始数据用于还原
const originalDrawingData = ref<ExtendedAreaRule | null>(null)

// 监听区域类型变化，清除关联算法选择
watch(() => formData.value.regionType, (newValue, oldValue) => {
  if (newValue !== oldValue && oldValue !== null) {
    formData.value.algoModeId = []
  }
})

// 列表抽屉相关
const showDrawer = ref(false)

// 工具类型枚举
const TOOLS = {
  AUXILIARY: '辅助线',
  RECTANGLE: '矩形',
  POLYGON: '多边形',
  LIST: '列表',
}

// 关联算法类型选项
const algoModeOptions = computed(() => {
  // zoneRequired: 是否只适用范围区域（即辅助线不能用该算法）
  // majorOnly：是否为专用算法

  // 优先判断编辑模式下的类型
  let type: ToolTypeEnum | undefined
  if (currentDrawingData.value) {
    type = currentDrawingData.value.ruleType
  }
  else if (selectedTool.value) {
    type = selectedTool.value as unknown as ToolTypeEnum
  }

  if (type === ToolTypeEnum.AUXILIARY) {
    return detail.value?.algoModes?.filter(item => !item.zoneRequired && !item.majorOnly) || []
  }
  else if (type === ToolTypeEnum.RECTANGLE || type === ToolTypeEnum.POLYGON) {
    return detail.value?.algoModes?.filter(item => formData.value.regionType === RegionTypeEnum.SPECIAL_REGION ? item.majorOnly : !item.majorOnly) || []
  }
  return []
})

// 区域类型选项
const regionTypeOptions = computed(() => {
  // 优先判断编辑模式下的类型
  let type: ToolTypeEnum | undefined
  if (currentDrawingData.value) {
    type = currentDrawingData.value.ruleType
  }
  else if (selectedTool.value) {
    type = selectedTool.value as unknown as ToolTypeEnum
  }

  if (type === ToolTypeEnum.AUXILIARY) {
    return [{
      label: '辅助线',
      remark: '只对特定算法有效，勾选后关联算法将只显示需要设定辅助线的算法',
      value: RegionTypeEnum.AUXILIARY,
    }]
  }
  else if (type === ToolTypeEnum.RECTANGLE || type === ToolTypeEnum.POLYGON) {
    return [{
      label: '兴趣区域',
      remark: '适用所有算法，关联算法后只检测区域内目标',
      value: RegionTypeEnum.INTEREST_REGION,
    }, {
      label: '专用算法区域',
      remark: '只对特定算法有效，勾选后关联算法将只显示需要设定该区域的算法',
      value: RegionTypeEnum.SPECIAL_REGION,
    }]
  }
  return []
})

// 尝试取消选中（如果有修改则弹出表单）
function tryCancelSelection() {
  if (selectedDrawingId.value && hasModifications.value) {
    const drawing = drawingHistory.value.find(item => item.id === selectedDrawingId.value)
    if (drawing) {
      currentDrawingData.value = drawing
      formData.value = {
        name: drawing.name!,
        regionType: drawing.regionType!,
        algoModeId: drawing.algoModeId,
      }
      showFormDialog.value = true
      return false
    }
  }

  // 否则直接清除编辑状态
  clearEditingState()
  return true
}

// 工具选择
function selectTool(tool: string) {
  // 如果当前有正在编辑且未保存的图形，阻止切换并弹出表单
  if (selectedDrawingId.value && hasModifications.value) {
    tryCancelSelection()
    return
  }

  // 如果点击的是列表工具，显示抽屉
  if (tool === TOOLS.LIST) {
    showDrawer.value = true
    return
  }

  // 如果点击的是当前选中的工具，则取消选中
  if (selectedTool.value === tool) {
    selectedTool.value = ''
    if (canvas) {
      canvas.selection = false
      canvas.defaultCursor = 'default'
    }
    // 清除所有绘制状态
    clearDrawingStates()
    console.log('工具已取消选中')
    return
  }

  selectedTool.value = tool
  if (canvas) {
    canvas.selection = false
    canvas.defaultCursor = 'crosshair'
  }

  // 清除所有绘制状态
  clearDrawingStates()
  // 清除临时绘制图形
  clearTemporaryDrawings()
  // 清除编辑状态（如果有）
  clearEditingState()

  console.log('已选择工具:', tool)

  // 初始化提示语
  if (selectedTool.value === TOOLS.AUXILIARY) {
    cursorTip.value = '单击确定起点'
  }
  else if (selectedTool.value === TOOLS.RECTANGLE) {
    cursorTip.value = '按下确定起点'
  }
  else if (selectedTool.value === TOOLS.POLYGON) {
    cursorTip.value = '按下确定起点'
  }
}

// 更新光标提示信息
function updateCursorTip(pointer: { x: number, y: number }, target?: fabric.Object | null) {
  // 编辑模式逻辑
  if (selectedDrawingId.value) {
    if (target && ((target as any).id === selectedDrawingId.value || (target as any).name === 'control_point')) {
      cursorTip.value = '按下移动图形'
    }
    else {
      cursorTip.value = hasModifications.value ? '单击取消选中并编辑保存' : '单击取消选中'
    }
    showCursorTip.value = true
    return
  }

  if (!selectedTool.value) {
    showCursorTip.value = false
    return
  }

  // 1. 辅助线逻辑
  if (selectedTool.value === TOOLS.AUXILIARY) {
    if (auxiliaryPoints.value.length === 0) {
      cursorTip.value = '单击确定起点'
    }
    else if (auxiliaryPoints.value.length === 1) {
      const startPoint = auxiliaryPoints.value[0]
      // 判断是否移动（允许2像素的误差）
      if (Math.abs(pointer.x - startPoint.x) <= 2 && Math.abs(pointer.y - startPoint.y) <= 2) {
        cursorTip.value = '移动开始绘制'
      }
      else {
        cursorTip.value = '单击确定终点'
      }
    }
  }
  // 2. 矩形逻辑
  else if (selectedTool.value === TOOLS.RECTANGLE) {
    if (!isDrawing.value) {
      cursorTip.value = '按下确定起点'
    }
    else if (rectangleStartPoint.value) {
      const startPoint = rectangleStartPoint.value
      // 判断是否移动（允许2像素的误差）
      if (Math.abs(pointer.x - startPoint.x) <= 2 && Math.abs(pointer.y - startPoint.y) <= 2) {
        cursorTip.value = '移动开始绘制'
      }
      else {
        cursorTip.value = '抬起完成绘制'
      }
    }
  }
  // 3. 多边形逻辑
  else if (selectedTool.value === TOOLS.POLYGON) {
    if (polygonPoints.value.length === 0) {
      cursorTip.value = '按下确定起点'
    }
    else {
      const lastPoint = polygonPoints.value[polygonPoints.value.length - 1]
      const isNear = Math.abs(pointer.x - lastPoint.x) <= 2 && Math.abs(pointer.y - lastPoint.y) <= 2
      if (isNear) {
        if (polygonPoints.value.length === 1) {
          cursorTip.value = '移动开始绘制'
        }
        else {
          cursorTip.value = 'ctrl+z 撤销'
        }
      }
      else {
        if (polygonPoints.value.length === 1) {
          cursorTip.value = '单击绘制'
        }
        else {
          cursorTip.value = '单击绘制/双击结束'
        }
      }
    }
  }
}

// 清除绘制状态
function clearDrawingStates() {
  polygonPoints.value = []
  auxiliaryPoints.value = []
  rectangleStartPoint.value = null
  isDrawing.value = false

  if (currentShape) {
    canvas?.remove(currentShape)
    currentShape = null
  }

  // 清除预览图形
  clearPreviewShape()
}

// 清除预览图形
function clearPreviewShape() {
  if (previewShape && canvas) {
    canvas.remove(previewShape)
    previewShape = null
  }

  // 恢复当前多边形的显示（如果存在）
  if (currentShape && selectedTool.value === TOOLS.POLYGON) {
    currentShape.set({ visible: true })
    canvas?.renderAll()
  }
}

function updatePreviewFrameSize() {
  if (!previewAreaRef.value)
    return

  const previewAreaEl = previewAreaRef.value
  const styles = window.getComputedStyle(previewAreaEl)
  const paddingX = Number.parseFloat(styles.paddingLeft) + Number.parseFloat(styles.paddingRight)
  const paddingY = Number.parseFloat(styles.paddingTop) + Number.parseFloat(styles.paddingBottom)
  const availableWidth = Math.max(previewAreaEl.clientWidth - paddingX, 0)
  const availableHeight = Math.max(previewAreaEl.clientHeight - paddingY, 0)
  const { width, height } = imageSize.value
  if (!availableWidth || !availableHeight || !width || !height)
    return

  // 以父容器剩余的最大宽高为约束边界，按较小缩放比等比适配
  const scale = Math.min(availableWidth / width, availableHeight / height)
  const nextWidth = Math.max(Math.round(width * scale), 1)
  const nextHeight = Math.max(Math.round(height * scale), 1)
  previewFrameSize.value = {
    width: nextWidth,
    height: nextHeight,
  }

  if (canvas) {
    canvas.setDimensions({
      width: nextWidth,
      height: nextHeight,
    })
    renderAllDrawings()
  }
}

function loadImageSize(src?: string) {
  return new Promise<void>((resolve) => {
    if (!src) {
      imageSize.value = { width: 1440, height: 810 }
      updatePreviewFrameSize()
      resolve()
      return
    }

    const img = new Image()
    img.onload = () => {
      imageSize.value = {
        width: img.naturalWidth || 1440,
        height: img.naturalHeight || 810,
      }
      updatePreviewFrameSize()
      resolve()
    }
    img.onerror = () => {
      imageSize.value = { width: 1440, height: 810 }
      updatePreviewFrameSize()
      resolve()
    }
    img.src = src
  })
}

// 初始化Canvas
function initCanvas() {
  if (!canvasRef.value)
    return

  canvas = new fabric.Canvas(canvasRef.value, {
    width: previewFrameSize.value.width,
    height: previewFrameSize.value.height,
    selection: true,
    backgroundColor: 'transparent',
    preserveObjectStacking: true, // 保持对象堆叠顺序，防止选中时遮挡控制点
  })

  // 处理鼠标事件
  canvas.on('mouse:down', handleMouseDown)
  canvas.on('mouse:move', handleMouseMove)
  canvas.on('mouse:up', handleMouseUp)
  canvas.on('mouse:out', handleMouseOut)

  // 添加原生事件监听以控制提示标签显示
  const canvasEl = canvas.upperCanvasEl
  canvasEl.addEventListener('mouseenter', () => {
    if (selectedTool.value || selectedDrawingId.value) {
      showCursorTip.value = true
    }
  })
  canvasEl.addEventListener('mouseleave', () => {
    showCursorTip.value = false
  })

  // 禁用右键菜单
  canvas.on('mouse:down', (e) => {
    if ((e.e as MouseEvent).button === 2) {
      e.e.preventDefault()
      return false
    }
  })

  // 不自动选择工具
}

// 鼠标按下事件
function handleMouseDown(e: fabric.TEvent) {
  // 阻止默认事件和冒泡
  e.e.preventDefault()
  e.e.stopPropagation()

  // 编辑模式下点击空白处取消选中
  if (selectedDrawingId.value) {
    const target = e.target
    const isControlPoint = target && (target as any).name === 'control_point'
    const isSelectedShape = target && (target as any).id === selectedDrawingId.value

    if (!isSelectedShape && !isControlPoint) {
      tryCancelSelection()
      return
    }
  }

  if (!canvas || !selectedTool.value)
    return

  const pointer = canvas.getPointer(e.e)

  if (selectedTool.value === TOOLS.AUXILIARY) {
    // 辅助线 - 点击两个点形成直线
    auxiliaryPoints.value.push({ x: pointer.x, y: pointer.y })

    if (auxiliaryPoints.value.length === 1) {
      // 第一个点，创建临时点标记
      const circle = new fabric.Circle({
        left: pointer.x - 3,
        top: pointer.y - 3,
        radius: 3,
        fill: '#0948ff',
        selectable: false,
        evented: false,
      })
      canvas.add(circle)
      currentShape = circle
    }
    else if (auxiliaryPoints.value.length === 2) {
      // 第二个点，清除预览线和临时点，创建最终直线
      clearPreviewShape()

      if (currentShape) {
        canvas.remove(currentShape)
      }

      const line = new fabric.Line([
        auxiliaryPoints.value[0].x,
        auxiliaryPoints.value[0].y,
        auxiliaryPoints.value[1].x,
        auxiliaryPoints.value[1].y,
      ], {
        stroke: '#0948ff',
        strokeWidth: 2,
        selectable: false,
        evented: false,
      })
      canvas.add(line)
      currentShape = line

      // 完成绘制，准备弹出表单
      completeDrawing(ToolTypeEnum.AUXILIARY, [...auxiliaryPoints.value])
    }
  }
  else if (selectedTool.value === TOOLS.RECTANGLE) {
    // 矩形绘制 - 开始拖拽绘制
    isDrawing.value = true
    rectangleStartPoint.value = { x: pointer.x, y: pointer.y }

    // 清除之前的临时图形
    clearTemporaryDrawings()

    // 创建矩形
    currentShape = new fabric.Rect({
      left: pointer.x,
      top: pointer.y,
      width: 0,
      height: 0,
      fill: 'rgba(9, 72, 255, 0.3)',
      stroke: '#0948ff',
      strokeWidth: 2,
      selectable: false,
      evented: false,
    })
    canvas.add(currentShape)
  }
  else if (selectedTool.value === TOOLS.POLYGON) {
    // 检查是否是双击（通过检查时间间隔）
    const now = Date.now()
    const lastClickTime = (e.e.target as any).lastClickTime || 0
    const isDoubleClick = now - lastClickTime < 200;
    (e.e.target as any).lastClickTime = now

    if (isDoubleClick && polygonPoints.value.length >= 3) {
      // 双击完成多边形绘制，清除预览线
      clearPreviewShape()
      completeDrawing(ToolTypeEnum.POLYGON, [...polygonPoints.value])
      return
    }

    // 多边形绘制 - 每次点击添加一个点
    polygonPoints.value.push({ x: pointer.x, y: pointer.y })

    // 清除预览线（因为要重新绘制多边形）
    clearPreviewShape()

    if (polygonPoints.value.length >= 2) {
      // 更新多边形
      if (currentShape) {
        canvas.remove(currentShape)
      }

      currentShape = new fabric.Polygon([...polygonPoints.value], {
        fill: 'rgba(9, 72, 255, 0.3)',
        stroke: '#0948ff',
        strokeWidth: 2,
        selectable: false,
        evented: false,
      })
      canvas.add(currentShape)
    }
  }
  updateCursorTip(pointer)
}

// 鼠标移动事件
function handleMouseMove(e: fabric.TEvent) {
  // 阻止默认事件和冒泡
  e.e.preventDefault()
  e.e.stopPropagation()

  if (!canvas)
    return

  const pointer = canvas.getPointer(e.e)
  // 更新提示标签位置和内容
  if (selectedTool.value || selectedDrawingId.value) {
    const event = e.e as MouseEvent
    cursorPosition.value = { x: event.clientX + 5, y: event.clientY + 5 }

    updateCursorTip(pointer, e.target)
  }

  // 矩形绘制时实时更新矩形大小
  if (selectedTool.value === TOOLS.RECTANGLE && isDrawing.value && rectangleStartPoint.value && currentShape instanceof fabric.Rect) {
    const rect = currentShape as fabric.Rect

    const left = Math.min(rectangleStartPoint.value.x, pointer.x)
    const top = Math.min(rectangleStartPoint.value.y, pointer.y)
    const width = Math.abs(pointer.x - rectangleStartPoint.value.x)
    const height = Math.abs(pointer.y - rectangleStartPoint.value.y)

    rect.set({
      left,
      top,
      width,
      height,
    })
    canvas.renderAll()
  }
  // 辅助线工具 - 第一个点已点击，显示预览线（实线，因为预览点击后的最终效果）
  else if (selectedTool.value === TOOLS.AUXILIARY && auxiliaryPoints.value.length === 1) {
    // 清除之前的预览线
    clearPreviewShape()

    // 创建预览线（实线样式，模拟点击后的最终效果）
    previewShape = new fabric.Line([
      auxiliaryPoints.value[0].x,
      auxiliaryPoints.value[0].y,
      pointer.x,
      pointer.y,
    ], {
      stroke: '#0948ff',
      strokeWidth: 2,
      selectable: false,
      evented: false,
      opacity: 0.8, // 稍微透明以区分是预览
    })
    canvas.add(previewShape)
    canvas.renderAll()
  }
  // 多边形工具 - 已有点位，显示预览多边形区域（包含鼠标位置作为下一个点）
  else if (selectedTool.value === TOOLS.POLYGON && polygonPoints.value.length >= 1) {
    // 清除之前的预览图形
    clearPreviewShape()

    // 在预览时隐藏当前的部分多边形
    if (currentShape) {
      currentShape.set({ visible: false })
    }

    // 创建包含鼠标位置的预览多边形点数组
    const previewPoints = [...polygonPoints.value, { x: pointer.x, y: pointer.y }]

    if (previewPoints.length >= 2) {
      // 如果只有2个点，显示预览线段
      if (previewPoints.length === 2) {
        previewShape = new fabric.Line([
          previewPoints[0].x,
          previewPoints[0].y,
          previewPoints[1].x,
          previewPoints[1].y,
        ], {
          stroke: '#0948ff',
          strokeWidth: 2,
          selectable: false,
          evented: false,
          opacity: 0.8,
        })
      }
      else {
        // 3个或以上点，显示预览多边形区域
        previewShape = new fabric.Polygon(previewPoints, {
          fill: 'rgba(9, 72, 255, 0.2)', // 更浅的填充，表示预览
          stroke: '#0948ff',
          strokeWidth: 2,
          selectable: false,
          evented: false,
          opacity: 0.8,
        })
      }
      canvas.add(previewShape)
      canvas.renderAll()
    }
  }
}

// 鼠标离开画布事件
function handleMouseOut(_e: fabric.TEvent) {
  // 清除预览图形，恢复部分多边形显示
  clearPreviewShape()
  showCursorTip.value = false
}

// 鼠标释放事件
function handleMouseUp(e: fabric.TEvent) {
  // 阻止默认事件和冒泡
  e.e.preventDefault()
  e.e.stopPropagation()

  if (!canvas)
    return

  // 矩形绘制完成
  if (selectedTool.value === TOOLS.RECTANGLE && isDrawing.value && rectangleStartPoint.value && currentShape instanceof fabric.Rect) {
    const pointer = canvas.getPointer(e.e)

    // 检查是否有有效的矩形大小
    if (Math.abs(pointer.x - rectangleStartPoint.value.x) < 5 || Math.abs(pointer.y - rectangleStartPoint.value.y) < 5) {
      // 如果矩形太小，取消绘制
      canvas.remove(currentShape)
      clearDrawingStates()
      updateCursorTip(pointer)
      return
    }

    // 计算矩形四个角点
    const rectPoints = [
      { x: Math.min(rectangleStartPoint.value.x, pointer.x), y: Math.min(rectangleStartPoint.value.y, pointer.y) },
      { x: Math.max(rectangleStartPoint.value.x, pointer.x), y: Math.min(rectangleStartPoint.value.y, pointer.y) },
      { x: Math.max(rectangleStartPoint.value.x, pointer.x), y: Math.max(rectangleStartPoint.value.y, pointer.y) },
      { x: Math.min(rectangleStartPoint.value.x, pointer.x), y: Math.max(rectangleStartPoint.value.y, pointer.y) },
    ]

    // 完成绘制，准备弹出表单
    completeDrawing(ToolTypeEnum.RECTANGLE, rectPoints)
  }
}

// 完成绘制
function completeDrawing(ruleType: ToolTypeEnum, points: Array<{ x: number, y: number }>) {
  // 标准化坐标（转为0-1之间的相对坐标）
  const normalizedPoints = points.map(p => ({
    x: p.x / canvas!.width,
    y: p.y / canvas!.height,
  }))

  // 准备当前绘制数据
  currentDrawingData.value = {
    id: Date.now().toString(),
    ruleType,
    points: normalizedPoints,
    rawPoints: points, // 保存原始坐标用于显示
  }

  // 显示表单弹窗
  showFormDialog.value = true

  console.log(`${ruleType}绘制完成:`, currentDrawingData.value)
}

// 表单验证规则
const rules = {
  name: [{ required: true, message: '请输入标识' }, { message: '标识不超过50个字符', trigger: ['input', 'blur'], max: 50 }],
  regionType: [{ required: true, message: '请选择区域类型' }],
  algoModeId: [{ required: true, message: '请选择关联算法' }],
}

// 保存绘制数据
function saveDrawing() {
  validate().then(() => {
    if (!currentDrawingData.value)
      return

    // 判断是新增还是编辑
    const isEditing = drawingHistory.value.some(item => item.id === currentDrawingData.value!.id)

    if (isEditing) {
      // 编辑逻辑
      const index = drawingHistory.value.findIndex(item => item.id === currentDrawingData.value!.id)
      if (index !== -1) {
        drawingHistory.value[index] = {
          ...currentDrawingData.value,
          name: formData.value.name,
          regionType: formData.value.regionType || undefined,
          algoModeId: formData.value.algoModeId,
        }
      }

      // 更新完成，重置修改状态并清除编辑状态
      hasModifications.value = false
      originalDrawingData.value = null
      clearEditingState()
      showFormDialog.value = false
      window.$message.success('修改已保存')
    }
    else {
      // 新增逻辑
      const drawingData: ExtendedAreaRule = {
        ...currentDrawingData.value,
        name: formData.value.name,
        regionType: formData.value.regionType || undefined,
        algoModeId: formData.value.algoModeId,
      }

      drawingHistory.value.push(drawingData)

      // 确保当前绘制的图形被标记为已保存，不会被清除
      if (currentShape && canvas) {
      // 给图形添加标识，标记为已保存的图形
        currentShape.set({
          id: drawingData.id,
          saved: true,
          selectable: false,
          evented: false,
        })
      }

      // 重置状态，但不清除已保存的图形
      showFormDialog.value = false
      formData.value = { name: '', regionType: null, algoModeId: [] }
      currentDrawingData.value = null

      // 重置绘制状态，但保留已保存的图形
      polygonPoints.value = []
      auxiliaryPoints.value = []
      rectangleStartPoint.value = null
      isDrawing.value = false
      currentShape = null // 清除当前形状引用，因为已经保存

      window.$message.success('绘制区域保存成功')
    }
  })
}

// 取消绘制
function cancelDrawing() {
  // 如果是编辑状态下的取消，需要还原数据
  if (selectedDrawingId.value && hasModifications.value && originalDrawingData.value) {
    // 还原数据
    const index = drawingHistory.value.findIndex(item => item.id === selectedDrawingId.value)
    if (index !== -1) {
      drawingHistory.value[index] = JSON.parse(JSON.stringify(originalDrawingData.value))

      // 还原画布上的图形
      if (canvas) {
        // 移除被修改的图形
        const modifiedShape = canvas.getObjects().find(obj => (obj as any).id === selectedDrawingId.value)
        if (modifiedShape) {
          canvas.remove(modifiedShape)
        }

        // 重新渲染该图形（使用还原后的数据）
        // 这里需要调用 renderAllDrawings 但只针对特定ID可能会比较复杂
        // 简单做法是清除所有并重新渲染，或者手动重建该对象
        // 为了简便，我们清除编辑状态，并触发一次完全重绘
        renderAllDrawings()
      }
    }

    // 清除状态
    hasModifications.value = false
    originalDrawingData.value = null
    clearEditingState()
  }
  else if (!selectedDrawingId.value) {
    // 新增状态下的取消
    if (currentShape && canvas) {
      canvas.remove(currentShape)
    }
    clearDrawingStates()
  }

  showFormDialog.value = false
  formData.value = { name: '', regionType: null, algoModeId: [] }
  currentDrawingData.value = null
}

// 清除临时绘制图形（只清除未保存的临时图形）
function clearTemporaryDrawings() {
  if (!canvas)
    return

  // 清除预览图形
  clearPreviewShape()

  canvas.getObjects().forEach((obj) => {
    // 只移除未保存的图形（没有saved属性或saved为false的图形）
    if (!obj.selectable && !(obj as any).saved) {
      canvas!.remove(obj)
    }
  })
  canvas.renderAll()
}

// 撤回功能
function undoDrawing() {
  if (!canvas)
    return

  // 如果正在绘制中，取消整个当前绘制
  if (isDrawing.value || polygonPoints.value.length > 0 || auxiliaryPoints.value.length > 0) {
    // 移除当前绘制的图形
    if (currentShape) {
      canvas.remove(currentShape)
      currentShape = null
    }

    // 清除所有绘制状态
    clearDrawingStates()
    console.log('已取消当前绘制')
    window.$message.success('已取消当前绘制')
    return
  }

  // 撤销已完成的绘制
  if (drawingHistory.value.length > 0) {
    const lastDrawing = drawingHistory.value.pop()
    // 从canvas中移除对应的图形
    const objectToRemove = canvas.getObjects().find(obj => (obj as any).id === lastDrawing?.id)
    if (objectToRemove) {
      canvas.remove(objectToRemove)
    }
    console.log('撤销绘制完成:', lastDrawing)
    window.$message.success('撤销成功')
  }
}

// 选中图形
function selectDrawing(row: ExtendedAreaRule) {
  if (!canvas || !row.id)
    return

  // 停止当前绘制
  if (isDrawing.value || selectedTool.value) {
    selectTool('')
  }

  // 清除之前的编辑状态
  clearEditingState()

  selectedDrawingId.value = row.id

  // 查找对应的图形
  const shape = canvas.getObjects().find(obj => (obj as any).id === row.id)

  // 备份原始数据，并重置修改状态
  hasModifications.value = false
  // 深拷贝当前图形数据作为备份
  const currentDrawing = drawingHistory.value.find(item => item.id === row.id)
  if (currentDrawing) {
    originalDrawingData.value = JSON.parse(JSON.stringify(currentDrawing))
  }

  if (shape) {
    // 高亮并允许选中
    shape.set({
      stroke: '#ff0000',
      strokeWidth: 2,
      selectable: true,
      evented: true,
      hasControls: row.ruleType === ToolTypeEnum.RECTANGLE, // 矩形使用默认控制点
      lockRotation: true, // 锁定旋转
      perPixelTargetFind: row.ruleType === ToolTypeEnum.POLYGON, // 多边形基于像素检测点击，而非包围盒
      objectCaching: false, // 关闭缓存以确保实时更新准确
    })

    // 如果是矩形，允许缩放，但不允许旋转
    if (row.ruleType === ToolTypeEnum.RECTANGLE) {
      shape.set({
        transparentCorners: false,
        cornerColor: '#ffffff',
        cornerStrokeColor: '#ff0000',
        borderColor: '#ff0000',
        cornerSize: 10,
        padding: 5,
        cornerStyle: 'circle',
      })
    }

    // 将选中图形置于顶层，防止被其他图形遮挡
    if (canvas) {
      canvas.bringObjectToFront(shape)
    }

    canvas.setActiveObject(shape)
    canvas.renderAll()

    // 开启编辑模式（显示顶点）
    enableEditingMode(shape, row.ruleType!)
  }
}

// 开启编辑模式
function enableEditingMode(shape: fabric.Object, ruleType: ToolTypeEnum) {
  if (!canvas)
    return

  // 移除旧的事件监听，避免重复
  shape.off('modified')
  shape.off('moving')

  // 绑定移动事件，同步更新数据
  shape.on('modified', () => {
    updateDrawingData(shape)
    hasModifications.value = true // 标记为已修改
  })

  // 图形移动时同步移动控制点
  shape.on('moving', () => {
    updateControlPoints(shape)
    hasModifications.value = true // 标记为已修改
  })

  // 对于多边形和辅助线，创建自定义控制点
  if (ruleType === ToolTypeEnum.POLYGON || ruleType === ToolTypeEnum.AUXILIARY) {
    // 移除已存在的控制点（如果有）
    const existingControls = canvas.getObjects().filter(obj => (obj as any).name === 'control_point' && (obj as any).data?.shapeId === (shape as any).id)
    existingControls.forEach(c => canvas!.remove(c))

    const points = (shape as any).points || []
    const matrix = shape.calcTransformMatrix()

    // 辅助线是Line对象，没有points属性，需要特殊处理
    if (ruleType === ToolTypeEnum.AUXILIARY && shape instanceof fabric.Line) {
      const line = shape as fabric.Line
      // 创建起终点控制点
      createControlPoint(line.x1!, line.y1!, shape, 0, 'x1', 'y1')
      createControlPoint(line.x2!, line.y2!, shape, 1, 'x2', 'y2')
    }
    else if (ruleType === ToolTypeEnum.POLYGON && shape instanceof fabric.Polygon) {
      const polygon = shape as fabric.Polygon
      // 为每个顶点创建控制点
      // 注意：Polygon的points是相对于中心点的，需要转换
      const points = polygon.points || []
      points.forEach((point, index) => {
        // 计算绝对坐标
        const pointInCanvas = fabric.util.transformPoint(
          new fabric.Point(point.x - polygon.pathOffset.x, point.y - polygon.pathOffset.y),
          matrix,
        )
        createControlPoint(pointInCanvas.x, pointInCanvas.y, shape, index)
      })
    }
  }
}

// 创建控制点
function createControlPoint(x: number, y: number, shape: fabric.Object, index: number, propX?: string, propY?: string) {
  const circle = new fabric.Circle({
    left: x - 5,
    top: y - 5,
    radius: 5,
    fill: '#ffffff',
    stroke: '#ff0000',
    strokeWidth: 2,
    hasControls: false,
    hasBorders: false,
    selectable: true,
    evented: true,
    originX: 'left',
    originY: 'top',
    name: 'control_point',
    data: { shapeId: (shape as any).id, index, propX, propY }, // 存储关联信息
    padding: 5, // 扩大点击热区
    hoverCursor: 'nesw-resize', // 鼠标悬停样式
  } as any)

  canvas?.add(circle)
  if (canvas) {
    canvas.bringObjectToFront(circle) // 确保在最上层（必须在preserveObjectStacking=true时配合使用）
  }

  // 拖拽控制点事件
  circle.on('moving', (e: any) => {
    hasModifications.value = true // 标记为已修改
    const p = e.pointer
    const newPoint = { x: p.x, y: p.y }

    if (shape instanceof fabric.Polygon) {
      // 更新多边形顶点
      // 需要将画布坐标转换为多边形内部坐标
      const polygon = shape as fabric.Polygon
      const matrix = polygon.calcTransformMatrix()
      const inverseMatrix = fabric.util.invertTransform(matrix)
      const pointInObject = fabric.util.transformPoint(new fabric.Point(newPoint.x, newPoint.y), inverseMatrix)

      // 更新对应点
      if (polygon.points) {
        polygon.points[index] = {
          x: pointInObject.x + polygon.pathOffset.x,
          y: pointInObject.y + polygon.pathOffset.y,
        }
        // 标记需要更新
        polygon.dirty = true
        // 重新计算尺寸，否则拖拽后包围盒不正确
        // 注意：fabric中修改points后需要重新初始化或手动处理pathOffset等，比较复杂
        // 这里简化处理：直接更新，但可能导致包围盒不准，更严谨的做法是销毁重建或使用setPositionByOrigin
      }
    }
    else if (shape instanceof fabric.Line) {
      // 更新直线端点
      (shape as any).set({
        [propX!]: newPoint.x,
        [propY!]: newPoint.y,
      })
    }

    shape.setCoords()
    canvas?.renderAll()
  })

  // 控制点修改完成，同步数据
  circle.on('modified', () => {
    // 对于Polygon，修改points后通常需要重新计算尺寸和中心点
    if (shape instanceof fabric.Polygon) {
      const polygon = shape as fabric.Polygon
      if (polygon._calcDimensions) {
        polygon._calcDimensions()
      }

      polygon.setCoords()
      updateDrawingData(shape)
    }
    else {
      updateDrawingData(shape)
    }
  })
}

// 更新控制点位置
function updateControlPoints(shape: fabric.Object) {
  if (!canvas)
    return

  const matrix = shape.calcTransformMatrix()
  const controls = canvas.getObjects().filter(obj => (obj as any).name === 'control_point' && (obj as any).data?.shapeId === (shape as any).id)

  controls.forEach((control) => {
    const data = (control as any).data
    let newPos

    if (shape instanceof fabric.Line) {
      const line = shape as fabric.Line
      if (data.index === 0)
        newPos = { x: line.x1!, y: line.y1! }
      else newPos = { x: line.x2!, y: line.y2! }
    }
    else if (shape instanceof fabric.Polygon) {
      const polygon = shape as fabric.Polygon
      const point = polygon.points![data.index]
      newPos = fabric.util.transformPoint(
        new fabric.Point(point.x - polygon.pathOffset.x, point.y - polygon.pathOffset.y),
        matrix,
      )
    }

    if (newPos) {
      control.set({
        left: newPos.x - 5,
        top: newPos.y - 5,
      })
      control.setCoords()
    }
  })
}

// 清除编辑状态
function clearEditingState() {
  if (!canvas)
    return

  // 移除所有控制点
  const controls = canvas.getObjects().filter(obj => (obj as any).name === 'control_point')
  controls.forEach(c => canvas!.remove(c))

  // 重置所有图形样式
  canvas.getObjects().forEach((obj) => {
    if ((obj as any).saved) {
      obj.set({
        stroke: '#0948ff',
        strokeWidth: 2,
        selectable: false,
        evented: false,
        hasControls: false,
        hasBorders: false,
      })
    }
  })

  selectedDrawingId.value = ''
  canvas.discardActiveObject()
  canvas.renderAll()
}

// 更新绘制数据到历史记录
function updateDrawingData(shape: fabric.Object) {
  const id = (shape as any).id
  const index = drawingHistory.value.findIndex(item => item.id === id)

  if (index !== -1) {
    // 计算新的绝对坐标点
    let rawPoints: Array<{ x: number, y: number }> = []

    if (shape instanceof fabric.Line) {
      const line = shape as fabric.Line
      rawPoints = [{ x: line.x1!, y: line.y1! }, { x: line.x2!, y: line.y2! }]
    }
    else if (shape instanceof fabric.Rect) {
      // 矩形转换为4个点
      const rect = shape as fabric.Rect
      // 获取矩形的四个角坐标（考虑旋转和缩放，虽然我们锁定了旋转）
      const matrix = rect.calcTransformMatrix()
      const w = rect.width!
      const h = rect.height!
      // 矩形原点通常在左上角
      const p1 = fabric.util.transformPoint(new fabric.Point(-w / 2, -h / 2), matrix) // 如果origin是center
      // 但fabric默认origin是left,top。这里我们简单处理，直接获取aCoords
      const aCoords = shape.aCoords as any
      rawPoints = [aCoords.tl, aCoords.tr, aCoords.br, aCoords.bl]
    }
    else if (shape instanceof fabric.Polygon) {
      const polygon = shape as fabric.Polygon
      const matrix = polygon.calcTransformMatrix()
      rawPoints = polygon.points!.map(p => fabric.util.transformPoint(
        new fabric.Point(p.x - polygon.pathOffset.x, p.y - polygon.pathOffset.y),
        matrix,
      ))
    }

    // 更新历史记录
    drawingHistory.value[index].rawPoints = rawPoints
    drawingHistory.value[index].points = rawPoints.map(p => ({
      x: p.x / canvas!.width!,
      y: p.y / canvas!.height!,
    }))

    hasModifications.value = true // 标记为已修改
    console.log('更新图形数据:', drawingHistory.value[index])
  }
}

// 删除指定绘制项
function deleteDrawing(id: string) {
  // 如果删除的是当前选中的，先清除编辑状态
  if (selectedDrawingId.value === id) {
    clearEditingState()
  }

  const index = drawingHistory.value.findIndex(item => item.id === id)
  if (index !== -1) {
    // 从canvas中删除对应的图形
    if (canvas) {
      const objectToRemove = canvas.getObjects().find(obj => (obj as any).id === id)
      if (objectToRemove) {
        canvas.remove(objectToRemove)
      }
    }

    // 从历史记录中删除
    drawingHistory.value.splice(index, 1)
    window.$message.success('删除成功')
  }
}

// 渲染所有绘制内容
function renderAllDrawings() {
  if (!canvas)
    return

  // 清除所有已保存的图形，准备重新绘制
  canvas.getObjects().forEach((obj) => {
    if ((obj as any).saved) {
      canvas!.remove(obj)
    }
  })

  // 重新绘制所有保存的图形
  drawingHistory.value.forEach((drawing) => {
    const points = drawing.points?.map(p => ({
      x: p.x! * canvas!.width,
      y: p.y! * canvas!.height,
    }))

    if (!points || points.length === 0) {
      console.warn('Drawing has no points:', drawing)
      return
    }

    let shape: fabric.Object

    if (drawing.ruleType === ToolTypeEnum.AUXILIARY) {
      shape = new fabric.Line([
        points[0].x,
        points[0].y,
        points[1].x,
        points[1].y,
      ], {
        stroke: '#0948ff',
        strokeWidth: 2,
        selectable: false,
        evented: false,
      })
    }
    else if (drawing.ruleType === ToolTypeEnum.RECTANGLE) {
      shape = new fabric.Rect({
        left: points[0].x,
        top: points[0].y,
        width: points[2].x - points[0].x,
        height: points[2].y - points[0].y,
        fill: 'rgba(9, 72, 255, 0.3)',
        stroke: '#0948ff',
        strokeWidth: 2,
        selectable: false,
        evented: false,
      })
    }
    else if (drawing.ruleType === ToolTypeEnum.POLYGON) {
      shape = new fabric.Polygon(points, {
        fill: 'rgba(9, 72, 255, 0.3)',
        stroke: '#0948ff',
        strokeWidth: 2,
        selectable: false,
        evented: false,
      })
    }
    else {
      return
    }

    // 标记为已保存的图形
    shape.set({
      id: drawing.id,
      saved: true,
    })

    canvas!.add(shape)
  })
}

function handleCardClick() {
  // 如果正在绘制中，不触发点击事件
  if (isDrawing.value)
    return
  console.log('卡片点击')
}

// 获取区域类型显示名称
function getTypeDisplayName(regionType: RegionTypeEnum | number) {
  const typeMap: Record<number, string> = {
    [RegionTypeEnum.AUXILIARY]: '辅助线',
    [RegionTypeEnum.INTEREST_REGION]: '兴趣区域',
    [RegionTypeEnum.SPECIAL_REGION]: '专用算法区域',
  }
  return typeMap[regionType as number] || '未知类型'
}
// 获算法类型显示名称
function getAlgoModeDisplayName(algoModeId: string) {
  return detail.value?.algoModes?.find(item => item.id === algoModeId)?.name || ''
}

// 保存所有绘制数据
async function saveAllDrawings() {
  try {
    startLoading()
    const apiData = getDrawingsForAPI()
    const [error, result] = await utils.tryit(api.task.postTaskAddTaskAreas)({
      taskId: taskId.value,
      areaRules: apiData as unknown as API.AddAreaRule[],
    })
    if (error || !result.isSuccess) {
      window.$message.error('保存失败，请重试')
      throw error
    }
    window.$message.success('保存成功')
    void triggerAiTaskReload(taskId.value, authStore.rawAccessToken)
    router.push({ name: 'taskManagement' })
  }
  catch (error) {
    console.error('保存失败:', error)
    window.$message.error('保存失败，请重试')
  }
  finally {
    endLoading()
  }
}

// 加载已保存的绘制数据
async function loadDetail() {
  try {
    const [error, r1] = await utils.tryit(api.task.getTaskGetTaskAreasTaskId)({
      taskId: taskId.value,
    })
    if (error || !r1.isSuccess)
      return

    detail.value = r1.data || null
    await loadImageSize(r1.data.photo)
    restoreDrawingsFromAPI(r1.data.areaRules || [])
  }
  catch (error) {
    console.error('加载绘制数据失败:', error)
  }
}

// 返回任务管理列表
function goBack() {
  router.push({ name: 'taskManagement' })
}

// 通过接口数据恢复绘制图形
function restoreDrawingsFromAPI(apiData: API.AreaRule[]) {
  if (!canvas) {
    console.error('Canvas未初始化')
    return
  }

  // 清除当前所有绘制
  canvas.clear()

  // 重置绘制历史
  drawingHistory.value = []

  // 处理API返回的数据
  apiData.forEach((item) => {
    // 转换API数据格式为ExtendedAreaRule
    const drawingData: ExtendedAreaRule = {
      id: item.id,
      ruleType: item.ruleType,
      name: item.name,
      regionType: item.regionType,
      algoModeId: item.algoModeId,
      points: item.points, // API返回的应该是相对坐标(0-1)
      rawPoints: item.points?.map(p => ({
        x: p.x! * canvas!.width,
        y: p.y! * canvas!.height,
      })) || [], // 转换为绝对坐标用于绘制
    }

    // 添加到绘制历史
    drawingHistory.value.push(drawingData)
  })

  // 重新渲染所有图形
  renderAllDrawings()

  console.log('从API恢复绘制数据:', apiData)
}

// 获取当前所有绘制数据（用于提交给API）
function getDrawingsForAPI(): API.AreaRule[] {
  return drawingHistory.value.map(drawing => ({
    regionType: drawing.regionType,
    name: drawing.name,
    ruleType: drawing.ruleType,
    algoModeId: drawing.algoModeId,
    points: drawing.points, // 相对坐标(0-1)
  }))
}

// 键盘事件处理
function handleKeyDown(e: KeyboardEvent) {
  if (!canvas)
    return

  // Esc 取消选中
  if (e.key === 'Escape') {
    // 如果正在绘制，取消绘制
    if (isDrawing.value || polygonPoints.value.length > 0 || auxiliaryPoints.value.length > 0) {
      e.preventDefault()
      undoDrawing() // 复用撤销逻辑来清理未完成的绘制
      return
    }

    // 如果处于编辑状态，尝试取消选中
    if (selectedDrawingId.value) {
      e.preventDefault()
      tryCancelSelection()
      return
    }
  }

  // Ctrl+Z 撤销
  if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'z') {
    e.preventDefault()
    // 多边形绘制过程中的撤销（撤销上一个点）
    if (selectedTool.value === TOOLS.POLYGON && polygonPoints.value.length >= 2) {
      polygonPoints.value.pop()

      // 清除当前多边形
      if (currentShape) {
        canvas.remove(currentShape)
      }

      // 如果剩余点数 >= 2，重新绘制多边形
      if (polygonPoints.value.length >= 2) {
        currentShape = new fabric.Polygon([...polygonPoints.value], {
          fill: 'rgba(9, 72, 255, 0.3)',
          stroke: '#0948ff',
          strokeWidth: 2,
          selectable: false,
          evented: false,
        })
        canvas.add(currentShape)
      }
      else {
        currentShape = null
      }

      // 清除预览线
      clearPreviewShape()

      // 立即触发一次鼠标移动逻辑，以更新预览图形
      if (cursorPosition.value) {
        // 构造一个模拟的鼠标移动事件对象
        const simulatedEvent = {
          e: {
            preventDefault: () => {},
            stopPropagation: () => {},
            clientX: cursorPosition.value.x - 5, // 还原真实坐标
            clientY: cursorPosition.value.y - 5,
          },
        } as any
        handleMouseMove(simulatedEvent)
      }

      canvas.renderAll()
    }
  }
}

// 生命周期
onMounted(async () => {
  await nextTick()
  initCanvas()
  if (previewAreaRef.value) {
    previewResizeObserver = new ResizeObserver(() => {
      updatePreviewFrameSize()
    })
    previewResizeObserver.observe(previewAreaRef.value)
  }
  updatePreviewFrameSize()
  window.addEventListener('keydown', handleKeyDown)
  // 加载已保存的绘制数据
  await loadDetail()
})

onUnmounted(() => {
  window.removeEventListener('keydown', handleKeyDown)
  previewResizeObserver?.disconnect()
  if (canvas) {
    canvas.dispose()
  }
})
</script>

<template>
  <div class="mt-[8px] pl-[74px] pr-[67px]">
    <CardHeaderOther :title="detail?.taskCode || ''">
      <template #header-extra>
        <div class="flex gap-5">
          <CustomButton class="h-[38px] w-[110px]" type="info" @click="goBack">
            取消
          </CustomButton>
          <CustomButton class="h-[38px] w-[110px]" type="warn" @click="undoDrawing">
            撤回
          </CustomButton>
          <CustomButton class="h-[38px] w-[110px]" type="success" @click="saveAllDrawings">
            保存
          </CustomButton>
        </div>
      </template>
      <div class="flex gap-[40px] pt-[24px]" :style="{ height: containerHeight }">
        <!-- 左侧菜单区域 -->
        <div class="ml-[33px] h-full w-[204px] flex flex-col gap-[16px] pr-2">
          <div
            v-for="tool in [TOOLS.AUXILIARY, TOOLS.RECTANGLE, TOOLS.POLYGON, TOOLS.LIST]"
            :key="tool"
            class="tool-item"
            :class="{ 'tool-item-active': selectedTool === tool }"
            @click="selectTool(tool)"
          >
            {{ tool }}
          </div>
        </div>

        <!-- 右侧卡片区域 -->
        <div ref="previewAreaRef" class="relative h-full flex flex-1 items-start justify-center pb-[40px]">
          <div class="relative shrink-0" :style="previewFrameStyle">
            <PreviewCard
              v-if="detail"
              :id="detail.taskId!"
              :title="detail.taskCode || ''"
              :image-url="detail.photo || ''"
              @click="handleCardClick"
            />

            <!-- Canvas 覆盖层 -->
            <canvas
              ref="canvasRef"
              class="pointer-events-auto absolute inset-0 z-20"
            />
          </div>
        </div>
      </div>
    </CardHeaderOther>

    <!-- 表单弹窗 -->
    <n-modal v-model:show="showFormDialog" :mask-closable="false" preset="card" title="绘制信息" class="w-[700px]" @close="cancelDrawing">
      <n-form
        ref="formRef"
        :model="formData" label-placement="left" label-width="80"
        :rules="rules"
      >
        <n-form-item
          label="标识"
          path="name"
        >
          <n-input v-model:value="formData.name" v-trim placeholder="请输入绘制区域标识" />
        </n-form-item>
        <n-form-item
          label="区域类型"
          path="regionType"
        >
          <n-select
            v-model:value="formData.regionType"
            :options="regionTypeOptions"
            placeholder="请选择区域类型"
            :render-label="(option: any) => `${option.label} - ${option.remark}`"
            :render-tag="renderSelectTag"
          />
        </n-form-item>
        <n-form-item
          v-if="formData.regionType !== null"
          label="关联算法"
          path="algoModeId"
        >
          <n-select
            v-model:value="formData.algoModeId"
            label-field="name"
            value-field="id"
            :options="algoModeOptions"
            placeholder="请选择关联算法"
            multiple
            clearable
          />
        </n-form-item>
      </n-form>
      <template #footer>
        <div class="flex justify-end gap-3">
          <NButton @click="cancelDrawing">
            取消
          </NButton>
          <NButton type="primary" @click="saveDrawing">
            保存
          </NButton>
        </div>
      </template>
    </n-modal>

    <!-- 列表抽屉 -->
    <n-drawer v-model:show="showDrawer" :width="600" placement="right">
      <n-drawer-content title="绘制区域列表">
        <n-data-table
          :columns="tableColumns"
          :data="drawingHistory"
          :pagination="false"
          size="small"
        />
      </n-drawer-content>
    </n-drawer>

    <!-- 鼠标跟随提示 -->
    <Teleport to="body">
      <div
        v-if="showCursorTip && (selectedTool || selectedDrawingId) && cursorTip"
        class="pointer-events-none fixed z-[9999] whitespace-nowrap rounded bg-black/60 px-2 py-1 text-xs text-white"
        :style="{
          left: `${cursorPosition.x}px`,
          top: `${cursorPosition.y}px`,
        }"
      >
        {{ cursorTip }}
      </div>
    </Teleport>
  </div>
</template>

<style lang="scss" scoped>
::v-deep(.canvas-container) {
  position: absolute !important;
  top: 0;
}
.tool-item {
  height: 38px;
  width: 110px;
  background-color: #0948ffff;
  clip-path: polygon(10% 0, 90% 0, 100% 50%, 90% 100%, 10% 100%, 0% 50%);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
  line-height: 16px;
  cursor: pointer;
  transition: all 0.3s ease;

  &.tool-item-active {
    background-color: #6f47ff;
  }
}
</style>
