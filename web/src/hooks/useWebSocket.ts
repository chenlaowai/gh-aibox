import websocket from 'websocket'

const W3CWebSocket = websocket.w3cwebsocket

export function useWebSocket(url: string, protocol?: string) {
  let reconnectAttempts = 0 // 当前重连次数
  const maxReconnectAttempts = 5 // 最大重连次数
  let timer: any = null
  let client: any = null
  let shipId: string = ''
  let manualClose = false // 标记是否手动关闭WebSocket
  let onMessageCallback: ((data: any) => void) | null = null

  function connect() {
    manualClose = false
    client = new W3CWebSocket(url, protocol)

    client.onerror = function () {
      reconnect()
    }

    client.onopen = function () {
      startHeartbeat()
      if (client && client.readyState === client.OPEN) {
        client.send(`shipid:${shipId}`)
      }
      reconnectAttempts = 0 // 重置重连次数
    }

    client.onmessage = function (r: any) {
      if (r.data === 'heartbeat') {
        client.send('response heartbeat')
      }
      if (r.data !== 'heartbeat' && r.data !== 'response heartbeat') {
        onMessageCallback && onMessageCallback(r.data)
      }
    }

    client.onclose = function () {
      stopHeartbeat()
      if (!manualClose) {
        reconnect()
      }
    }
  }

  function startHeartbeat() {
    function sendHeartbeat() {
      if (client && client.readyState === client.OPEN) {
        client.send('heartbeat') // 发送心跳消息到服务器
        timer = setTimeout(sendHeartbeat, 1000 * 30)
      }
    }
    sendHeartbeat()
  }

  function stopHeartbeat() {
    if (timer)
      clearTimeout(timer)
  }

  function reconnect() {
    if (reconnectAttempts < maxReconnectAttempts) {
      reconnectAttempts++
      timer = setTimeout(() => {
        connect()
      }, 5000) // 重连间隔时间为5秒
    }
    else {
      console.log('达到最大重连次数，停止重连')
    }
  }

  function close() {
    if (client) {
      manualClose = true // 标记手动关闭WebSocket
      client.close()
      client = null
    }
    stopHeartbeat()
    clearTimeout(timer)
  }

  function onMessage(callback: (data: any) => void) {
    onMessageCallback = callback
  }

  function setShipId(val: string) {
    shipId = val
  }

  connect()

  return {
    client,
    connect,
    close,
    onMessage,
    setShipId,
  }
}
