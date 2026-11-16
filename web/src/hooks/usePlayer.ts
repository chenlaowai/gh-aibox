/**
 * 大华监控hls播放逻辑封装，基于H5版本
 * 依赖jquery
 * @param intervalTime 间隔时间
 * @returns
 */

export function usePlayer(intervalTime = 180) {
  const cache = ref<any>([])
  let playerList: any = []

  class VideoPlayer {
    index: number
    playerDom: any
    createTime: number
    timer: any
    player: any
    item: { url: string, name: string, [key: string]: unknown }
    setTime: any

    constructor(item, index, videoWrapper) {
      this.index = index
      this.createTime = Date.now()
      this.timer = null
      this.player = {}
      this.item = item
      this.setTime = null

      // 创建视频 DOM
      this.playerDom = window.$(`
                <div class="video-item h-full">
                    <video id="video${index}" class="wh-full" muted preload="auto" autoplay="true"></video>
                </div>
            `)

      this.startPlay(videoWrapper)
    }

    startPlay(videoWrapper) {
      // 添加视频 DOM 到容器
      window.$(videoWrapper).append(this.playerDom[0])
      const video = window.$('video', this.playerDom[0])[0]

      // 初始化播放器
      this.player = playSingle(this.item.url, video)

      const dhHlsVideoWrappervideo = window.$(`#dhHlsVideoWrappervideo${this.index}`, this.playerDom[0])
      window.$(dhHlsVideoWrappervideo).append(window.$(`<div class="video-toolbar">
                    <div class="video-name" title="${this.item.name}">${this.item.name}</div>
                    <div class="video-zoom" title="全屏"></div>
                </div>`))

      window.$(dhHlsVideoWrappervideo).append(window.$(`<div class="video-loading"></div>`))

      const isPlayable = ref(false) // 标记视频是否可播放
      this.setTime = setTimeout(() => {
        if (!isPlayable.value) {
          this.player.destroy()
          window.$(video.parentNode).append(window.$(`<div class="videoLoadTimeOut">
                        </div>`))
        }
      }, 30000)
      this.playerDom[0].addEventListener('dblclick', (e: Event) => {
        if (!isPlayable.value)
          return
        const target = e.currentTarget as HTMLElement
        target?.classList.toggle('zoomin')
      })

      video.addEventListener('canplaythrough', () => {
        window.$('.video-loading', this.playerDom[0])?.[0]?.remove()
        isPlayable.value = true
        this.setTime && clearTimeout(this.setTime)
      })
      video.addEventListener('error', () => {
        window.$('.video-loading', this.playerDom[0])?.[0]?.remove()
      })

      window.$('.video-zoom', this.playerDom[0]).on('click', () => {
        utils.toggleFullScreen(video)
        return false
      })

      // 定时重启播放器
      this.timer = setInterval(() => {
        this.player.destroy()
        this.player = playSingle(this.item.url, video)
      }, intervalTime * 60 * 1000)
    }

    destroy() {
      window.clearInterval(this.timer as any)
      this.player?.destroy()
    }
  }

  function playMultiple(items: Array<{ url: string, name: string, [key: string]: unknown }>, videoWrapper: string) {
    window.$(videoWrapper).empty()

    // 销毁所有现有播放器
    playerList.forEach(player => player?.destroy())
    playerList = []

    // 使用类构造播放器
    items.forEach((item, index) => {
      const playerObj = new VideoPlayer(item, index, videoWrapper)
      playerList.push(playerObj)
    })

    return playerList
  }

  function playSingle(url, video) {
    if (!window.Hls.isSupported()) {
      window.$message.warning('浏览器不支持 HLS, 请升级！')
      return
    }
    if (video.canPlayType('application/vnd.apple.mpegurl')) {
      // 如果支持原生播放
      video.src = url
      return
    }
    const player = new window.Hls()
    player.config.manifestLoadingTimeOut = 30 * 1000
    player.loadSource(url)
    player.attachMedia(video)
    player.on(window.Hls.Events.MANIFEST_PARSED, () => { video.play() })

    cache.value.push(player)
    return player
  }

  function destroy() {
    cache.value?.forEach(val => val?.destroy())
    cache.value = []
  }

  onBeforeUnmount(destroy)

  return {
    playSingle,
    playMultiple,
    destroy,
  }
}
