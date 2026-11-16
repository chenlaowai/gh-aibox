import debounce from 'lodash/debounce'
import config from '@/hooks/shipxy/config'

type ListItem = API.ProjectAndShipItemResult & { videoType: 0 | 1, url: string, pid?: string }

export function useProjectAndShip(itemClick?: (item: ListItem) => void) {
  const markers = ref<Array<any>>([])
  const videoMarkers = ref<Array<any>>([])
  const zoomChangeEventRef = shallowRef()
  const baselayerchangeEventRef = shallowRef()
  const loading = shallowRef(false)
  const showWmsZoom = 10
  let isInit = true
  let timer: any
  let wmss: Array<any> = []

  const icons = [
    [new URL('@/assets/images/img_32.png', import.meta.url).href, new URL('@/assets/images/img_37.png', import.meta.url).href],
    [new URL('@/assets/images/img_8.png', import.meta.url).href, new URL('@/assets/images/img_11.png', import.meta.url).href],
    [new URL('@/assets/images/icon-video.png', import.meta.url).href],
  ]

  function marker(list: Array<ListItem>) {
    // 缩小时的点
    const dotMarkers: any = []
    // 项目部或船舶icon
    const iconMarkers: any = []
    // 波纹动画
    const warningDotMarkers: any = []

    for (const item of list) {
      // 添加点
      const dotMarker = window.L.marker([item.lat, item.lon], {
        // opacity: 1,
        icon: window.L.divIcon({
          html: `
            <div class="circle">
              <div class="circle-point ${item?.isWarning ? 'error' : 'success'}"></div>
              <div class="circle-point ${item?.isWarning ? 'error' : 'success'}"></div>
              <div class="circle-point ${item?.isWarning ? 'error' : 'success'}"></div>
            </div>
          `,
          className: 'div-icon',
          iconSize: [8, 8],
        }),
      })

      dotMarker.bindTooltip(item.name, {
        permanent: false,
        direction: 'top',
        offset: window.L.point(0, -4),
      })

      dotMarker.on('click', () => {
        focus([item.lat, item.lon], config.zoom + 5)
      })

      dotMarker.addTo(window.$map)
      dotMarkers.push({ marker: dotMarker, item })

      // 预警标识
      const warningDotMarker = window.L.marker([item.lat, item.lon], {
        // opacity: 1,
        icon: window.L.divIcon({
          html: `
              <div class="circle">
                <div class="circle-point ${item?.isWarning ? 'error' : 'success'}" style="width: 20px; height: 20px"></div>
                <div class="circle-point ${item?.isWarning ? 'error' : 'success'}" style="width: 20px; height: 20px"></div>
                <div class="circle-point ${item?.isWarning ? 'error' : 'success'}" style="width: 20px; height: 20px"></div>
              </div>
            `,
          className: 'div-icon',
          iconSize: [30, 30],
        }),
      })

      warningDotMarkers.push({ marker: warningDotMarker, item })

      // 添加icon
      const iconMarker = window.L.marker([item.lat, item.lon], {
        // opacity: 1,
        icon: window.L.icon({
          iconUrl: icons[item.dataType!][item?.isWarning ? 1 : 0],
          iconSize: [32, 42],
        }),
      })

      iconMarker.bindTooltip(item.name, {
        permanent: true,
        direction: 'top',
        offset: window.L.point(0, -22),
      })

      iconMarkers.push({ marker: iconMarker, item })
    }

    markers.value = [dotMarkers, iconMarkers, warningDotMarkers]
  }

  async function handlePopup(marker, item: API.ProjectAndShipItemResult) {
    const [error, r1] = await utils.tryit(api.statistics.getStatisticsGetProjectAndShipSynopsis)({
      relationType: item.dataType as 0 | 1,
      id: item.id!,
    })

    if (error || !r1.data) {
      window.$message.error(r1?.message || '暂无数据')
      return
    }

    const _html = `
          <div class="info-window">
            <div class="info-window-head">
              <div class="info-window-title" title="${item.name}">${item.name}</div>
            </div>
            <div class="info-window-body">
                <table style="border-collapse:separate">
                  <tbody>
                    <tr class="table-row">
                      <td class="flex">
                        <span class="label c-#fff">
                          所属公司
                        </span>
                        <span class="value c-#fff">
                          <span>${r1.data.companyName}</span>
                        </span>
                      </td>
                    </tr>
                    <tr class="table-row">
                      <td class="flex">
                        <span class="label c-#fff">
                          传 感 器
                          <span class="n-descriptions-separator"></span>
                        </span>
                        <span class="value c-#fff">
                          <span >${r1.data.onlineSensorCount}/${(r1.data.onlineSensorCount ?? 0) + ((r1.data.offlineSensorCount ?? 0))}(在线/总数)</span>
                        </span>
                      </td>
                    </tr>
                    <tr class="table-row">
                      <td class="flex">
                        <span class="label c-#fff">
                          摄 像 头
                          <span class="n-descriptions-separator"></span>
                        </span>
                        <span class="value c-#fff">
                          <span>${r1.data.onlineVideoCount}/${(r1.data.onlineVideoCount ?? 0) + ((r1.data.offlineVideoCount ?? 0))}(在线/总数)</span>
                        </span>
                      </td>
                    </tr>
                    <tr class="table-row" style="display: ${r1.data.uavCount ? 'block' : 'none'}">
                      <td class="flex">
                        <span class="label c-#fff">
                          无 人 机
                          <span class="n-descriptions-separator"></span>
                        </span>
                        <span class="value c-#fff">
                          <span>${r1.data.uavCount}架</span>
                        </span>
                      </td>
                    </tr>
                    <tr class="table-row">
                      <td class="flex">
                        <span class="label c-#fff">
                          今日预警
                          <span class="n-descriptions-separator"></span>
                        </span>
                        <span class="value c-#fff">
                          <span>${r1.data.toDayWarningCount}次</span>
                        </span>
                      </td>
                    </tr>
                  </tbody>
                </table>
              <div class="btn" onClick="handlePointClick('${item.id}', '${item.name}', '${r1.data.route}')">查看详情</div>
            </div>
          </div>`

    marker.bindPopup(_html).openPopup()
  }

  function focus([lat, lng], zoom = config.zoom, duration = 0.5) {
    window.$map.flyTo([lat, lng], zoom, { duration })
  }

  function focusAndShowVideo(item: API.ProjectAndShipItemResult, zoom = config.zoom) {
    const currentZoom = window.$map.getZoom()
    if (currentZoom === zoom)
      return

    focus([item.lat, item.lon], zoom)
    removeVideos()
    showVideos(item)
  }

  function focusAndShowPopup(item: API.ProjectAndShipItemResult, zoom = config.zoom) {
    const target = markers.value[1].find(val => val.item.id === item.id)
    if (!target)
      return

    focus([item.lat, item.lon], zoom)
    removeVideos()
    showVideos(item)

    setTimeout(() => {
      handlePopup(target.marker, target.item)
    }, 1000)
  }

  function onZoomChange() {
    if (!markers.value?.length)
      return
    const currentZoom = window.$map?.getZoom()
    const dotMarkers = toRaw(markers.value[0])
    const iconMarkers = toRaw(markers.value[1])
    const warningDotMarkers = toRaw(markers.value[2])
    removeAllMarkers()

    if (currentZoom <= config.zoom) {
      removeMarkers(iconMarkers, 1)
      removeMarkers(warningDotMarkers, 2)
      showMarkers(filter(dotMarkers), 0)
    }
    else {
      removeMarkers(dotMarkers, 0)
      showMarkers(filter(iconMarkers), 1)
      showMarkers(filter(warningDotMarkers), 2)
    }

    if (currentZoom >= showWmsZoom) {
      removeWmssLayer()
      addWmssLayer(config.defaultMapType as any)
    }
    else if (currentZoom < showWmsZoom) {
      removeWmssLayer()
    }

    if (currentZoom <= config.zoom + 3) {
      removeVideos()
    }
  }

  function showVideos(item: API.ProjectAndShipItemResult) {
    const items = recursion(toRaw(markers.value[0]).map(val => val.item))
    const _markers = items.filter(val => val.pid === item.id)

    for (const iterator of _markers) {
      const videoMarker = window.L.marker([iterator.lat, iterator.lon], {
        icon: window.L.icon({
          iconUrl: icons[2][0],
          iconSize: [32, 81],
        }),
      })

      videoMarker.bindTooltip(iterator.name, {
        permanent: true,
        direction: 'top',
        offset: window.L.point(0, -42),
      })

      videoMarker.on('click', () => itemClick && itemClick(iterator))
      videoMarker.setZIndexOffset(-1)
      videoMarker.addTo(window.$map)

      videoMarkers.value.push(videoMarker)
    }
  }

  function removeVideos() {
    const _videoMarkers = toRaw(videoMarkers.value)
    if (!_videoMarkers.length)
      return

    _videoMarkers.forEach((val: any) => {
      val.off('click')
      val.unbindTooltip()
      val.remove()
    })
    videoMarkers.value = []
  }

  function showMarkers(_markers: any, index: number) {
    const currentZoom = window.$map.getZoom()
    for (const iterator of _markers) {
      if (index === 0) {
        iterator.marker.bindTooltip(iterator.item.name, { permanent: false, direction: 'top', offset: window.L.point(0, -4) })
        iterator.marker.on('click', () => {
          focus([iterator.item.lat, iterator.item.lon], config.zoom + 5)
        })
      }
      else if (index === 1) {
        iterator.marker.bindTooltip(iterator.item.name, { permanent: currentZoom >= config.zoom + 5, direction: 'top', offset: window.L.point(0, -22) })
        iterator.marker.on('click', () => {
          focusAndShowVideo(iterator.item, config.zoom + 10)
          setTimeout(() => {
            handlePopup(iterator.marker, iterator.item)
          }, 1000)
        })
      }
      else if (index === 2) {
        iterator.marker.setZIndexOffset(-1)
      }

      iterator.marker.addTo(window.$map)
    }
  }

  function removeMarkers(_markers: any, index: number) {
    for (const iterator of _markers) {
      if (index === 0 || index === 1) {
        iterator.marker.unbindTooltip()
        iterator.marker.off('click')
      }

      iterator.marker.remove()
    }
  }

  function filter(markers: any) {
    const currentZoom = window.$map.getZoom()
    if (currentZoom <= 3) {
      return markers.filter((_: any, i: number) => i % 6 === 0)
    }
    else if (currentZoom > 3 && currentZoom <= 5) {
      return markers.filter(
        (_: any, i: number) => i % 2 === 0 || i % 3 === 0 || i % 5 === 0 || i % 6 === 0,
      )
    }
    else if (currentZoom > 4 && currentZoom < 6) {
      return markers.filter(
        (_: any, i: number) =>
          i % 2 === 0 || i % 3 === 0 || i % 5 === 0 || i % 6 === 0 || i % 7 === 0,
      )
    }
    else {
      return markers
    }
  }

  // 查询地图上项目和船数据
  async function getStatisticsGetProjectAndShip() {
    const commonStore = store.useCommonStore()
    const [error, r1] = await utils.tryit(api.statistics.getStatisticsGetProjectAndShip)({ companyId: commonStore.companyId })

    if (error || !r1?.data)
      return

    return recursion(r1.data)
  }

  function recursion(nodes: Array<API.ProjectAndShipItemResult>) {
    if (!nodes?.length)
      return
    const arr: any = []
    _recursion(nodes)
    function _recursion(nodes: Array<API.ProjectAndShipItemResult>) {
      for (let i = 0; i < nodes.length; i++) {
        if (nodes[i].videoDeviceList) {
          const _list = nodes[i].videoDeviceList?.map(val => ({ ...val, pid: nodes[i].id }))
          _recursion(_list as any)
        }
        arr.push(nodes[i])
      }
    }
    return arr
  }

  function addWmssLayer(type: 'MT_SEA' | 'MT_GOOGLE' | 'MT_SATELLITE') {
    const geoserver = 'http://10.10.140.23:8088/geoserver'
    // 象山图层
    // const wmss1 = [
    //   'chaogoushujun_4326',
    //   'diqianzhongzhi_4326',
    //   'haidixiufu_4326',
    //   'huhuamicao_4326',
    //   'mulijiaoxiufu_4326',
    //   'shengwuduoyangxing_4326'
    // ]

    // 广州港图层
    const wmss2 = {
      MT_SEA: ['gzgpolyline_4326', 'gzgpoint_4326'],
      MT_SATELLITE: ['gzgpolyline_4326google', 'gzgpoint_4326google'],
      MT_GOOGLE: ['gzgpolyline_4326google', 'gzgpoint_4326google'],
    }

    const common = {
      format: 'image/png',
      transparent: true,
      version: '1.3.0',
      // wms: window.L.CRS.EPSG3857
    }

    let wms: any

    for (const item of wmss2[type]) {
      wms = window.L.tileLayer.wms(`${geoserver}/gzg/wms`, {
        layers: `gzg:${item}`,
        ...common,
      })
      wms.addTo(window.$map)

      wmss.push(wms)
    }
  }

  function baselayerchangeEvent(layer: any) {
    const type = layer?.options.id
    removeWmssLayer()
    addWmssLayer(type)
  }

  function _getStatisticsGetProjectAndShip() {
    removeAllMarkers()
    markers.value = []
    loading.value = true

    getStatisticsGetProjectAndShip()
      .then((res) => {
        if (!res?.length)
          return

        marker(res.filter(val => !val.pid))
        removeVideos()

        if (isInit) {
          zoomChangeEventRef.value = debounce(onZoomChange, 500)

          window.$map.on('zoomend', zoomChangeEventRef.value)
          window.$map.on('baselayerchange', baselayerchangeEvent)

          timer = setInterval(() => {
            getStatisticsGetProjectAndShip()
          }, 5 * 60 * 1000)
        }

        isInit = false
      }).finally(() => loading.value = false)
  }

  function init() {
    if (loading.value) {
      window.$message.warning('操作频繁,请稍后再试')
      return
    }

    _getStatisticsGetProjectAndShip()
  }

  // 重置地图层级
  function resetZoom() {
    window.$map?.setZoom(config.zoom)
  }

  // 移除事件监听
  function removeEventListener() {
    if (zoomChangeEventRef.value) {
      window.$map.off('zoomend', zoomChangeEventRef.value)
      zoomChangeEventRef.value = null
    }

    window.$map.off('baselayerchange', baselayerchangeEventRef.value)
  }

  // 清除定时器
  function removeInterval() {
    if (timer) {
      clearInterval(timer)
      timer = null
    }
  }

  // 移除CAD图层
  function removeWmssLayer() {
    if (!wmss.length)
      return
    for (const item of wmss) {
      item.remove()
    }
    wmss = []
  }

  // 移除地图上的点位
  function removeAllMarkers() {
    const _markers = toRaw(markers.value)

    for (let i = 0; i < _markers.length; i++) {
      for (let j = 0; j < _markers[i].length; j++) {
        if (i === 0 || i == 1) {
          _markers[i][j].marker.unbindTooltip()
          _markers[i][j].marker.off('click')
        }

        _markers[i][j].marker?.remove()
      }
    }
  }

  function removeMap() {
    window.$map?.remove()
    window.$map = undefined
  }

  function destroy() {
    removeInterval()
    removeEventListener()
    removeWmssLayer()
    removeAllMarkers()
    removeMap()
  }

  return {
    init,
    focus,
    focusAndShowPopup,
    focusAndShowVideo,
    resetZoom,
    destroy,
  }
}
