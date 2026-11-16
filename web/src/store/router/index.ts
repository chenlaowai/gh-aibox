import type { MenuOption } from 'naive-ui'
import { createMenus, createRoutes, generateCacheRoutes } from './helper'
import { router } from '@/router'
import { staticRoutes } from '@/router/routes.static'

interface RoutesStatus {
  isInitAuthRoute: boolean
  menus: MenuOption[]
  rowRoutes: AppRoute.RowRoute[]
  activeMenu: string | null
  cacheRoutes: string[]
}

export const useRouteStore = defineStore('route-store', {
  state: (): RoutesStatus => {
    return {
      isInitAuthRoute: false,
      activeMenu: null,
      menus: [],
      rowRoutes: [],
      cacheRoutes: [],
    }
  },
  actions: {
    resetRouteStore() {
      this.resetRoutes()
      this.$reset()
    },
    resetRoutes() {
      if (router.hasRoute('appRoot'))
        router.removeRoute('appRoot')
    },
    // set the currently highlighted menu key
    setActiveMenu(key: string) {
      this.activeMenu = key
    },

    async initRouteInfo() {
      const authStore = store.useAuthStore()
      if (import.meta.env.VITE_AUTH_ROUTE_MODE === 'dynamic') {
        try {
          const data = await authStore.getUserInfo() as any

          if (!data?.permission?.menus)
            return []

          return data.permission.menus
        }
        catch (error) {
          return error
        }
      }
      else {
        await authStore.getUserInfo()
        this.rowRoutes = staticRoutes
        return staticRoutes
      }
    },

    async initAuthRoute() {
      this.isInitAuthRoute = false

      try {
        // 初始化路由信息
        const rowRoutes = await this.initRouteInfo()

        if (!rowRoutes?.length)
          throw new Error('当前用户无权限')

        this.rowRoutes = rowRoutes

        // 生成路由并插入
        const routes = createRoutes(rowRoutes)
        router.addRoute(routes)

        // 侧边栏菜单项
        this.menus = createMenus(rowRoutes)

        // 生成路由缓存
        this.cacheRoutes = generateCacheRoutes(rowRoutes)
      }
      catch (error: any) {
        console.log(`output->error`, error)
        throw error.message || error.msg
      }
      finally {
        this.isInitAuthRoute = true
      }
    },
  },
})
