// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 获取预览视频（WebSocket） GET /api/Preview/GetVideo */
export async function getPreviewGetVideo(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getPreviewGetVideoParams,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Preview/GetVideo", {
    method: "GET",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}
