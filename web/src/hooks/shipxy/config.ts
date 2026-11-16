const config = {
  attribution: { isShow: false, emptyString: "" }, // 公司版权信息( 支持html )，默认Elane Inc.
  mapTypes: ["MT_SEA", "MT_GOOGLE", "MT_SATELLITE"], // 显示地图类型，'MT_SEA'，MT_GOOGLE,MT_SATELLITE,
  ak: import.meta.env.VITE_SHIPXY_KEY, // 授权码
  defaultMapType: "MT_SATELLITE", // 默认展示地图类型
  gratings: { isShow: false, maxZoom: 9, type: "RASTER_WORLD" }, // 光栅图,是否显示，最大显示级别,默认:false
  centerPoint: [22.8, 114.13528] as number[], // 初始中心点坐标
  zoom: 5, // 初始缩放级别
  minZoom: 2, // 最小缩放级别
  maxZoom: 18, // 最大缩放级别
  measureCtrl: { isShow: false, position: "bottomright" }, // 测量控件的显示隐藏
  mousePostionCtrl: { isShow: false, position: "bottomright" }, // 鼠标移动悬浮经纬度控件
  zoomControlElane: { isShow: false, position: "topright" }, // 缩放控件的显示隐藏
  zoomviewControl: { isShow: false, position: "topleft" }, // 缩放级别显示控件
  basemapsControl: { isShow: false, position: "bottomright" }, // 地图切换控件的位置
  scaleCtrl: { isShow: false, position: "bottomleft" }, // 比例尺，控件
  addTileLayer: [], // 自定义图源
  cjhdTileLayer: { isShow: false }, // 是否显示长江航道，默认：false,
  miniMapControl: { isShow: false, options: {} }, // 鹰眼控件
  // 默认图源设置（url，errorTileUrl），默认图源类型有：sea、google、satellite
  tileLayer: {
    sea: {
      url: "//m12.shipxy.com/tile.c?l=Na&m=o&x={x}&y={y}&z={z}",
      errorTileUrl: "//m.shipxy.com/img/erroshipxy.png",
    },
    google: {
      url: "//mt2.google.cn/vt/lyrs=m@180000000&hl=zh-CN&gl=cn&src=app&x={x}&y={y}&z={z}&s=Gal",
      errorTileUrl: "//m.shipxy.com/img/erroshipxy.png",
    },
    satellite: {
      url: {
        12: {
          // 船讯网瓦片
          url: "//g1.shipxy.com/tile.g?m=1&x={x}&y={y}&z={z}",
        },
        18: {
          // 谷歌卫星
          url: "//g1.shipxy.com/tile.g?m=1&x={x}&y={y}&z={z}",
        },
      },
      errorTileUrl: "//m.shipxy.com/img/erroshipxy.png",
    },
  },
};

export default config;
