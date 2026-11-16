export const staticRoutes: AppRoute.RowRoute[] = [
  {
    name: 'index',
    path: '/index',
    url: '/index',
    title: '广航AI盒子展示平台',
    requiresAuth: true,
    icon: 'local:map',
    componentPath: '/index/IndexView.vue',
    id: '1',
    parentId: undefined,
  },
]
