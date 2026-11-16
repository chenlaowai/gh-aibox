// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 添加参数配置 POST /api/ParamConfig/AddParamConfig */
export async function postParamConfigAddParamConfig(
  body: API.AddParamConfigRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/ParamConfig/AddParamConfig", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 删除参数配置 DELETE /api/ParamConfig/DeleteParamConfig/${param0} */
export async function deleteParamConfigDeleteParamConfigId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.deleteParamConfigDeleteParamConfigIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/ParamConfig/DeleteParamConfig/${param0}`, {
    method: "DELETE",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取参数配置分页列表 GET /api/ParamConfig/GetParamConfigPage */
export async function getParamConfigGetParamConfigPage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getParamConfigGetParamConfigPageParams,
  options?: AxiosRequestConfig
) {
  return request<API.GetParamConfigPageResultPage>(
    "/api/ParamConfig/GetParamConfigPage",
    {
      method: "GET",
      params: {
        ...params,
      },
      ...(options || {}),
    }
  );
}

/** 更新参数配置 POST /api/ParamConfig/UpdateParamConfig */
export async function postParamConfigUpdateParamConfig(
  body: API.UpdateParamConfigRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/ParamConfig/UpdateParamConfig", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}
