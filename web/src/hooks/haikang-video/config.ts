interface IConfig {
  max_retries_count?: number;
  init_params?: {
    appkey?: string;
    ip?: string;
    port?: number;
    secret?: string;
    enableHTTPS?: 1;
    language?: string;
    layout?: string;
    playMode?: 0 | -1;
    reconnectDuration?: number;
    reconnectTimes?: number;
    showSmart?: 0 | string;
    showToolbar?: 0 | 1;
    toolBarButtonIDs?: string;
    snapDir?: string;
    videoDir?: string;
  };
  instance_params?: {
    szPluginContainer?: string;
    iServicePortStart?: number;
    iServicePortEnd?: number;
    szClassId?: string;
  };
  downloadPluginUrl?: string;
}

const config = {
  // 最大重试次数
  max_retries_count: 2,
  init_params: {
    // appkey
    appkey: "21507178",
    // ip
    ip: "10.10.69.250",
    // 端口
    port: 443,
    // secret
    secret: "jE0EjPQKbFiDmZvpSeyH",
    // 是否启用 HTTPS 协议与平台交互(如指定为非 0 值，则表示使用 HTTPS协议与平台进行网络通信，如未指定该字段或指定为 0，表示使用 HTTP协议与平台进行网络通信。目前仅支持 HTTPS，请填充非 0 值)
    enableHTTPS: 1,
    // 语言
    language: "zh_CN",
    // 窗体布局
    layout: "3x3",
    // 初始播放模式(0-预览 1-回放，如未指定则使用默认值 0，如指定非可选值，返回错误)
    playMode: 0,
    // 重连间隔时间(取值不小于 1 秒，如小于 1 秒，按 1秒计算。不填默认重连间隔时间 5 秒)
    reconnectDuration: 5,
    // 重连次数(0-无限重连 >0-重连次数 <0-不重连，不填默认无限重连)
    reconnectTimes: 5,
    // 是否显示智能信息(不指定或指定字段值为非 0，则默认显示，如指定字段值为 0，则不显示)
    showSmart: 0,
    // 是否显示工具栏(不指定或指定字段值为非 0，则默认显示，如指定字段值为 0，则不显示)
    showToolbar: 1,
    // 显示智能信息
    showIntelligent: 1,
    // buttonIDs: "0,16,256,257,258",
    // 工具栏按钮字符串，多个之间以“,”分割(工具栏上，预览回放通用按钮有：全部静音，全部抓图，全部关闭，切换布局，切换全屏，全部自适应，分隔)
    toolBarButtonIDs:
      "2048,2049,2050,2304,2306,2305,2307,2308,2309,4096,4608,4097,4099,4098,4609,4100",
    // 抓图存储路径
    snapDir: "D:/snap",
    // 紧急录像或录像剪辑存储路径
    videoDir: "D:/video",
  },
  instance_params: {
    szPluginContainer: "playWnd", // 指定 DIV 窗口标识
    iServicePortStart: 15900, // 指定起止端口号，建议使用该值
    iServicePortEnd: 15900,
    szClassId: "23BF3B0A-2C56-4D97-9C03-0CB103AA8F11", // 用于IE10使用ActiveX的clsid
  },
  // 插件下载地址
  downloadPluginUrl: "https://www.hnkancha.com/gzgstatic/VideoWebPlugin.exe",
};

export default config;
export type { IConfig };
