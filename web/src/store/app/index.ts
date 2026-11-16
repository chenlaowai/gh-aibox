import type { GlobalThemeOverrides } from 'naive-ui'
import { colord } from 'colord'
import { set } from 'radash'
import themeConfig from './theme.json'

export type TransitionAnimation = '' | 'fade-slide' | 'fade-bottom' | 'fade-scale' | 'zoom-fade' | 'zoom-out'
export type LayoutMode = 'leftMenu' | 'topMenu' | 'mixMenu'

const docEle = ref(document.documentElement)

const { isFullscreen, toggle } = useFullscreen(docEle)

const { system, store } = useColorMode({
  emitAuto: true,
})

export const useAppStore = defineStore('app-store', {
  state: () => {
    return {
      footerText: import.meta.env.VITE_COPYRIGHT_INFO,
      lang: 'zhCN' as App.lang,
      theme: themeConfig as GlobalThemeOverrides,
      primaryColor: themeConfig.common.primaryColor,
      collapsed: false,
      grayMode: false,
      colorWeak: false,
      loadFlag: true,
      showLogo: true,
      showTabs: true,
      showFooter: false,
      showProgress: true,
      showBreadcrumb: true,
      showBreadcrumbIcon: true,
      showWatermark: false,
      showSetting: false,
      transitionAnimation: 'fade-slide' as TransitionAnimation,
      layoutMode: 'leftMenu' as LayoutMode,
      contentFullScreen: false,
      // 系统字典数据
      systemDict: {} as Record<string, any>,
      // 字典是否已加载
      isDictLoaded: false,
    }
  },
  getters: {
    storeColorMode() {
      return store.value
    },
    colorMode() {
      return store.value === 'auto' ? system.value : store.value
    },
    fullScreen() {
      return isFullscreen.value
    },
    // 获取协议类型枚举选项
    protocolTypeEnum() {
      const protocolType = this.systemDict.protocolTypeEnum?.data || []
      return protocolType.map((item: any) => ({
        label: item.label.split('-')[0], // 取第一部分作为显示名称
        value: item.value,
        fullLabel: item.label, // 保留完整标签用于详细显示
        remark: item.label.split('-').slice(1).join('-'), // 取后面部分作为说明
      }))
    },
    // 获取任务状态枚举选项
    taskStatusEnum() {
      const taskStatus = this.systemDict.taskStatusEnum?.data || []
      return taskStatus.map((item: any) => ({
        label: item.label,
        value: item.value,
      }))
    },
    // 获取区域类型枚举选项
    regionTypeEnum() {
      const regionType = this.systemDict.regionTypeEnum?.data || []
      return regionType.map((item: any) => ({
        label: item.label,
        value: item.value,
      }))
    },
  },
  actions: {
    // 重置所有设置
    resetAlltheme() {
      this.theme = themeConfig
      this.primaryColor = '#2080f0'
      this.collapsed = false
      this.grayMode = false
      this.colorWeak = false
      this.loadFlag = true
      this.showLogo = true
      this.showTabs = true
      this.showFooter = true
      this.showBreadcrumb = true
      this.showBreadcrumbIcon = true
      this.showWatermark = false
      this.transitionAnimation = 'fade-slide'
      this.layoutMode = 'leftMenu'
      this.contentFullScreen = false

      // 重置所有配色
      this.setPrimaryColor(this.primaryColor)
    },
    /* 设置主题色 */
    setPrimaryColor(color: string) {
      const brightenColor = colord(color).lighten(0.05).toHex()
      const darkenColor = colord(color).darken(0.05).toHex()
      set(this.theme, 'common.primaryColor', color)
      set(this.theme, 'common.primaryColorHover', brightenColor)
      set(this.theme, 'common.primaryColorPressed', darkenColor)
      set(this.theme, 'common.primaryColorSuppl', brightenColor)
    },
    setColorMode(mode: 'light' | 'dark' | 'auto') {
      store.value = mode
    },
    /* 切换侧边栏收缩 */
    toggleCollapse() {
      this.collapsed = !this.collapsed
    },
    /* 切换全屏 */
    toggleFullScreen() {
      toggle()
    },
    /**
     * @description: 页面内容重载
     * @param {number} delay - 延迟毫秒数
     * @return {*}
     */
    async reloadPage(delay = 600) {
      this.loadFlag = false
      await nextTick()
      if (delay) {
        setTimeout(() => {
          this.loadFlag = true
        }, delay)
      }
      else {
        this.loadFlag = true
      }
    },
    /* 切换色弱模式 */
    toggleColorWeak() {
      docEle.value.classList.toggle('color-weak')
      this.colorWeak = docEle.value.classList.contains('color-weak')
    },
    /* 切换灰色模式 */
    toggleGrayMode() {
      docEle.value.classList.toggle('gray-mode')
      this.grayMode = docEle.value.classList.contains('gray-mode')
    },
    /* 获取系统字典数据 */
    async loadSystemDict() {
      if (this.isDictLoaded)
        return

      try {
        const [error, res] = await utils.tryit(api.system.getSystemSystemDict)()
        if (error || !res?.isSuccess) {
          console.error('获取系统字典失败:', error)
          return
        }

        this.systemDict = res.data || {}
        this.isDictLoaded = true
      }
      catch (error) {
        console.error('获取系统字典异常:', error)
      }
    },
  },
  persist: {
    storage: localStorage,
  },
})
