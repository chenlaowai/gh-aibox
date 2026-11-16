// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 创建告警类别 POST /api/AlarmType/AddAlarmType */
export async function postAlarmTypeAddAlarmType(
  body: API.AddAlarmTypeRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/AlarmType/AddAlarmType", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 删除告警类别 DELETE /api/AlarmType/DeleteAlarmType/${param0} */
export async function deleteAlarmTypeDeleteAlarmTypeId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.deleteAlarmTypeDeleteAlarmTypeIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/AlarmType/DeleteAlarmType/${param0}`, {
    method: "DELETE",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取所有告警类别（用于筛选框） GET /api/AlarmType/GetAlarmTypeDropdown */
export async function getAlarmTypeGetAlarmTypeDropdown(
  options?: AxiosRequestConfig
) {
  return request<API.GetAlarmTypeDropdownResult[]>(
    "/api/AlarmType/GetAlarmTypeDropdown",
    {
      method: "GET",
      ...(options || {}),
    }
  );
}

/** 获取告警类别分页列表 GET /api/AlarmType/GetAlarmTypePage */
export async function getAlarmTypeGetAlarmTypePage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getAlarmTypeGetAlarmTypePageParams,
  options?: AxiosRequestConfig
) {
  return request<API.GetAlarmTypePageResultPage>(
    "/api/AlarmType/GetAlarmTypePage",
    {
      method: "GET",
      params: {
        ...params,
      },
      ...(options || {}),
    }
  );
}
