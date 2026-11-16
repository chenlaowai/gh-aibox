import { router } from '@/router'

interface IAuthStore {
  rawAccessToken: string
  accessToken: string
  refreshToken: string
  idToken?: string
  state?: string
  userInfo: Partial<API.UserInfo>
  _isLoggingOut: boolean
}
export const useAuthStore = defineStore('auth-store', {
  state: (): IAuthStore => {
    return {
      rawAccessToken: '',
      accessToken: '',
      refreshToken: '',
      userInfo: {},
      _isLoggingOut: false,
    }
  },
  getters: {
    isLogin(state) {
      return Boolean(state.accessToken)
    },
  },
  actions: {
    // 获取用户信息
    async getUserInfo() {
      const [error, r1] = await utils.tryit(api.user.getUserUserInfo)()
      if (error || !r1.data)
        return

      this.userInfo = r1.data

      return r1.data
    },

    // 账密登录
    async login(account: string, password: string, sliderCaptcha: number[]) {
      const [error, r1] = await utils.tryit(api.auth.postAuthLogin)({ account, password, sliderCaptcha })
      if (error || !r1.data)
        return
      this.saveToken(r1.data.accessToken!, r1.data.refreshToken!)

      // 进行重定向跳转
      const route = unref(router.currentRoute)
      const query = route.query as { redirect: string }
      router.replace({
        path: query.redirect || '/bi/home',
      })
    },
    // 刷新token
    async getRefreshToken() {
      const [error, r1] = await utils.tryit(api.auth.postAuthRefreshToken)({ refreshToken: this.refreshToken }, { hideErrMsg: true })
      // 检查 code === 200 和 isSuccess
      if (error || !r1.isSuccess) {
        // 如果刷新失败，抛出错误让调用者处理（或者直接返回 false）
        return Promise.reject(error || new Error(r1?.message || '刷新Token失败'))
      }

      this.saveToken(r1.data.accessToken!, r1.data.refreshToken!)

      return r1.data
    },

    saveToken(accessToken: string, refreshToken: string, idToken?: string) {
      this.rawAccessToken = accessToken
      this.accessToken = `Bearer ${accessToken}`
      this.refreshToken = refreshToken
      if (this.idToken)
        this.idToken = idToken
    },

    clear() {
      this.accessToken = ''
      this.refreshToken = ''
      this.idToken = ''
      this.userInfo = {}

      const routeStore = store.useRouteStore()
      routeStore.resetRouteStore()

      const tabStore = store.useTabStore()
      tabStore.clearAllTabs()
    },

    async logout() {
      // 防止重复调用logout（避免死循环）
      if (this._isLoggingOut) {
        console.warn('重复调用logout，跳过')
        return
      }

      this._isLoggingOut = true

      try {
        // 如果没有 token，直接跳过后端请求，进行本地清理
        if (this.accessToken) {
          const [error, r1] = await utils.tryit(api.auth.postAuthLogout)({
            refreshToken: this.refreshToken,
          })

          // 401 也认为是退出成功（因为本身就是 token 失效）
          if (error && error.code !== '401' && (!r1 || r1.code !== 200)) {
            console.error('退出登录接口调用失败:', error || r1)
          }
          else if (r1 && r1.isSuccess) {
            window.$message.success(r1.message || '退出登录成功')
          }
        }
      }
      catch (error) {
        console.error('退出登录过程异常:', error)
      }
      finally {
        this.clear()
        this._isLoggingOut = false
        // 只有当不在登录页时才跳转
        if (router.currentRoute.value.path !== '/login') {
          router.replace({ path: '/login', query: { ...router.currentRoute.value.query } })
        }
      }
    },
  },

  persist: true,
})
