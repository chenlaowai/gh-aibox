import { generateService } from '@umijs/openapi'
import { loadEnv } from 'vite'

const env = loadEnv('dev', '')
const schemaPath = `${env.VITE_BASE_API}/swagger/v1/swagger.json`

generateService({
  // openapi地址
  schemaPath,
  // 文件生成目录
  serversPath: 'src/services',
  // 自定义网络请求函数路径
  requestImportStatement: 'import type { AxiosRequestConfig } from \'axios\';import request from \'@/services/axios/index\'',
  requestOptionsType: 'AxiosRequestConfig',
  // 代码组织命名空间, 例如：Api
  namespace: 'API',
  authorization: 'Basic YWRtaW46MVtaUS1tQnRHaFhRPHtKR3VBT0E=',
})
