// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 上传算法模型 POST /api/AlgoMode/AddAlgoMode */
export async function postAlgoModeAddAlgoMode(
  body: {
    /** 算法名称 */
    Name: string;
    /** 备注 */
    Remark?: string;
  },
  Model?: File,
  options?: AxiosRequestConfig
) {
  const formData = new FormData();

  if (Model) {
    formData.append("Model", Model);
  }

  Object.keys(body).forEach((ele) => {
    const item = (body as any)[ele];

    if (item !== undefined && item !== null) {
      if (typeof item === "object" && !(item instanceof File)) {
        if (item instanceof Array) {
          item.forEach((f) => formData.append(ele, f || ""));
        } else {
          formData.append(ele, JSON.stringify(item));
        }
      } else {
        formData.append(ele, item);
      }
    }
  });

  return request<any>("/api/AlgoMode/AddAlgoMode", {
    method: "POST",
    data: formData,
    requestType: "form",
    ...(options || {}),
  });
}

/** 删除算法模型 DELETE /api/AlgoMode/DeleteAlgoMode/${param0} */
export async function deleteAlgoModeDeleteAlgoModeId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.deleteAlgoModeDeleteAlgoModeIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/AlgoMode/DeleteAlgoMode/${param0}`, {
    method: "DELETE",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取所有算法模型的名称和ID（用于筛选框） GET /api/AlgoMode/GetAlgoModeDropdown */
export async function getAlgoModeGetAlgoModeDropdown(
  options?: AxiosRequestConfig
) {
  return request<API.GetAlgoModeDropdownResult[]>(
    "/api/AlgoMode/GetAlgoModeDropdown",
    {
      method: "GET",
      ...(options || {}),
    }
  );
}

/** 获取算法模型配置 GET /api/AlgoMode/GetAlgoModelConfig */
export async function getAlgoModeGetAlgoModelConfig(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getAlgoModeGetAlgoModelConfigParams,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/AlgoMode/GetAlgoModelConfig", {
    method: "GET",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}

/** 获取算法模型分页列表 GET /api/AlgoMode/GetAlgoModePage */
export async function getAlgoModeGetAlgoModePage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getAlgoModeGetAlgoModePageParams,
  options?: AxiosRequestConfig
) {
  return request<API.GetAlgoModePageResultPage>(
    "/api/AlgoMode/GetAlgoModePage",
    {
      method: "GET",
      params: {
        ...params,
      },
      ...(options || {}),
    }
  );
}
