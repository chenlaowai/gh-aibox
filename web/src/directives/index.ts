import type { App } from 'vue'
// import { copy } from './copy'
// import { permission } from './permission'
import { autofit } from './autofit'

/**
 * 注册全局指令
 */
export function setupDirectives(app: App) {
  // 复制指令
//   app.directive('copy', copy)

  // 权限指令
  //   app.directive('permission', permission)

  // autofit缩放适配指令
  app.directive('autofit', autofit)
}

// 导出各个指令，以便按需使用
export { autofit }
