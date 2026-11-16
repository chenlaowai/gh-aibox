/**
 * 组件当中使用示例：
 <script setup lang="ts">
    import { useShipxy } from '@/hooks'
 
    const shipxy = useShipxy()
 
    onMounted(shipxy.init)
  </script>
 
  <template>
    <div id="map" class="wh-full" />
  </template>
 */

import config from './config'

export interface Map {
  [key: string]: any
}

export function useShipxy() {
  function init() {
    return new Promise((resolve) => {
      // eslint-disable-next-line no-new
      new window.ShipxyAPI.Map('map', {
        ...config,
        mapReadyCallBack: (map: any) => {
          window.$map = map

          resolve(map)
        },
      })
    })
  }

  function destroy() {
    window.$map.trackService?.removeAll()
  }

  onBeforeUnmount(destroy)

  return {
    init,
    destroy,
  }
}
