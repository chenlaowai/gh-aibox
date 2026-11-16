import { clone, construct, min, omit, pick } from 'radash'
import type { RouteRecordRaw } from 'vue-router'
import type { MenuOption } from 'naive-ui'
import { RouterLink } from 'vue-router'
import Layout from '@/layouts/BasicLayout.vue'
import { arrayToTree, renderIcon } from '@/utils'

const metaFields: AppRoute.MetaKeys[]
  = ['title', 'icon', 'requiresAuth', 'roles', 'keepAlive', 'isHide', 'sort', 'href', 'activeMenu', 'withoutTab', 'pinTab', 'menuType']

function standardizedRoutes(route: AppRoute.RowRoute[]) {
  return clone(route).map((i) => {
    const route = omit(i, metaFields)
    Reflect.set(route, 'meta', pick(i, metaFields))
    return route
  }) as AppRoute.Route[]
}

export function createRoutes(routes: AppRoute.RowRoute[]) {
  // 构造元字段
  let resultRouter = standardizedRoutes(routes)

  // 生成路由，无需为重定向的路由导入文件
  const modules = import.meta.glob('@/views/**/*.vue')

  resultRouter = resultRouter.map((item: AppRoute.Route) => {
    item.path = item.url!
    item.meta.title = item.name!
    item.component = modules[`/src/views${item.componentPath}`]

    delete item.name
    delete item.componentPath
    delete item.url

    return item
  })

  // 生成路由配置
  resultRouter = arrayToTree(resultRouter) as AppRoute.Route[]
  const appRootRoute: RouteRecordRaw = {
    path: '/appRoot',
    name: 'appRoot',
    redirect: resultRouter?.[0]?.redirect || resultRouter?.[0]?.path || import.meta.env.VITE_HOME_PATH,
    component: Layout,
    meta: {
      title: '',
      icon: 'icon-park-outline:home',
    },
    children: [],
  }

  // 设置重定向
  setRedirect(resultRouter)

  // 将处理后的路由插入到根路由
  appRootRoute.children = resultRouter as unknown as RouteRecordRaw[]

  return appRootRoute
}

// 生成需要保持活动的路由名称数组
export function generateCacheRoutes(routes: AppRoute.RowRoute[]) {
  return routes
    .filter(i => i.keepAlive)
    .map(i => i.name)
}

function setRedirect(routes: AppRoute.Route[]) {
  routes.forEach((route) => {
    if (route.children) {
      if (!route.redirect) {
        // 过滤掉未隐藏的子元素集合
        const visibleChilds = route.children.filter(child => !child.meta.isHide)

        // 默认情况下，将页面重定向到第一个子元素的路径
        let target = visibleChilds[0]
        // 过滤出=具有排序字段的页面
        const orderChilds = visibleChilds.filter(child => child.meta.sort)

        if (orderChilds.length > 0)
          target = min(orderChilds, i => i.meta.sort!) as AppRoute.Route

        if (target) {
          route.redirect = target.path
        }
      }

      setRedirect(route.children)
    }
  })
}

/* 生成侧边菜单的数据 */
export function createMenus(userRoutes: AppRoute.RowRoute[]) {
  const resultMenus = clone(userRoutes).map(i =>
    construct(i),
  ) as AppRoute.RowRoute[]

  // filter menus that do not need to be displayed
  const visibleMenus = resultMenus.filter(route => !route.isHide)
  // generate side menu
  return arrayToTree(transformAuthRoutesToMenus(visibleMenus) as any)
}

// 将返回的路由表呈现为侧边栏
function transformAuthRoutesToMenus(userRoutes: AppRoute.RowRoute[]): MenuOption[] {
  // Filter out side menus without permission
  return userRoutes
    // .filter(i => hasPermission(i.meta.roles))
    //  Sort the menu according to the order size
    .sort((a, b) => {
      if (a.sort && b.sort)
        return a.sort - b.sort
      else if (a.sort)
        return -1
      else if (b.sort)
        return 1
      else return 0
    })

    // 转换为侧边栏菜单数据结构
    .map((item) => {
      const target: MenuOption = {
        id: item.id,
        parentId: item.parentId,
        label:
                !item.isCatalog
                  ? () =>
                      h(
                        RouterLink,
                        {
                          to: {
                            path: item.url,
                          },
                        },
                        {
                          default: () => item.name,
                        },
                      )
                  : () => item.name,
        key: item.url,
        icon: item.icon ? renderIcon(item.icon) : undefined,
      }
      return target
    })
}
