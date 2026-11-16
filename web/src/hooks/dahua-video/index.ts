import type { IConfig } from './config'
import config from './config'

const tips1 = `
<div class="video-tips">
  插件未启动，正在尝试启动，请稍候...
</div>
`
const tips2 = `
<div class="video-tips">
<span>提示：系统检测到您还没有安装视频监控插件，请您下载插件进行播放。</span>
<a class="btn-link" target="_blank" download="视频WEB插件" href="${config.downloadPluginUrl}">立即下载</a>
</div>
`
const tips3 = `
<div class="video-tips">
监控信号弱，正在连接，请稍后...
</div>
`

export default function useDHVideo(params?: IConfig, event: any = {}) {
  let videoPlayer = shallowReactive<any>(null);
  const loaded = ref<boolean>(false);

  const _init_params = params?.initParams ? { ...config.initParams, ...params.initParams } : config.initParams

  // 初始化并登录插件
  function initDHPlayer(callback?: any) {
    const dom = document.getElementById(_init_params.videoId)

    if (!dom) {
      console.warn(`获取dom对象失败，请检查指定 DIV 窗口标识是否为${_init_params.videoId}，请确保initDHPlayer在mounted中执行`)
      return
    }

    dom.innerHTML = tips1

    return new Promise((resolve, reject) => {
      videoPlayer = new window.VideoPlayer({
        ..._init_params,
        // 下面均为回调函数，请按需使用，具体可在 API 内进行功能的体验和查看。

        // 创建播放器成功回调
        createSuccess: (versionInfo: any) => {
          loaded.value = true
          // 该回调触发后，我们可以进行实时预览/录像回放等操作
          console.log('大华监控创建成功-versionInfo', versionInfo)
          dom.innerHTML = ''
          callback && callback()
          resolve(videoPlayer)
          // document.querySelector("#desc").innerHTML = "创建成功!!!" + versionInfo.message
        },
        // 创建播放器失败回调
        createError: (err: any) => {
          console.log('大华监控创建失败-err', err)
          // 有错误码，可打印查看错误信息
          // document.querySelector("#desc").innerHTML = "创建失败!!!" + JSON.stringify(err)
          dom.innerHTML = err.code === 1004 ? tips3 : tips2
          reject(err)
        },
        // 插件公共回调
        dhPlayerMessage: (info: any, err: any) => {
          console.table({ info, err })
        },
        // 实时预览成功回调
        realSuccess: (info: any) => {
          console.log('实时预览成功回调info', info)
          event.realSuccess && event.realSuccess(info)
        },
        // 实时预览失败回调
        realError: (info: any, err: any) => {
          console.log('实时预览失败回调')
          console.table({ info, err })
        },
        // 对讲成功回调
        talkSuccess: (info: any) => {
          console.log('对讲成功回调info', info)
        },
        // 对讲失败回调
        talkError: (info: any, err: any) => {
          console.table({ info, err })
        },
        // 录像播放成功回调
        playbackSuccess: (info: any) => {
          console.log('录像播放成功回调info', info)
        },
        // 录像播放失败回调
        playbackError: (info: any, err: any) => {
          console.table({ info, err })
        },
        // 录像播放完成回调
        playbackFinish: (info: any) => {
          console.log('录像播放完成回调info', info)
        },
        // 抓图成功回调
        snapshotSuccess: ({ base64Url, path }: { base64Url: string, path: string }, info: any) => {
          const byteCharacters = atob(
            base64Url.replace(/^data:image\/(png|jpeg|jpg);base64,/, "")
          );
          const byteNumbers = new Array(byteCharacters.length);
          for (let i = 0; i < byteCharacters.length; i++) {
            byteNumbers[i] = byteCharacters.charCodeAt(i);
          }
          const byteArray = new Uint8Array(byteNumbers);
          const blob = new Blob([byteArray], {
            type: undefined,
          });
          const aLink = document.createElement("a");
          aLink.download = "图片名称.jpg"; //这里写保存时的图片名称
          aLink.href = URL.createObjectURL(blob);
          aLink.click();
        },
        // 本地录像下载成功回调
        videoDownloadSuccess: (path: any, info: any) => {
          alert("本地录像地址：" + path)
        },
        // 关闭视频窗口回调
        // closeWindowSuccess: ({ isAll, snum, channelList }) => { },
        // 鼠标单击窗口回调
        clickWindow: (snum: any) => {
          event.clickWindow & event.clickWindow(snum)
        },
        // 鼠标双击窗口回调
        dbClickWindow: (snum: any) => { },
        // 播放器窗口的数量回调
        changeDivision: (division: any) => { },
        // rtsp 流下载录像成功回调
        downloadRecordSuccess: (info: any) => { },
        // rtsp 流下载录像失败回调
        downloadRecordError: (info: any, err: any) => { }
      })
    })

  }

  // 实时预览
  function play(params: {
    /**
     * 通道id 【必传】
     */
    channelId: string,
    /**
     * 通道名称 (用于本地录像下载)
     */
    channelName: string,
    /**
     * 即将要播放的窗口序号，从0开始 【必传】
     */
    snum: number
    /**
     * 1-主码流  2-辅码流 (可不传，默认主码流)
     */
    streamType?: 1 | 2
    /**
     * 设备类别 (用于对讲使用) 
     */
    deviceType?: number
    /**
     * 摄像头类型 (用于云台)
     */
    cameraType?: string
    /**
     * 能力集 (用于云台)
     */
    capability?: string
  }) {
    console.log('videoPlayer', videoPlayer)
    if (!videoPlayer) {
      // document.querySelector("#desc").innerHTML = "不能播放，请先初始化插件！！！！！"
      window.$message.warning('不能播放，请先初始化插件!')
      return
    }

    videoPlayer.closeVideo(params.snum || 0).then(() => {
      videoPlayer.startReal([params], { isReOpen: true })
    })
  }

  // 录像回放
  function playBack(params: {
    /**
     * 通道id 【必传】
     */
    channelId: string,
    /**
     * 通道名称 (用于本地录像下载)
     */
    channelName: string,
    /**
     * 即将要播放的窗口序号，从0开始 【必传】
     */
    snum: number
    /**
     * 1-主码流  2-辅码流 (可不传，默认主码流)
     */
    streamType: 1 | 2
    /**
     * 2-录像 3-中心
     */
    recordSource: 2 | 3
    /**
     * 开始时间
     */
    startTime: string
    /**
     * 结束时间
     */
    endTime: string
  }) {
    if (!videoPlayer) {
      // document.querySelector("#desc").innerHTML = "不能播放，请先初始化插件！！！！！"
      window.$message.warning('不能播放，请先初始化插件!')
      return
    }
    videoPlayer.startPlayback([params])
  }

  function getInstance() {
    return videoPlayer
  }

  function show() {
    videoPlayer?.show()
  }

  // 控制插件的隐藏，两种方式均试用
  function hide() {
    videoPlayer?.hide()
  }

  function destroy() {
    videoPlayer?.hide()
    videoPlayer?.destroy()
  }

  return { initDHPlayer, show, hide, play, playBack, destroy, loaded, getInstance }
}