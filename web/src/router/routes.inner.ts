import type { RouteRecordRaw } from 'vue-router'

/* 页面中的一些固定路由，错误页等 */
export const routes: RouteRecordRaw[] = [
  {
    path: '/',
    name: 'root',
    redirect: '/bi/home',
    children: [],
  },
  {
    path: '/bi',
    name: 'aiDashboard',
    component: () => import('@/views/bi/IndexView.vue'),
    meta: {
      title: '广航AI盒子展示平台',
    },
    children: [
      {
        path: 'home',
        name: 'home',
        component: () => import('@/views/bi/home/Index.vue'),
        meta: {
          title: '首页',
        },
      },
      {
        path: 'channel-management',
        name: 'channelManagement',
        component: () => import('@/views/bi/channel-management/Index.vue'),
        meta: {
          title: '通道管理',
        },
      },
      {
        path: 'task-management',
        name: 'taskManagement',
        component: () => import('@/views/bi/task-management/Index.vue'),
        meta: {
          title: '任务管理',
        },
      },
      {
        path: 'task-management/edit/:id?',
        name: 'taskManagementEdit',
        component: () => import('@/views/bi/task-management/AddOrEdit.vue'),
        meta: {
          title: '任务管理',
          activeMenu: '/bi/task-management',
        },
      },
      {
        path: 'task-management/config/:id?',
        name: 'taskManagementConfig',
        component: () => import('@/views/bi/task-management/Config.vue'),
        meta: {
          title: '任务管理',
          activeMenu: '/bi/task-management',
        },
      },
      {
        path: 'current-preview',
        name: 'currentPreview',
        component: () => import('@/views/bi/current-preview/Index.vue'),
        meta: {
          title: '实时预览',
        },
      },
      {
        path: 'alarm-preview',
        name: 'alarmPreview',
        component: () => import('@/views/bi/alarm-preview/Index.vue'),
        meta: {
          title: '告警预览',
        },
      },
      {
        path: 'parameter-config',
        name: 'parameterConfig',
        component: () => import('@/views/bi/parameter-config/Index.vue'),
        meta: {
          title: '参数配置',
        },
      },
      {
        path: 'other-config',
        name: 'otherConfig',
        component: () => import('@/views/bi/other-config/Index.vue'),
        meta: {
          title: '其他配置',
        },
      },
    ],
  },
  {
    path: '/login',
    name: 'login',
    component: () => import('@/views/login/Index.vue'), // 注意这里要带上 文件后缀.vue
    meta: {
      title: '登录',
      withoutTab: true,
    },
  },
  {
    path: '/logincallback',
    name: 'logincallback',
    component: () => import('@/views/error/LoginCallback.vue'),
    meta: {
      title: '登录回调',
      icon: 'icon-park-outline:logout',
    },
  },
  {
    path: '/403',
    name: '403',
    component: () => import('@/views/error/NoPermission.vue'),
    meta: {
      title: '用户无权限',
    },
  },
  {
    path: '/404',
    name: '404',
    component: () => import('@/views/error/NotFound.vue'),
    meta: {
      title: '找不到页面',
      icon: 'icon-park-outline:ghost',
    },
  },
  {
    path: '/500',
    name: '500',
    component: () => import('@/views/error/ServerError.vue'),
    meta: {
      title: '服务器错误',
      icon: 'icon-park-outline:close-wifi',
    },
  },
  {
    path: '/:pathMatch(.*)*',
    component: () => import('@/views/error/NotFound.vue'),
    name: '404',
    meta: {
      title: '找不到页面',
      icon: 'icon-park-outline:ghost',
    },
  },
]
