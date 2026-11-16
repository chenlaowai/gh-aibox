// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 获取算法告警统计 GET /api/Device/GetAlogAlarmStats */
export async function getDeviceGetAlogAlarmStats(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/GetAlogAlarmStats", {
    method: "GET",
    ...(options || {}),
  });
}

/** 获取首页预警信息 GET /api/Device/GetDeviceAlarms */
export async function getDeviceGetDeviceAlarms(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/GetDeviceAlarms", {
    method: "GET",
    ...(options || {}),
  });
}

/** 获取资源占用率 GET /api/Device/GetOccupancy */
export async function getDeviceGetOccupancy(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/GetOccupancy", {
    method: "GET",
    ...(options || {}),
  });
}

/** 获取首页任务日志 GET /api/Device/GetTaskLogs */
export async function getDeviceGetTaskLogs(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/GetTaskLogs", {
    method: "GET",
    ...(options || {}),
  });
}

/** 获取任务告警排行 GET /api/Device/GetTopTaskAlarm */
export async function getDeviceGetTopTaskAlarm(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/GetTopTaskAlarm", {
    method: "GET",
    ...(options || {}),
  });
}

/** 获取首页基本信息 GET /api/Device/HomeBasicInfo */
export async function getDeviceHomeBasicInfo(options?: AxiosRequestConfig) {
  return request<API.DeviceInfoResult>("/api/Device/HomeBasicInfo", {
    method: "GET",
    ...(options || {}),
  });
}

/** 重启设备 POST /api/Device/Reboot */
export async function postDeviceReboot(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/Reboot", {
    method: "POST",
    ...(options || {}),
  });
}

/** 重启服务 POST /api/Device/RestartService */
export async function postDeviceRestartService(options?: AxiosRequestConfig) {
  return request<any>("/api/Device/RestartService", {
    method: "POST",
    ...(options || {}),
  });
}
