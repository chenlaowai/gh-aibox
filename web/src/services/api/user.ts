// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 新增用户 POST /api/User/Add */
export async function postUserAdd(
  body: API.AddUserRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/User/Add", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 删除用户 DELETE /api/User/Delete/${param0} */
export async function deleteUserDeleteId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.deleteUserDeleteIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/User/Delete/${param0}`, {
    method: "DELETE",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 编辑用户 POST /api/User/Modify */
export async function postUserModify(
  body: API.ModifyUserRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/User/Modify", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 重置密码 POST /api/User/ResetPassword */
export async function postUserResetPassword(
  body: API.ResetPasswordRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/User/ResetPassword", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 获取用户信息 GET /api/User/UserInfo */
export async function getUserUserInfo(options?: AxiosRequestConfig) {
  return request<API.UserInfo>("/api/User/UserInfo", {
    method: "GET",
    ...(options || {}),
  });
}

/** 用户分页列表 GET /api/User/UserPage */
export async function getUserUserPage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getUserUserPageParams,
  options?: AxiosRequestConfig
) {
  return request<API.UserBaseInfoPage>("/api/User/UserPage", {
    method: "GET",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}
