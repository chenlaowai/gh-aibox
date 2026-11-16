/**
 * WebSocket 配置
 */

/** WebSocket 默认配置 */
export const DEFAULT_WS_OPTIONS = {
  /** 最大重连次数 */
  maxReconnectAttempts: 5,
  /** 重连间隔（毫秒） */
  reconnectInterval: 5000,
  /** 心跳间隔（毫秒） */
  heartbeatInterval: 30000,
  /** 心跳消息 */
  heartbeatMessage: 'heartbeat',
  /** 是否自动重连 */
  autoReconnect: true,
}

/**
 * 获取 WebSocket URL
 * 将 http/https 转换为 ws/wss
 */

export function getWebSocketUrl(path: string, token?: string): string {
  let wsUrl = ''
  if (import.meta.env.PROD) {
    // 生产环境：使用当前主机名，端口9000
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const hostname = window.location.hostname
    wsUrl = `${protocol}//${hostname}:9000`
  }
  else {
    // 开发环境：使用环境变量中的配置
    wsUrl = import.meta.env.VITE_WS_API
  }

  // 如果有 token，添加到 URL 中
  if (token) {
    const separator = path.includes('?') ? '&' : '?'
    return `${wsUrl}${path}${separator}token=${encodeURIComponent(token)}`
  }

  return `${wsUrl}${path}`
}
