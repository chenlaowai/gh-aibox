import dayjs from 'dayjs'
import duration from 'dayjs/plugin/duration'

dayjs.extend(duration)

/**
 * 根据秒数获取格式化后的时间
 * @param startTime
 * @param endTime
 * @returns [string, string]
 */
export function getFormatTimesBySeconds(startTime: number, endTime: number): string[] {
  if (Number.isNaN(startTime) || Number.isNaN(endTime))
    return ['00:00:00', '00:00:00']
  return [
    dayjs.duration(Math.floor(startTime) * 1000).format('HH:mm:ss'),
    dayjs.duration(Math.floor(endTime) * 1000).format('HH:mm:ss'),
  ]
}
