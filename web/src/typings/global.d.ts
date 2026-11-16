interface Window {
  $loadingBar: import('naive-ui').LoadingBarApi
  $dialog: import('naive-ui').DialogApi
  $message: import('naive-ui').MessageApi
  $notification: import('naive-ui').NotificationApi
  $modal: import('naive-ui').ModalApi

  ShipxyAPI: any
  $map: any
  L: any
  WwLogin: any
  WebControl: any
  VideoPlayer: any
}

declare const AMap: any
declare const BMap: any

declare module '*.vue' {
  import type { DefineComponent } from 'vue'

  const component: DefineComponent
  export default component
}

declare namespace NaiveUI {
  type ThemeColor = 'default' | 'error' | 'primary' | 'info' | 'success' | 'warning'
}
declare namespace Storage {
  interface Session {
    dict: DictMap
  }

  interface Local {
    /* 存储用户信息 */
    userInfo: Api.Login.Info
    /* 存储访问token */
    accessToken: string
    /* 存储刷新token */
    refreshToken: string
    /* 存储登录账号 */
    loginAccount: any
    /* 存储当前语言 */
    lang: App.lang
    /* 登出state */
    logoutState: string
    /* 机密信息模态框显示状态 */
    secret_modal_shown: boolean
    /* 用户退出登录标识 */
    user_logout_flag: boolean
  }
}

declare namespace App {
  type lang = 'zhCN' | 'enUS'
}

interface DictMap {
  [key: string]: Entity.Dict[]
}
