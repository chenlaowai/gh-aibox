declare module 'axios' {
  interface AxiosRequestConfig {
    // umi的，不知道啥玩意，兼容一下ts
    requestType?: string
    /** 隐藏错误提示 */
    hideErrMsg?: boolean
  }
}

export interface Result<T = any> {
  code: number
  isSuccess: boolean
  message: string
  data: T
  errorType: string | null
}
