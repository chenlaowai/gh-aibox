// Alarm media files are written by the AI service to
// /data/CT/project/ai/inference/save/<task>/<algo>/{img,video}/...
// The dotnet API exposes that directory as its wwwroot under /aibox/, so the
// browser URL is /aibox/<relative path>.  The old code used /data/... directly,
// which nginx's SPA fallback answered with index.html (HTTP 200 text/html), so
// every alarm image/video appeared broken.
const MEDIA_ROOT = '/data/CT/project/ai/inference/save/'
const SAVE_MARKER = '/inference/save/'

export function resolveAlarmMediaUrl(path?: string): string {
  if (!path) return ''
  const p = path.replace(/\\/g, '/')
  if (/^(https?:)?\/\//i.test(p) || p.startsWith('data:') || p.startsWith('blob:'))
    return p
  if (p.startsWith('/aibox/'))
    return p

  const base = import.meta.env.VITE_BASE_API || '/aibox/'
  if (p.startsWith(MEDIA_ROOT))
    return `${base}${p.slice(MEDIA_ROOT.length)}`

  const marker = p.indexOf(SAVE_MARKER)
  if (marker >= 0)
    return `${base}${p.slice(marker + SAVE_MARKER.length)}`

  return `${base}${p.replace(/^\/+/, '')}`
}
