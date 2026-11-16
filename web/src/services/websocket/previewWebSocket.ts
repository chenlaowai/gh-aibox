/**
 * 实时预览 WebSocket 服务
 */

import { BaseWebSocket, type WebSocketOptions } from './baseWebSocket'

interface PreviewMessage {
  ImageData?: string
  message?: string
  type?: string
}

interface PreviewWebSocketOptions extends Partial<Omit<WebSocketOptions, 'url' | 'onMessage'>> {
  /** 图片数据回调 */
  onMessage?: (ImageData: string) => void
  /** Token过期回调 */
  onAuthExpired?: () => void
}

export class PreviewWebSocket extends BaseWebSocket<PreviewMessage> {
  private currentKey: string = ''
  private pendingKey?: string

  constructor(options?: PreviewWebSocketOptions) {
    super({
      url: '', // 初始URL为空，在connect时设置
      binaryType: 'arraybuffer',
      ...options,
    })
  }

  /**
   * 处理消息
   */
  protected handleMessage(event: MessageEvent): void {
    try {
      // 忽略心跳消息
      if (this.options.heartbeatResponses.includes(event.data)) {
        return
      }

      if (event.data instanceof ArrayBuffer) {
        const buffer = event.data
        const view = new DataView(buffer)
        // 前4个字节表示header长度
        const headerLen = view.getUint32(0)

        // 解析header JSON
        const headerBuf = buffer.slice(4, 4 + headerLen)
        const header = JSON.parse(new TextDecoder().decode(headerBuf))

        // 取出图片字节
        const imgBuf = buffer.slice(4 + headerLen)

        // 生成blob以供<img>显示
        const blob = new Blob([imgBuf], { type: `image/${header.imageType}` })
        const url = URL.createObjectURL(blob)
        this.options.onMessage?.(url)
      }
      else {
        // 解析JSON数据
        const data = JSON.parse(event.data) as PreviewMessage

        // 处理Token过期
        if (data.type === 'auth_expired') {
          this.options.onAuthExpired?.(data.message)
          return
        }

        // 处理图片数据
        if (data.ImageData) {
          this.options.onMessage?.(data.ImageData)
        }
      }
    }
    catch (error) {
      console.error('解析WebSocket消息失败:', error, event.data)
    }
  }

  /**
   * 连接成功钩子
   */
  protected onOpen(): void {
    // 连接成功后，如果有待发送的key值则发送
    if (this.pendingKey) {
      this.sendKey(this.pendingKey)
      this.pendingKey = undefined
    }
  }

  /**
   * 达到最大重连次数
   */
  protected onMaxReconnectAttemptsReached(): void {
    window.$message?.error('实时预览连接失败，请刷新页面重试')
  }

  /**
   * 连接 WebSocket
   * @param url WebSocket 完整 URL（包含 token）
   * @param key 可选的初始 key 值
   */
  connectWithUrl(url: string, key?: string) {
    this.setUrl(url)
    this.pendingKey = key
    this.connect()
  }

  /**
   * 发送key值切换视频流
   */
  sendKey(key: string) {
    this.currentKey = key
    const success = this.send(key)
    if (success) {
      console.log('发送key值:', key)
    }
    else {
      // 如果发送失败，保存为待发送
      this.pendingKey = key
    }
  }

  /**
   * 获取当前的key值
   */
  get currentKeyValue(): string {
    return this.currentKey
  }
}
