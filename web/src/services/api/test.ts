// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 操作日志测试 POST /api/Test/DeleteFilePathTest */
export async function postTestDeleteFilePathTest(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.postTestDeleteFilePathTestParams,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Test/DeleteFilePathTest", {
    method: "POST",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}
