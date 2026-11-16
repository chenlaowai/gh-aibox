/**
 * 阿里云直播流拉取
 
   组件当中使用示例：
   import { useAliyunRtc } from '@/hooks'
 
   const aliyunRtc = useAliyunRtc({
    renderFail: () => h('div', { class: 'wh-full flex-center bg-#1e3c7899 font-size-16px' }, { default: () => theUavGetTopologies.value.modeCode === -1 ? '断开连接' : '加载失败' }),
   })
 
   aliyunRtc.changeStatus(1)
    const [error, r1] = await utils.tryit(api.uav.getUavGetPlayUrl)({
      id: commonStore.airportId!,
    })
    if (error || !r1.data?.url) {
      aliyunRtc.changeStatus(3)
      return false
    }
 
    aliyunRtc.start(r1.data.url!, videoRef.value)
 */

import { AliRTS } from 'aliyun-rts-sdk'
import type { VNode } from 'vue'

type Opt = {
  renderFail?: () => VNode
}

const aliRts = AliRTS.createClient()

/**
 * 视频流加载状态（0、待加载 1、加载中 2、加载成功 3、加载失败 4、手动停止）
 */
const status = ref(0)

export function useAliyunRtc(opt?: Opt) {
  /**
   * rts开始拉流接口
   * @param {string} pullStreamUrl 拉流地址,在地址后添加@subaudio=no或者@subvideo=no来表示不订阅音频流或视频流
   * @param {object} [config] （可选）自定义配置
   * @param {string} [config.signalUrl] （可选）信令地址
   * @return {Promise}
   */
  function start(url: string, video: HTMLMediaElement) {
    status.value = 1
    return new Promise((resolve, reject) => {
      aliRts.subscribe(url).then((remoteStream: any) => {
        // mediaElement是媒体标签audio或video
        remoteStream.play(video)
        remoteStream.muted = true

        status.value = 2

        resolve(url)
      }).catch((err: any) => {
        console.log(`output->err`, err)

        status.value = 3
        // 订阅失败
        reject(err)
      })
    })
  }

  /**
   * 停止rts播放接口
   */
  function stop() {
    aliRts.unsubscribe()
    status.value = 4
  }

  function changeStatus(_status: number) {
    status.value = _status
  }

  function render() {
    const src = new URL('@/assets/images/loading.webp', import.meta.url).href
    return [
      null,
      h('img', { src, class: 'wh-full' }),
      null,
      opt?.renderFail?.() || h('div', { class: 'wh-full flex-center bg-#1e3c7899 font-size-16px' }, { default: () => '加载失败' }),
    ][status.value]
  }

  return {
    start,
    stop,
    changeStatus,
    render,
    status,
  }
}
