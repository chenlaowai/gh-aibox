// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 获取算法模型阈值分页列表 GET /api/AlgoConfig/GetAlgoConfigPage */
export async function getAlgoConfigGetAlgoConfigPage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getAlgoConfigGetAlgoConfigPageParams,
  options?: AxiosRequestConfig
) {
  return request<API.GetAlgoConfigPageResultPage>(
    "/api/AlgoConfig/GetAlgoConfigPage",
    {
      method: "GET",
      params: {
        ...params,
      },
      ...(options || {}),
    }
  );
}

/** 更新算法模型阈值 PUT /api/AlgoConfig/UpdateAlgoConfig */
export async function putAlgoConfigUpdateAlgoConfig(
  body: API.UpdateAlgoConfigRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/AlgoConfig/UpdateAlgoConfig", {
    method: "PUT",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}
