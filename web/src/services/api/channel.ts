// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 创建通道 POST /api/Channel/AddChannel */
export async function postChannelAddChannel(
  body: API.AddChannelRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Channel/AddChannel", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 删除通道 DELETE /api/Channel/DeleteChannel/${param0} */
export async function deleteChannelDeleteChannelId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.deleteChannelDeleteChannelIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/Channel/DeleteChannel/${param0}`, {
    method: "DELETE",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取通道详情 GET /api/Channel/GetChannelDetail/${param0} */
export async function getChannelGetChannelDetailId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getChannelGetChannelDetailIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<API.GetChannelDetailResult>(
    `/api/Channel/GetChannelDetail/${param0}`,
    {
      method: "GET",
      params: { ...queryParams },
      ...(options || {}),
    }
  );
}

/** 获取所有通道名称和ID（用于筛选框） GET /api/Channel/GetChannelDropdown */
export async function getChannelGetChannelDropdown(
  options?: AxiosRequestConfig
) {
  return request<API.GetChannelDropdownResult[]>(
    "/api/Channel/GetChannelDropdown",
    {
      method: "GET",
      ...(options || {}),
    }
  );
}

/** 获取通道列表 GET /api/Channel/GetChannelsPage */
export async function getChannelGetChannelsPage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getChannelGetChannelsPageParams,
  options?: AxiosRequestConfig
) {
  return request<API.GetChannelPageResultPage>("/api/Channel/GetChannelsPage", {
    method: "GET",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}

/** 更新通道 PUT /api/Channel/UpdateChannel */
export async function putChannelUpdateChannel(
  body: API.UpdateChannelRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Channel/UpdateChannel", {
    method: "PUT",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}
