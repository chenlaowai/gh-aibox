using HNKC.AIBox.Enums;
using HNKC.AIBox.Models;

namespace HNKC.AIBox.Services;

public interface ITaskService
{
    /// <summary>
    /// 获取任务分页列表
    /// </summary>
    Task<Page<GetTaskPageResult>> GetTaskPageAsync(PageRequest request);

    /// <summary>
    /// 获取任务详情
    /// </summary>
    Task<GetTaskDetailResult> GetTaskDetailAsync(string id);

    /// <summary>
    /// 创建任务
    /// </summary>
    Task AddTaskAsync(AddTaskRequest request);

    /// <summary>
    /// 更新任务
    /// </summary>
    Task UpdateTaskAsync(UpdateTaskRequest request);

    /// <summary>
    /// 删除任务
    /// </summary>
    Task DeleteTaskAsync(string id);

    /// <summary>
    /// 更新任务状态
    /// </summary>
    Task UpdateTaskStatusAsync(string id, TaskStatusEnum status);

    /// <summary>
    /// 停用任务
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    Task StopTaskAsync(string id);

    /// <summary>
    /// 获取任务下拉列表（编号和ID）
    /// </summary>
    Task<List<GetTaskDropdownResult>> GetTaskDropdownAsync();


    /// <summary>
    /// 获取任务区域
    /// </summary>
    /// <param name="taskId"></param>
    /// <returns></returns>
    Task<GetTaskAreaResult> GetTaskAreasAsync(string taskId);

    /// <summary>
    /// 添加任务区域
    /// </summary>
    /// <param name="id"></param>
    /// <param name="request"></param>
    /// <returns></returns>
    Task AddTaskAreasAsync(AddTaskAreaRequest request);



    /// <summary>
    /// 获取任务算法配置详情
    /// </summary>
    /// <param name="code"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    Task<TaskAlgoConfigDetail> GetTaskAlgoConfigDetailAsync(string task_id);
}