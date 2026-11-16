// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 获取系统字典枚举 GET /api/System/SystemDict */
export async function getSystemSystemDict(options?: AxiosRequestConfig) {
  return request<Record<string, any>>("/api/System/SystemDict", {
    method: "GET",
    ...(options || {}),
  });
}
