// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 登录 POST /api/Auth/Login */
export async function postAuthLogin(
  body: API.LoginRequest,
  options?: AxiosRequestConfig
) {
  return request<API.TokenInfoResult>("/api/Auth/Login", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 退出登录 POST /api/Auth/Logout */
export async function postAuthLogout(
  body: API.RefreshTokenRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Auth/Logout", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 刷新Token POST /api/Auth/RefreshToken */
export async function postAuthRefreshToken(
  body: API.RefreshTokenRequest,
  options?: AxiosRequestConfig
) {
  return request<API.TokenInfoResult>("/api/Auth/RefreshToken", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}
