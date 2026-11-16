// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 添加AI预警事件 POST /api/AI/ReportAlarmEvent */
export async function postAiReportAlarmEvent(
  body: API.AddAIAlarmRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/AI/ReportAlarmEvent", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 获取任务算法配置详情 GET /api/AI/TaskAlgoConfigDetail */
export async function getAiTaskAlgoConfigDetail(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getAITaskAlgoConfigDetailParams,
  options?: AxiosRequestConfig
) {
  return request<API.TaskAlgoConfigDetail>("/api/AI/TaskAlgoConfigDetail", {
    method: "GET",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}
