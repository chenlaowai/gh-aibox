/**
 * WebSocket 基类
 * 提供通用的连接、重连、心跳等功能
 */

import { DEFAULT_WS_OPTIONS } from './config'

export interface WebSocketOptions {
  /** WebSocket URL */
  url: string
  /** 消息回调 */
  onMessage?: (data: any) => void
  /** 错误回调 */
  onError?: (error: Event) => void
  /** 连接成功回调 */
  onOpen?: () => void
  /** 连接关闭回调 */
  onClose?: () => void
  /** 最大重连次数 */
  maxReconnectAttempts?: number
  /** 重连间隔（毫秒） */
  reconnectInterval?: number
  /** 心跳间隔（毫秒） */
  heartbeatInterval?: number
  /** 心跳消息 */
  heartbeatMessage?: string
  /** 是否自动重连 */
  autoReconnect?: boolean
  /** 心跳响应消息列表（用于忽略） */
  heartbeatResponses?: string[]
  /** 二进制数据格式 */
  binaryType?: BinaryType
}

export class BaseWebSocket<T = any> {
  protected ws: WebSocket | null = null
  protected reconnectTimer: number | null = null
  protected heartbeatTimer: number | null = null
  protected reconnectAttempts = 0
  protected manualClose = false
  protected options: Required<WebSocketOptions>
  protected url: string

  constructor(options: WebSocketOptions) {
    this.url = options.url
    this.options = {
      ...DEFAULT_WS_OPTIONS,
      heartbeatResponses: ['heartbeat', 'response heartbeat'],
      ...options,
    } as Required<WebSocketOptions>
  }

  /**
   * 更新 WebSocket URL
   */
  setUrl(url: string) {
    this.url = url
    this.options.url = url
  }

  /**
   * 启动心跳
   */
  protected startHeartbeat() {
    this.stopHeartbeat()
    this.heartbeatTimer = window.setInterval(() => {
      if (this.ws && this.ws.readyState === WebSocket.OPEN) {
        this.send(this.options.heartbeatMessage)
      }
    }, this.options.heartbeatInterval)
  }

  /**
   * 停止心跳
   */
  protected stopHeartbeat() {
    if (this.heartbeatTimer) {
      clearInterval(this.heartbeatTimer)
      this.heartbeatTimer = null
    }
  }

  /**
   * 连接 WebSocket
   */
  connect() {
    try {
      this.manualClose = false
      console.log('正在连接WebSocket:', this.url)

      this.ws = new WebSocket(this.url)
      this.ws.binaryType = this.options.binaryType || 'blob'

      this.ws.onopen = () => {
        console.log('WebSocket连接成功')
        this.reconnectAttempts = 0
        this.startHeartbeat()
        this.onOpen()
        this.options.onOpen?.()
      }

      this.ws.onmessage = (event) => {
        this.handleMessage(event)
      }

      this.ws.onerror = (error) => {
        console.error('WebSocket错误:', error)
        this.onError(error)
        this.options.onError?.(error)
      }

      this.ws.onclose = () => {
        console.log('WebSocket连接关闭')
        this.stopHeartbeat()
        this.onClose()
        this.options.onClose?.()

        // 如果不是手动关闭且开启自动重连，则尝试重连
        if (!this.manualClose && this.options.autoReconnect) {
          this.reconnect()
        }
      }
    }
    catch (error) {
      console.error('创建WebSocket连接失败:', error)
    }
  }

  /**
   * 处理消息（子类可重写）
   */
  protected handleMessage(event: MessageEvent) {
    try {
      // 忽略心跳消息
      if (this.options.heartbeatResponses.includes(event.data)) {
        return
      }

      // 尝试解析 JSON
      try {
        const data = JSON.parse(event.data)

        this.onMessage(data)
        this.options.onMessage?.(data)
      }
      catch {
        // 如果不是 JSON，直接传递原始数据
        this.onMessage(event.data)
        this.options.onMessage?.(event.data)
      }
    }
    catch (error) {
      console.error('处理WebSocket消息失败:', error, event.data)
    }
  }

  /**
   * 消息处理钩子（子类可重写）
   */
  protected onMessage(data: T) {
    // 子类可以重写此方法进行自定义处理
  }

  /**
   * 连接成功钩子（子类可重写）
   */
  protected onOpen() {
    // 子类可以重写此方法进行自定义处理
  }

  /**
   * 错误钩子（子类可重写）
   */
  protected onError(error: Event) {
    // 子类可以重写此方法进行自定义处理
  }

  /**
   * 连接关闭钩子（子类可重写）
   */
  protected onClose() {
    // 子类可以重写此方法进行自定义处理
  }

  /**
   * 重连
   */
  protected reconnect() {
    if (this.reconnectAttempts < this.options.maxReconnectAttempts) {
      this.reconnectAttempts++
      console.log(
        `尝试重连 (${this.reconnectAttempts}/${this.options.maxReconnectAttempts})...`,
      )
      this.reconnectTimer = window.setTimeout(() => {
        this.connect()
      }, this.options.reconnectInterval)
    }
    else {
      console.log('达到最大重连次数，停止重连')
      this.onMaxReconnectAttemptsReached()
    }
  }

  /**
   * 达到最大重连次数钩子（子类可重写）
   */
  protected onMaxReconnectAttemptsReached() {
    window.$message?.error('连接失败，请刷新页面重试')
  }

  /**
   * 发送消息
   */
  send(data: string | ArrayBufferLike | Blob | ArrayBufferView) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(data)
      return true
    }
    else {
      console.warn('WebSocket未连接，无法发送消息')
      return false
    }
  }

  /**
   * 关闭连接
   */
  close() {
    this.manualClose = true
    this.stopHeartbeat()

    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }

    if (this.ws) {
      this.ws.close()
      this.ws = null
    }

    console.log('WebSocket连接已手动关闭')
  }

  /**
   * 获取连接状态
   */
  get isConnected(): boolean {
    return this.ws !== null && this.ws.readyState === WebSocket.OPEN
  }

  /**
   * 获取 WebSocket 实例
   */
  get instance(): WebSocket | null {
    return this.ws
  }
}
