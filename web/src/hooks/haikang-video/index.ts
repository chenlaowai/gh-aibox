/**
 * 开发流程：
 * 1、安装 VideoWebPlugin.exe 插件包
 * 2、引入js文件
 *    <script src="jsencrypt.min.js"></script> // 用于 RSA 公钥加密
 *    <script src="jsWebControl-1.0.0.min.js"></script> // 用于前端与插件交互
 *
 * 工作流程：
 * 1、new WebControl创建实例(必须)
 * 2、JS_StartService启动websocket服务(必须)
 * 3、JS_SetWindowControlCallback设置消息回调(可选)
 * 4、JS_CreateWnd创建插件窗口(必须)
 * 5、JS_RequestInterface/getRSAPubKey申请RSA公钥(可选)
 * 6、JS_RequestInterface/init初始化(必须)
 * 7、JS_RequestInterface/startPreview(startPlayback)开始视频预览或录像回放(必须)
 * 8、JS_RequestInterface/startAllPreview(startAllPlayback)停止全部视频预览或所有录像回放(必须)
 * 9、JS_DestroyWnd销毁插件窗口(可选)
 * 10、JS_Disconnect断开插件服务连接(必须)
 *
 * @author xiaohuangmaoth
 */
import type { IConfig } from "./config";
import config from "./config";

export default function useHaiKangVideo(params?: IConfig, event: any = {}) {
  // 插件对象实例，初始化为null，需要创建多个插件窗口时，需要定义多个插件对象实例变量，各个变量唯一标志对应的插件实例
  let oWebControl: any;
  // 插件初始化次数
  const initCount = ref<number>(0);
  // 公钥
  const pubKey = ref<string>("");
  // 窗体尺寸
  const size = reactive<{ width: number; height: number }>({
    width: 0,
    height: 0,
  });

  const _max_retries_count =
    params?.max_retries_count ?? config.max_retries_count;
  const _init_params = params?.init_params
    ? { ...config.init_params, ...params.init_params }
    : config.init_params;
  const _instance_params = params?.instance_params
    ? { ...config.instance_params, ...params.instance_params }
    : config.instance_params;

  const tips1 = `
    <div class="tips">
      插件未启动，正在尝试启动，请稍候...
    </div>
  `;
  const tips2 = `
  <div class="tips">
    <span>提示：系统检测到您还没有安装视频监控插件，请您下载插件进行播放。</span>
    <a class="btn-link" target="_blank" download="视频WEB插件" href="${config.downloadPluginUrl}">立即下载</a>
  </div>
`;

  // 创建 WebControl 实例
  function initPlugin() {
    const dom = document.getElementById(_instance_params.szPluginContainer);
    console.log(`output->dom`, dom);
    if (!dom) {
      console.warn(
        `获取dom对象失败，请检查指定 DIV 窗口标识是否为${_instance_params.szPluginContainer}，请确保initPlugin在mounted中执行`
      );
      return;
    }

    const { width, height } = dom.getBoundingClientRect();

    size.width = width;
    size.height = height;

    console.table({ width: size.width, height: size.height });
    dom.innerHTML = tips1;
    return new Promise((resolve, reject) => {
      oWebControl = new window.WebControl({
        ..._instance_params,
        cbConnectSuccess: function () {
          // 创建 WebControl 实例成功
          initCount.value = 0;

          oWebControl.JS_SetWindowControlCallback({
            cbIntegrationCallBack: event.showCBInfo,
          });

          // 建立websocket连接
          oWebControl
            .JS_StartService("window", {
              dllPath: "./VideoPluginConnect.dll",
            })
            .then(
              function () {
                // 创建插件窗口
                oWebControl
                  .JS_CreateWnd(
                    _instance_params.szPluginContainer,
                    size.width,
                    size.height
                  )
                  .then(function () {
                    console.log("JS_CreateWnd success");
                    getRSAPubKey();
                    init();
                    dom.innerHTML = "";
                    resolve(oWebControl);
                    window.addEventListener("resize", resize);
                  });
              },
              function () {
                dom.innerHTML = tips2;
                reject(false);
              }
            );
        },
        cbConnectError: function () {
          // 创建 WebControl 实例失败
          console.log("cbConnectError");
          oWebControl = null;

          dom.innerHTML = tips1;
          // 程序未启动时执行 error 函数，采用 wakeup 来启动程序
          window.WebControl.JS_WakeUp("VideoWebPlugin://");
          initCount.value++;
          if (initCount.value < _max_retries_count) {
            setTimeout(function () {
              initPlugin();
            }, 3000);
          } else {
            dom.innerHTML = tips2;
          }
          reject(false);
        },
        cbConnectClose: function (bNormalClose: boolean) {
          // 插件使用过程中发生的断开与插件服务连接的回调
          // bNormalClose = false 时表示异常断开
          // bNormalClose = true 时表示正常断开
          console.warn(
            "cbConnectClose",
            bNormalClose ? "插件服务连接正常断开" : "插件服务连接异常断开"
          );
          if (bNormalClose) return; // 如果是正常断开，则停止往下执行

          oWebControl = null;
          dom.innerHTML = tips1;
          window.WebControl.JS_WakeUp("VideoWebPlugin://");
          initCount.value++;
          if (initCount.value < _max_retries_count) {
            setTimeout(function () {
              initPlugin();
            }, 3000);
          } else {
            dom.innerHTML = tips2;
          }
        },
      });
    });
  }

  async function getLayout() {
    const r1 = await oWebControl.JS_RequestInterface({
      funcName: "getLayout",
    });

    return JSON.parse(r1.responseMsg.data);
  }

  function init() {
    oWebControl
      .JS_RequestInterface({
        funcName: "init",
        argument: JSON.stringify({
          ..._init_params,
          // secret: setEncrypt(params.secret),
        }),
      })
      .then(function (oData: any) {
        console.log("初始化成功：", oData);
        oWebControl.JS_Resize(size.width, size.height);
      });
  }

  function play({
    cameraIndexCode,
    playMode = 0,
    ...args
  }: {
    cameraIndexCode: string;
    playMode?: 0 | 1;
    [key: string]: unknown;
  }) {
    console.table({ cameraIndexCode, playMode });

    const funcName = ["startPreview", "startPlayback"][playMode];
    oWebControl
      .JS_RequestInterface({
        funcName,
        argument: JSON.stringify({
          cameraIndexCode,
          ...args,
        }),
      })
      .then(function (oData: any) {
        console.log(`output->oData`, oData);
      });
  }

  function playList(
    list: Array<{
      /**
       * 监控点编号
       */
      cameraIndexCode: string;
      /**
       * 是否直连萤石预览
       */
      ezvizDirect?: string;
      /**
       * 是否启用 GPU 硬解
       */
      gpuMode?: string;
      /**
       * 主子码流标识
       */
      streamMode?: string;
      /**
       * 传输协议
       */
      transMode?: string;
      /**
       * 播放窗口序号
       */
      wndId?: number;
    }>
  ) {
    oWebControl
      .JS_RequestInterface({
        funcName: "stopAllPreview",
      })
      .then(function () {
        oWebControl
          .JS_RequestInterface({
            funcName: "startMultiPreviewByCameraIndexCode",
            argument: JSON.stringify({ list }),
          })
          .then(function (oData: any) {
            console.log(`output->playListCallback`, oData);
          });
      });
  }

  function getRSAPubKey() {
    oWebControl
      .JS_RequestInterface({
        funcName: "getRSAPubKey",
        argument: JSON.stringify({
          keyLength: 1024,
        }),
      })
      .then(function (oData: any) {
        console.log("公钥", oData);
        if (oData.responseMsg.data) {
          pubKey.value = oData.responseMsg.data;
        }
      });
  }

  // RSA加密
  // function setEncrypt(value: string) {
  //   const encrypt = new window.JSEncrypt();
  //   encrypt.setPublicKey(pubKey.value);
  //   return encrypt.encrypt(value);
  // }

  // 设置窗口裁剪，当因滚动条滚动导致窗口需要被遮住的情况下需要JS_CuttingPartWindow部分窗口
  function setWndCover() {
    //获取web页面的尺寸
    const iWidth = window.innerWidth;
    const iHeight = window.innerHeight;
    //获取播放窗口div元素的左边界、右边界距离web页面左边界的长度、上边界、下边界距离web页面上边界的长度
    const oDivRect = document
      .getElementById(_instance_params.szPluginContainer)
      ?.getBoundingClientRect();
    if (!oDivRect) return;
    //Math.round 为四舍五入    Math.abs 为取绝对值
    let iCoverLeft = oDivRect.left < 0 ? Math.abs(oDivRect.left) : 0;
    let iCoverTop = oDivRect.top < 0 ? Math.abs(oDivRect.top) : 0;
    let iCoverRight =
      oDivRect.right - iWidth > 0 ? Math.round(oDivRect.right - iWidth) : 0;
    let iCoverBottom =
      oDivRect.bottom - iHeight > 0 ? Math.round(oDivRect.bottom - iHeight) : 0;

    iCoverLeft = iCoverLeft > size.width ? size.height : iCoverLeft;
    iCoverTop = iCoverTop > size.height ? size.height : iCoverTop;
    iCoverRight = iCoverRight > size.width ? size.width : iCoverRight;
    iCoverBottom = iCoverBottom > size.height ? size.height : iCoverBottom;

    oWebControl.JS_RepairPartWindow(0, 0, size.width + 1, size.height); // 多1个像素点防止还原后边界缺失一个像素条
    //抠除左边界
    if (iCoverLeft != 0) {
      oWebControl.JS_CuttingPartWindow(0, 0, iCoverLeft, size.height + 1);
    }
    //抠除上边界
    if (iCoverTop != 0) {
      oWebControl.JS_CuttingPartWindow(0, 0, size.width + 1, iCoverTop); // 多剪掉一个像素条，防止出现剪掉一部分窗口后出现一个像素条
    }
    //抠除右边界
    if (iCoverRight != 0) {
      oWebControl.JS_CuttingPartWindow(
        size.width + 1 - iCoverRight,
        0,
        iCoverRight,
        size.height + 1
      );
    }
    //抠除下边界
    if (iCoverBottom != 0) {
      oWebControl.JS_CuttingPartWindow(
        0,
        size.height + 1 - iCoverBottom,
        size.width + 1,
        iCoverBottom
      );
    }
  }

  function close() {
    if (oWebControl != null) {
      oWebControl.JS_HideWnd(); // 先让窗口隐藏，规避可能的插件窗口滞后于浏览器消失问题
      oWebControl.JS_Disconnect().then(
        function () {},
        function () {}
      );
      window.removeEventListener("resize", resize);
    }
  }

  function show() {
    if (oWebControl != null) {
      oWebControl.JS_ShowWnd();
    }
  }

  function hide() {
    if (oWebControl != null) {
      oWebControl.JS_HideWnd();
    }
  }

  function resize() {
    const dom = document.getElementById(_instance_params.szPluginContainer);
    if (!dom) return;

    const { width, height } = dom.getBoundingClientRect();

    size.width = width;
    size.height = height;

    if (oWebControl != null) {
      oWebControl.JS_Resize(size.width, size.height);
      setWndCover();
    }
  }

  function getVersion() {
    oWebControl
      .JS_RequestInterface({
        funcName: "getVersion",
      })
      .then(function (oData: any) {
        console.log("版本号：", oData);
      });
  }

  function hasInstance() {
    return oWebControl != null;
  }

  // onMounted(initPlugin)

  // onUnmounted(() => {
  //   window.removeEventListener('resize', resize)
  //   close()
  // })

  return {
    initPlugin,
    play,
    playList,
    close,
    resize,
    getVersion,
    show,
    hide,
    hasInstance,
    getLayout,
  };
}
