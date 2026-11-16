/**
 * WebSocket URL 管理
 *
 * 注意：此文件用于管理所有 WebSocket 端点的 URL 获取函数
 * 由于 api 目录下的文件是脚本自动生成的，会被覆盖，
 * 所以将 WebSocket URL 管理统一放在这里
 */

import { getWebSocketUrl } from './config'

/**
 * 获取预览视频 WebSocket URL
 * @param token 认证 token
 * @returns WebSocket URL
 */
export function getPreviewVideoWebSocketUrl(token: string): string {
  return getWebSocketUrl('/ws/frame', token)
}

/**
 * 示例：添加其他 WebSocket URL 获取函数
 *
 * export function getAlarmWebSocketUrl(token: string): string {
 *   return getWebSocketUrl('/api/Alarm/Subscribe', token)
 * }
 *
 * export function getChatWebSocketUrl(token: string, roomId: string): string {
 *   const path = `/api/Chat/Room?roomId=${roomId}`
 *   return getWebSocketUrl(path, token)
 * }
 */
