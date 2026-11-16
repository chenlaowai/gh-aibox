// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 删除告警 DELETE /api/Alarm/DeletedAlarm */
export async function deleteAlarmDeletedAlarm(
  body: API.DeleteAlarmRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Alarm/DeletedAlarm", {
    method: "DELETE",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 告警详情查询 GET /api/Alarm/GetAlarmDetail/${param0} */
export async function getAlarmGetAlarmDetailId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getAlarmGetAlarmDetailIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<API.GetAlarmDetailResult>(
    `/api/Alarm/GetAlarmDetail/${param0}`,
    {
      method: "GET",
      params: { ...queryParams },
      ...(options || {}),
    }
  );
}

/** 告警分页查询 POST /api/Alarm/QueryAlarmPage */
export async function postAlarmQueryAlarmPage(
  body: API.QueryAlarmPageRequest,
  options?: AxiosRequestConfig
) {
  return request<API.QueryAlarmPageResultPage>("/api/Alarm/QueryAlarmPage", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}
