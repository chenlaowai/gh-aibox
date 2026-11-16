/** 默认实例的Aixos配置 */
export const DEFAULT_OPTIONS = {
  // 请求超时时间,默认60秒
  timeout: 60 * 1000,
}

/** 默认实例的后端字段配置 */
export const DEFAULT_BACKEND_OPTIONS = {
  codeKey: 'code',
  dataKey: 'data',
  msgKey: 'message',
  successCode: 200,
}

/** 请求不成功各种状态的错误 */
export const ERROR_STATUS = {
  default: '请求错误',
  400: '请求出现语法错误',
  401: '登录已失效，请重新登录',
  403: '服务器拒绝访问',
  404: '请求的资源不存在',
  405: '请求方法未允许',
  408: '网络请求超时',
  500: '服务器内部错误',
  501: '服务器未实现请求功能',
  502: '错误网关',
  503: '服务不可用',
  504: '网关超时',
  505: 'http版本不支持该请求',
}

/** 没有错误提示的code */
export const ERROR_NO_TIP_STATUS = [10000]

/** 是否开启HTTP状态码非200错误提示 */
export const ENABLE_HTTP_ERROR_TIP = true

/** 是否开启业务状态码非200错误提示 */
export const ENABLE_BUSINESS_ERROR_TIP = true

/** 是否开启双Token刷新机制 */
export const ENABLE_REFRESH_TOKEN = true

/** token刷新接口地址 */
export const REFRESH_TOKEN_API = '/api/Auth/RefreshToken'