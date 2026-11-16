import type { Router } from 'vue-router'
import { createDiscreteApi } from 'naive-ui'

const { notification } = createDiscreteApi(['notification'])

const title = import.meta.env.VITE_APP_NAME

// let _code: string | null = null  // 单点登录相关，目前已禁用

export function setupRouterGuard(router: Router) {
  const appStore = store.useAppStore()
  const routeStore = store.useRouteStore()
  const tabStore = store.useTabStore()
  const authStore = store.useAuthStore()

  router.beforeEach(async (to, from, next) => {
    // 判断是否是外链，如果是直接打开网页并拦截跳转
    if (to.meta.href) {
      window.open(to.meta.href)
      return false
    }
    // 开始 loadingBar
    appStore.showProgress && window.$loadingBar?.start()

    // 未登录跳转登录页
    if (!authStore.isLogin) {
      // 单点登录，Code换Token (目前已禁用)
      // const urlObj = new URL(window.location.href)
      // const code = urlObj.searchParams.get('code')

      // if (code && code !== _code) {
      //   const res = await authStore.getTokenByCode(code as string)
      //   _code = code
      //   const query = JSON.parse(JSON.stringify(to.query))
      //   delete query.code
      //   delete query.state
      //   next({ path: '/bi/home', query })
      //   if (res === false)
      //     notification.error({ title: '提示', content: '该用户暂无权限', duration: 2000 })
      //   return false
      // }

      if (to.name === 'login') {
        next()
      }
      else {
        const redirect = to.name === '404' ? undefined : to.fullPath
        next({ path: '/login', query: { redirect } })
      }

      return false
    }

    // 判断路由有无进行初始化
    if (!routeStore.isInitAuthRoute) {
      try {
        await routeStore.initAuthRoute()
        // 初始化系统字典
        await appStore.loadSystemDict()
      }
      catch (error: any) {
        console.warn(`[initAuthRoute]-> `, error)
        notification.error({ title: '提示', content: error, duration: 2000 })
        // authStore.clear()
        next('/403')
        return false
      }

      // 动态路由加载完回到根路由
      if (to.name === '404') {
        // 等待权限路由加载好了，回到之前的路由,否则404
        next({
          path: to.fullPath,
          replace: true,
          query: to.query,
          hash: to.hash,
        })
        return false
      }
    }

    next()
  })

  router.beforeResolve((to) => {
    // 设置菜单高亮
    routeStore.setActiveMenu(to.meta.activeMenu ?? to.fullPath)
    // 添加tabs
    tabStore.addTab(to)
    // 设置高亮标签;
    tabStore.setCurrentTab(to.path as string)
  })

  router.afterEach((to) => {
    // 修改网页标题
    document.title = `${to.meta.title} - ${title}`
    // 结束 loadingBar
    window.$loadingBar?.finish()
  })
}
