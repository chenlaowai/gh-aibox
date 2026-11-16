using HNKC.AIBox.Enums;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Services;
using HNKC.AIBox.SqlSugars.UnitOfTransaction;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 任务管理
/// </summary>
/// <param name="sqlSugarClient"></param>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class TaskController(ITaskService taskService) : ControllerBase
{
    /// <summary>
    /// 获取任务分页列表
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(GetTaskPageResult), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetTaskPageAsync([FromQuery] PageRequest request)
    {
        var result = await taskService.GetTaskPageAsync(request);
        return Ok(result);
    }

    /// <summary>
    /// 获取任务详情
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    [HttpGet("{id}")]
    [AllowAnonymous]
    [ProducesResponseType(typeof(GetTaskDetailResult), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetTaskDetailAsync([FromRoute] string id)
    {
        var data = await taskService.GetTaskDetailAsync(id);
        return Ok(data);
    }

    /// <summary>
    /// 创建任务
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpPost]
    [Transactional]
    public async Task<IActionResult> AddTaskAsync([FromBody] AddTaskRequest request)
    {
        await taskService.AddTaskAsync(request);
        return Ok();
    }

    /// <summary>
    /// 更新任务
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpPut]
    [Transactional]
    public async Task<IActionResult> UpdateTaskAsync([FromBody] UpdateTaskRequest request)
    {
        await taskService.UpdateTaskAsync(request);
        return Ok();
    }

    /// <summary>
    /// 删除任务
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpDelete("{id}")]
    [Transactional]
    public async Task<IActionResult> DeleteTaskAsync([FromRoute] string id)
    {
        await taskService.DeleteTaskAsync(id);
        return Ok();
    }

    /// <summary>
    /// 更新任务状态
    /// </summary>
    /// <param name="id">任务ID</param>
    /// <param name="status">新状态</param>
    /// <returns></returns>
    [HttpPatch("{id}/{status}")]
    public async Task<IActionResult> UpdateTaskStatusAsync([FromRoute] string id, TaskStatusEnum status)
    {
        await taskService.UpdateTaskStatusAsync(id, status);
        return Ok();
    }

    /// <summary>
    /// 停用任务
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    [HttpPatch("{id}/Stop")]
    public async Task<IActionResult> StopTaskAsync([FromRoute] string id)
    {
        await taskService.StopTaskAsync(id);
        return Ok();
    }

    /// <summary>
    /// 获取所有任务的编号和ID（用于筛选框）
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(List<GetTaskDropdownResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetTaskDropdownAsync()
    {
        var result = await taskService.GetTaskDropdownAsync();
        return Ok(result);
    }

    /// <summary>
    /// 获取任务区域
    /// </summary>
    /// <param name="taskId"></param>
    /// <returns></returns>
    [ProducesResponseType(typeof(GetTaskAreaResult), StatusCodes.Status200OK)]
    [HttpGet("{taskId}")]
    public async Task<IActionResult> GetTaskAreasAsync([FromRoute] string taskId)
    {
        var result = await taskService.GetTaskAreasAsync(taskId);
        return Ok(result);
    }

    /// <summary>
    /// 添加任务区域
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpPost]
    [Transactional]
    public async Task<IActionResult> AddTaskAreasAsync([FromBody] AddTaskAreaRequest request)
    {
        await taskService.AddTaskAreasAsync(request);
        return Ok();
    }
}