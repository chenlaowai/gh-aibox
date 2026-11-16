export interface IConfig {
  initParams?: {
    videoId?: string;
    division?: number;
    usePluginLogin?: boolean;
    windowType?: number;
    draggable?: boolean;
    showBar?: boolean;
    coverShieldClass?: string[];
    parentIframeShieldClass?: string[];
  };
}

export default {
  initParams: {
    videoId: "playWnd",
    pluginLoginInfo: {
      host: "10.10.69.247", // icc 平台ip
      port: "443", // icc 平台端口 https 默认 443
      username: "huanancehui", // icc 平台用户名
      password: "icc#2023", // icc 平台密码
    },
    division: 1, // 默认展示的窗口数量， 必传
    usePluginLogin: true, // 采用登录 (请默认传true，插件内部自动拉流)
    windowType: 0, // 播放器类型，必传， 0 - 实时预览，3 - 录像回放，7- 录像回放（支持倒放）
    draggable: false, // 窗口拖拽 【暂不支持】
    showBar: true, // 底部操作栏， 选传，【true - 显示, false - 隐藏】
    shieldClass: [], // 如果DOM元素被插件挡住了，把DOM元素的类名传入。
    coverShieldClass: [], // 如果插件要在dom内滚动，需要把DOM元素的类名传入，请查看案例-遮挡
    parentIframeShieldClass: [], // 有 iframe 时，top层 的 dom 元素被插件挡住了，把DOM元素的类名传入。
  },
  downloadPluginUrl: "https://www.hnkancha.com/gzgstatic/DHPlayerSetup.exe",
};
