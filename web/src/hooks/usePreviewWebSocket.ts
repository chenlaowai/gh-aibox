import { onUnmounted, ref, watch } from 'vue'
import { PreviewWebSocket, getPreviewVideoWebSocketUrl } from '@/services/websocket'
import { useAuthStore } from '@/store/auth'
import { ENABLE_REFRESH_TOKEN } from '@/services/axios/config'

export function usePreviewWebSocket() {
  const authStore = useAuthStore()
  const imageData = ref<string>('')
  const isConnected = ref(false)
  const error = ref<string | null>(null)

  // 监听 imageData 变化，释放旧的 blob url 资源，防止内存泄漏
  watch(imageData, (_newVal, oldVal) => {
    if (oldVal && oldVal.startsWith('blob:')) {
      URL.revokeObjectURL(oldVal)
    }
  })

  let ws: PreviewWebSocket | null = null

  /**
   * 连接 WebSocket
   */
  const connect = (initialKey?: string) => {
    try {
      ws = new PreviewWebSocket({
        onMessage: (data) => {
          imageData.value = data
          error.value = null
        },
        onAuthExpired: async (message = '登录已过期，请重新登录') => {
          if (ENABLE_REFRESH_TOKEN) {
            try {
              // 尝试刷新token
              await authStore.getRefreshToken()
              // 刷新成功后重连
              console.log('Token refreshed, reconnecting WebSocket...')
              const currentKey = ws?.currentKeyValue || initialKey
              // 断开旧连接（如果未断开）
              if (ws) {
                ws.close()
                ws = null
              }
              // 重新连接
              connect(currentKey)
              return
            }
            catch (e) {
              console.error('WebSocket token refresh failed:', e)
            }
          }

          window.$message?.error(message)
          authStore.logout()
        },
        onOpen: () => {
          isConnected.value = true
          error.value = null
          console.log('预览 WebSocket 连接成功')
        },
        onClose: () => {
          isConnected.value = false
          console.log('预览 WebSocket 连接关闭')
        },
        onError: (err) => {
          error.value = '连接失败'
          console.error('预览 WebSocket 错误:', err)
        },
      })

      const wsUrl = getPreviewVideoWebSocketUrl(authStore.rawAccessToken)
      ws.connectWithUrl(wsUrl, initialKey)
    }
    catch (err) {
      error.value = '初始化失败'
      console.error('初始化预览 WebSocket 失败:', err)
    }
  }

  /**
   * 发送 key 切换视频流
   */
  const sendKey = (key: string) => {
    if (ws) {
      ws.sendKey(key)
    }
    else {
      console.warn('WebSocket 未初始化')
    }
  }

  /**
   * 断开连接
   */
  const disconnect = () => {
    if (ws) {
      ws.close()
      ws = null
    }
    isConnected.value = false
    imageData.value = ''
  }

  /**
   * 重新连接
   */
  const reconnect = (initialKey?: string) => {
    disconnect()
    connect(initialKey)
  }

  // 组件卸载时自动断开
  onUnmounted(() => {
    disconnect()
  })

  return {
    imageData,
    isConnected,
    error,
    connect,
    sendKey,
    disconnect,
    reconnect,
  }
}
