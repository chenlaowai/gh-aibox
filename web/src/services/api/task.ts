// @ts-ignore
/* eslint-disable */
import type { AxiosRequestConfig } from "axios";
import request from "@/services/axios/index";

/** 创建任务 POST /api/Task/AddTask */
export async function postTaskAddTask(
  body: API.AddTaskRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Task/AddTask", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 添加任务区域 POST /api/Task/AddTaskAreas */
export async function postTaskAddTaskAreas(
  body: API.AddTaskAreaRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Task/AddTaskAreas", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 删除任务 DELETE /api/Task/DeleteTask/${param0} */
export async function deleteTaskDeleteTaskId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.deleteTaskDeleteTaskIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/Task/DeleteTask/${param0}`, {
    method: "DELETE",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取任务区域 GET /api/Task/GetTaskAreas/${param0} */
export async function getTaskGetTaskAreasTaskId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getTaskGetTaskAreasTaskIdParams,
  options?: AxiosRequestConfig
) {
  const { taskId: param0, ...queryParams } = params;
  return request<API.GetTaskAreaResult>(`/api/Task/GetTaskAreas/${param0}`, {
    method: "GET",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取任务详情 GET /api/Task/GetTaskDetail/${param0} */
export async function getTaskGetTaskDetailId(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getTaskGetTaskDetailIdParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<API.GetTaskDetailResult>(`/api/Task/GetTaskDetail/${param0}`, {
    method: "GET",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 获取所有任务的编号和ID（用于筛选框） GET /api/Task/GetTaskDropdown */
export async function getTaskGetTaskDropdown(options?: AxiosRequestConfig) {
  return request<API.GetTaskDropdownResult[]>("/api/Task/GetTaskDropdown", {
    method: "GET",
    ...(options || {}),
  });
}

/** 获取任务分页列表 GET /api/Task/GetTaskPage */
export async function getTaskGetTaskPage(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.getTaskGetTaskPageParams,
  options?: AxiosRequestConfig
) {
  return request<API.GetTaskPageResult>("/api/Task/GetTaskPage", {
    method: "GET",
    params: {
      ...params,
    },
    ...(options || {}),
  });
}

/** 停用任务 PATCH /api/Task/StopTask/${param0}/Stop */
export async function patchTaskStopTaskIdStop(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.patchTaskStopTaskIdStopParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, ...queryParams } = params;
  return request<any>(`/api/Task/StopTask/${param0}/Stop`, {
    method: "PATCH",
    params: { ...queryParams },
    ...(options || {}),
  });
}

/** 更新任务 PUT /api/Task/UpdateTask */
export async function putTaskUpdateTask(
  body: API.UpdateTaskRequest,
  options?: AxiosRequestConfig
) {
  return request<any>("/api/Task/UpdateTask", {
    method: "PUT",
    headers: {
      "Content-Type": "application/json",
    },
    data: body,
    ...(options || {}),
  });
}

/** 更新任务状态 PATCH /api/Task/UpdateTaskStatus/${param0}/${param1} */
export async function patchTaskUpdateTaskStatusIdStatus(
  // 叠加生成的Param类型 (非body参数swagger默认没有生成对象)
  params: API.patchTaskUpdateTaskStatusIdStatusParams,
  options?: AxiosRequestConfig
) {
  const { id: param0, status: param1, ...queryParams } = params;
  return request<any>(`/api/Task/UpdateTaskStatus/${param0}/${param1}`, {
    method: "PATCH",
    params: { ...queryParams },
    ...(options || {}),
  });
}
