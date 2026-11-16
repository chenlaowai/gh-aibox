/**
 * 在组件当中使用示例：
 * <script setup lang="ts">
      import { useMars3d } from '@/hooks'

      const el = 'mars3d-container-guanghang'
      const mars = useMars3d()

      onMounted(() => {
        mars.init(el)
      })
    </script>

    <template>
      <div :id="el" class="wh-full" />
    </template>
 */

import * as mars3d from 'mars3d'
import config from './config.json'

// 用于存放地球组件实例
let map: mars3d.Map | null, graphicLayer: any, terrainClip: any

export function useMars3d() {
  // 返回地图对象map和矢量图层对象graphicLayer
  function init(el: string, options = config.map3d as any) {
    // options.scene.center = mapStore.getProjectCenter().center

    map = new mars3d.Map(el, options)
    graphicLayer = new mars3d.layer.GraphicLayer()

    map.addLayer(graphicLayer)

    terrainClip = new mars3d.thing.TerrainClip({
      // image: 'http://mars3d.cn/img/textures/circle-two.png',
      splitNum: 80, // 井边界插值数
    })
    map.addThing(terrainClip)

    // 开场动画
    // map.openFlyAnimation();

    // 针对不同终端的优化配置
    if (mars3d.Util.isPCBroswer()) {
      map.zoomFactor = 2.0 // 鼠标滚轮放大的步长参数

      // IE浏览器优化
      if (window.navigator.userAgent.toLowerCase().includes('msie')) {
        map.viewer.targetFrameRate = 20 // 限制帧率
        map.scene.requestRenderMode = false // 取消实时渲染
      }
    }
    else {
      map.zoomFactor = 5.0 // 鼠标滚轮放大的步长参数

      // 移动设备上禁掉以下几个选项，可以相对更加流畅
      map.scene.requestRenderMode = false // 取消实时渲染
      map.scene.fog.enabled = false
      map.scene.skyAtmosphere.show = false
      map.scene.globe.showGroundAtmosphere = false
    }

    // 二三维切换不用动画
    if (map.viewer.sceneModePicker) {
      map.viewer.sceneModePicker.viewModel.duration = 0.0
    }

    // webgl渲染失败后，刷新页面
    map.on(mars3d.EventType.renderError, () => {
      window.$message.warning('程序内存消耗过大，请重启浏览器')
      window.location.reload()
    })

    return { map, graphicLayer, terrainClip }
  }

  // 雨效果
  function raining(speed = 20, size = 20, direction = 10) {
    const rainEffect = new mars3d.effect.Rain({
      speed,
      size,
      direction,
    })
    map?.addEffect(rainEffect)
    return rainEffect
  }

  // 雪效果
  function snowing(speed = 20, scale = 10, maxHeight = 9000000) {
    const snowingEffect = new mars3d.effect.Snow({
      speed,
      scale,
      maxHeight,
    })
    map?.addEffect(snowingEffect)
    return snowingEffect
  }

  function flyTo(values: {
    lng: string | number
    lat: string | number
    alt?: string | number
    heading?: string | number | undefined
    pitch?: string | number | undefined
    range?: string | number | undefined
    duration?: string | number | undefined
  }, _callback?: any) {
    if (!(values.lng && values.lat) || !map)
      return
    const lng = typeof values.lng === 'string' ? Number(values.lng) : values.lng
    const lat = typeof values.lat === 'string' ? Number(values.lat) : values.lat
    const alt = typeof values.alt === 'string' ? Number(values.alt) : values.alt || 0
    const heading = values.heading ? toRadians(values.heading) : toRadians(config.map3d.scene.center.heading)
    const pitch = values.pitch ? toRadians(values.pitch) : toRadians(config.map3d.scene.center.pitch)
    const range = typeof values.range === 'string' ? Number(values.range) : values.range || 0
    const duration = typeof values.duration === 'string' ? Number(values.duration) : (values.duration != null ? values.duration : 1)

    map?.camera.flyToBoundingSphere(new mars3d.Cesium.BoundingSphere(
      mars3d.Cesium.Cartesian3.fromDegrees(lng, lat),
      alt,
    ), {
      duration,
      offset: new mars3d.Cesium.HeadingPitchRange(heading, pitch, range),
    })
  }

  function toRadians(value: number | string) {
    value = typeof value === 'string' ? Number(value) : value
    return mars3d.Cesium.Math.toRadians(value)
  }

  function destroy() {
    if (map) {
      map.destroy()
      map = null
    }
  }

  return {
    // ...toRefs(state),
    init,
    destroy,
    flyTo,
    raining,
    snowing,
    toRadians,
    config,
  }
}

export default mars3d
