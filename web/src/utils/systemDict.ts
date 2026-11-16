import { useAppStore } from '@/store'

/**
 * 系统字典工具函数
 */

/**
 * 根据枚举值获取标签文本
 * @param enumType 枚举类型 ('protocolType' | 'taskStatus' | 'regionType')
 * @param value 枚举值
 * @returns 标签文本
 */
export function getEnumLabel(enumType: 'protocolType' | 'taskStatus' | 'regionType', value: number): string {
  const appStore = useAppStore()

  let enumData: any[] = []
  switch (enumType) {
    case 'protocolType':
      enumData = appStore.protocolTypeEnum
      break
    case 'taskStatus':
      enumData = appStore.taskStatusEnum
      break
    case 'regionType':
      enumData = appStore.regionTypeEnum
      break
  }

  const item = enumData.find(item => item.value === value)
  return item?.label || '未知'
}

/**
 * 根据协议类型值获取完整说明
 * @param value 协议类型值
 * @returns 完整说明文本
 */
export function getProtocolRemark(value: number): string {
  const appStore = useAppStore()
  const item = appStore.protocolTypeEnum.find(item => item.value === value)
  return item?.remark || ''
}

/**
 * 获取任务状态的显示样式
 * @param status 状态值
 * @returns 包含文本和样式类型的对象
 */
export function getTaskStatusDisplay(status: number) {
  const text = getEnumLabel('taskStatus', status)
  let type: 'success' | 'warning' | 'error' = 'success'
  switch (status) {
    case 1: // 运行中
      type = 'success'
      break
    case 2: // 已停止
      type = 'warning'
      break
    case 3: // 运行异常
      type = 'error'
      break
  }

  return { text, type }
}
