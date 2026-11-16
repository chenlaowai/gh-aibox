import type { AxiosError, AxiosResponse, InternalAxiosRequestConfig } from 'axios'
import { createDiscreteApi } from 'naive-ui'

import {
  ENABLE_BUSINESS_ERROR_TIP,
  ENABLE_HTTP_ERROR_TIP,
  ERROR_NO_TIP_STATUS,
  ERROR_STATUS,
} from './config'
import type { Result } from './types'

type ErrorStatus = keyof typeof ERROR_STATUS

const { message } = createDiscreteApi(['message'])

let isAuthErrorHandled = false

/**
 * @description: 处理请求成功，但返回后端服务器报错
 * @param {AxiosError['response']} response
 * @return {*}
 */
export function handleResponseError(err: AxiosError<Result>) {
  const error: Service.RequestError<InternalAxiosRequestConfig> = {
    errorType: 'Response Error',
    code: 0,
    message: ERROR_STATUS.default,
    data: null,
    config: err?.config,
  }

  const errorCode: ErrorStatus = err?.response?.status as ErrorStatus
  const message = err?.response?.data?.message || ERROR_STATUS[errorCode] || ERROR_STATUS.default
  Object.assign(error, { code: errorCode, message })

  if (ENABLE_HTTP_ERROR_TIP)
    showError(error)

  return error
}

/**
 * @description:
 * @param {Record} response 接口返回的后台数据
 * @param {Service} config 后台字段配置
 * @return {*}
 */
export function handleBusinessError(response: AxiosResponse, config: Required<Service.BackendConfig>) {
  const { codeKey, msgKey } = config
  const { data } = response
  const error: Service.RequestError<InternalAxiosRequestConfig> = {
    errorType: 'Business Error',
    code: data[codeKey],
    message: data[msgKey],
    data: data.data,
    config: response.config,
  }

  if (ENABLE_BUSINESS_ERROR_TIP)
    showError(error)

  return error
}

/**
 * @description: 统一成功和失败返回类型
 * @param {any} data
 * @param {boolean} isSuccess
 * @return {*} result
 */
export function handleServiceResult(data: any, isSuccess: boolean = true) {
  const result = {
    isSuccess,
    errorType: null,
    ...data,
  }

  return result
}

export function showError(error: Service.RequestError<InternalAxiosRequestConfig>) {
  const code = Number(error.code)

  // 如果error不需要提示,则跳过
  if (ERROR_NO_TIP_STATUS.includes(code) || error.config?.hideErrMsg)
    return

  if (!isAuthErrorHandled) {
    isAuthErrorHandled = true // 设置标志为 true，表示已处理

    message.error(error.message || ERROR_STATUS.default)

    setTimeout(() => {
      isAuthErrorHandled = false
    }, 1000)
  }
}
