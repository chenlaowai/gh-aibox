import type { AxiosError, AxiosRequestConfig, AxiosResponse, InternalAxiosRequestConfig } from 'axios'

import qs from 'qs'
import axios from 'axios'
import type { Result } from './types'
import { DEFAULT_BACKEND_OPTIONS, DEFAULT_OPTIONS, ENABLE_REFRESH_TOKEN, REFRESH_TOKEN_API } from './config'
import { handleBusinessError, handleResponseError, handleServiceResult } from './handle'

const axiosInstance = axios.create({
  baseURL: import.meta.env.VITE_HTTP_PROXY === 'Y' && import.meta.env.DEV ? import.meta.env.VITE_HTTP_PROXY_URL : import.meta.env.VITE_BASE_API,
  timeout: DEFAULT_OPTIONS.timeout,
})

// 是否正在刷新token
let isRefreshing = false
// 定义一个队列，用于存储失败的请求
let failedQueue: any[] = []

// 请求拦截
axiosInstance.interceptors.request.use(
  async (config: InternalAxiosRequestConfig) => {
    const authStore = store.useAuthStore()

    config.headers.Authorization = authStore.accessToken

    return config
  },
  (error: any) => {
    return Promise.reject(error)
  },
)
// 响应拦截
axiosInstance.interceptors.response.use(
  (response: AxiosResponse<Result>) => {
    // 单独处理下载文件流情况
    if (response.config.responseType === 'blob') {
      return response
    }
    // 业务请求成功
    if (response.data.isSuccess)
      return { ...response, data: handleServiceResult(response.data) }

    // 业务请求失败
    const errorResult = handleBusinessError(response, DEFAULT_BACKEND_OPTIONS)
    // eslint-disable-next-line prefer-promise-reject-errors
    return Promise.reject({ ...response, data: handleServiceResult(errorResult, false) })
  },
  async (error: AxiosError<Result>) => {
    // http状态码非200
    const errorResult = handleResponseError(error)
    const originalRequest = error.config as AxiosError['config'] & { _retry: boolean }

    // originalRequest._retry是一个自定义属性，用于标记请求是否已经重试过
    if (error.response?.status === 401 && !originalRequest._retry && ENABLE_REFRESH_TOKEN) {
      originalRequest._retry = true

      // 刷新token
      if (!isRefreshing) {
        isRefreshing = true
        const authStore = store.useAuthStore()

        try {
          await authStore.getRefreshToken()
          axiosInstance.defaults.headers.common.Authorization = authStore.accessToken
          // 使用新的token重试所有等待的请求
          failedQueue.forEach(promise => promise.resolve(authStore.accessToken))
          // 重试原始请求
          return axiosInstance(originalRequest)
        }
        catch (refreshError) {
          // 刷新Token失败，跳转登录页
          failedQueue.forEach(item => item.reject(refreshError))
          authStore.logout()
          return Promise.reject(refreshError)
        }
        finally {
          // 重置刷新状态并清空队列
          isRefreshing = false
          failedQueue = []
        }
      }

      if (originalRequest.url === REFRESH_TOKEN_API) {
        return Promise.reject(handleServiceResult(errorResult, false))
      }

      // 如果正在刷新token，则将请求加入队列
      return new Promise((resolve, reject) => failedQueue.push({ resolve, reject }))
        .then((newToken) => {
          originalRequest.headers.Authorization = newToken as string
          return axiosInstance(originalRequest)
        })
        .catch((err) => {
          return Promise.reject(err)
        })
    }

    return Promise.reject(handleServiceResult(errorResult, false))
  },
)

export async function request<T = any, R = Result<T>>(url: string, config: AxiosRequestConfig): Promise<R> {
  try {
    const { data, headers } = await axiosInstance.request<Result<T>>({
      url,
      paramsSerializer: (params) => {
        if (config.method && config.method.toUpperCase() === 'GET') {
          return qs.stringify(params, { indices: false })
        }
        return ''
      },
      ...config,
    })

    // 判断是否需要返回 HeaderResult
    if (config.responseType === 'blob') {
      return {
        data,
        headers,
      } as R // 强制转换为 HeaderResult
    }

    return data as R // 返回 Result
  }
  catch (err) {
    return Promise.reject(err)
  }
}

export default request
