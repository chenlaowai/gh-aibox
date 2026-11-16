/**
 * 直连 AI 控制端口（9000）的任务热更新触发。
 *
 * 生产盒子的保存请求会通过 handleRemoteMessage -> onTaskArrived ->
 * archiveUpdated 立即把新 TaskArchive 推给运行中的任务。我们保留 2s 轮询
 * 作为兜底，但保存成功后主动调用一次，避免用户等轮询。
 */
export function getAiHttpBase(): string {
  if (import.meta.env.PROD) {
    const protocol = window.location.protocol === 'https:' ? 'https:' : 'http:'
    return `${protocol}//${window.location.hostname}:9000`
  }
  const ws = import.meta.env.VITE_WS_API || 'ws://127.0.0.1:9000'
  return ws.replace(/^ws/, 'http').replace(/\/$/, '')
}

export function compactStreamError(err?: string): string {
  if (!err)
    return ''
  if (/Input\/output|I\/O error|upstream no video|gateway exited|DESCRIBE failed|5XX|Server returned 5/i.test(err))
    return 'Input/output error'
  if (/missing data\.url|Camera not found|ISC/i.test(err))
    return 'Camera not found'
  if (/decoder open/i.test(err))
    return '打开解码器失败'
  return err.slice(0, 32)
}

export async function uploadAiMedia(file: File, token: string): Promise<string> {
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), 120000)
  try {
    const res = await fetch(`${getAiHttpBase()}/channel/upload?name=${encodeURIComponent(file.name)}`, {
      method: 'POST',
      headers: { 'Authorization': `Bearer ${token}` },
      body: file,
      signal: controller.signal,
    })
    const data = await res.json()
    if (!res.ok || data.status !== 'ok' || !data.path)
      throw new Error(data.msg || '上传失败')
    return data.path
  }
  finally {
    clearTimeout(timer)
  }
}

export interface AiChannelProbe {
  status: 'ok' | 'error' | string
  health?: string
  resolution?: string
  lastError?: string
}

export async function probeAiChannel(address: string, token: string): Promise<AiChannelProbe | null> {
  if (!address || !token)
    return null
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), 9000)
  try {
    const res = await fetch(`${getAiHttpBase()}/channel/probe`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ address }),
      signal: controller.signal,
    })
    if (!res.ok)
      return null
    return await res.json()
  }
  catch {
    return null
  }
  finally {
    clearTimeout(timer)
  }
}

export interface AiTaskHealth {
  taskId: string
  taskName?: string
  channelAddress?: string
  resolution?: string
  health?: 'ok' | 'connecting' | 'stalled' | 'error' | 'stopped' | string
  lastFrameAgeMs?: number
  everFrame?: boolean
  error?: string
  lastError?: string
}

export async function fetchAiTaskHealth(token: string): Promise<AiTaskHealth[]> {
  if (!token)
    return []
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), 3000)
  try {
    const res = await fetch(`${getAiHttpBase()}/task/list`, {
      headers: { 'Authorization': `Bearer ${token}` },
      signal: controller.signal,
    })
    if (!res.ok)
      return []
    const data = await res.json()
    return Array.isArray(data?.running) ? data.running : []
  }
  catch {
    return []
  }
  finally {
    clearTimeout(timer)
  }
}

export async function triggerAiTaskReload(taskId: string, token: string): Promise<void> {
  if (!taskId || !token)
    return
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), 2500)
  try {
    await fetch(`${getAiHttpBase()}/task/reload_config`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ taskId }),
      signal: controller.signal,
    })
  }
  catch (e) {
    // 轮询兜底仍会在 2s 内生效；不要让 AI 直连失败影响保存流程。
    console.warn('AI 任务热更新触发失败，等待轮询兜底:', e)
  }
  finally {
    clearTimeout(timer)
  }
}
