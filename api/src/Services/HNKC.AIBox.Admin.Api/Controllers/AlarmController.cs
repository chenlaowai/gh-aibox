using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Models.Dtos.Alarm;
using HNKC.AIBox.SqlSugars.Extensions;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 告警管理
/// </summary>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class AlarmController(ISqlSugarClient sqlSugarClient) : ControllerBase
{
    /// <summary>
    /// 告警分页查询
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpPost]
    [ProducesResponseType(typeof(Page<QueryAlarmPageResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> QueryAlarmPageAsync([FromBody] QueryAlarmPageRequest request)
    {
        var data = await sqlSugarClient.Queryable<Tb_Alarm>()
            .Where(r => !r.IsDelete)
            .WhereIF(request.TaskId != null, r => r.TaskId == request.TaskId)
            .WhereIF(request.TypeId != null, r => r.TypeId == request.TypeId)
            .WhereIF(request.ChannelId != null, r => r.ChannelId == request.ChannelId)
            .WhereIF(request.AlarmStartTime != null, r => r.AlarmTime >= request.AlarmStartTime)
            .WhereIF(request.AlarmEndTime != null, r => r.AlarmTime <= request.AlarmEndTime)
            .LeftJoin<Tb_Task>((r, task) => r.TaskId == task.Id)
            .LeftJoin<Tb_Alarm_Type>((r, task, alarmType) => r.TypeId == alarmType.Id)
            .LeftJoin<Tb_Channel>((r, task, alarmType,channel) => task.ChannelId == channel.Id)
            .OrderBy((r, task, alarmType, channel) => r.AlarmTime, OrderByType.Desc)
            .Select((r, task, alarmType,channel) => new QueryAlarmPageResult
            {
                Id = r.Id,
                TaskCode = task.Code,
                ChannelCode = channel.Code,
                ChannelName = channel.Name,
                AlarmTypeCode = alarmType.Code,
                AlarmTypeRemark = alarmType.Remark,
                ImgPath = r.ImgPath,
                AlarmTime = r.AlarmTime,
                ReportStatus = r.ReportStatus
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);
        return Ok(data);
    }

    /// <summary>
    /// 告警详情查询
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    [HttpGet("{id}")]
    [ProducesResponseType(typeof(GetAlarmDetailResult), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetAlarmDetailAsync([FromRoute] string id)
    {
        var data = await sqlSugarClient.Queryable<Tb_Alarm>()
            .Where(r => !r.IsDelete)
            .Where(r => r.Id == id)
            .LeftJoin<Tb_Channel>((r, channel) => r.ChannelId == channel.Id)
            .LeftJoin<Tb_Task>((r, channel, task) => r.TaskId == task.Id)
            .LeftJoin<Tb_Alarm_Type>((r, channel, task, alarmType) => r.TypeId == alarmType.Id)
            .Select((r, channel, task, alarmType) => new GetAlarmDetailResult
            {
                Id = r.Id,
                ChannelAddress = channel.Address,
                TaskCode = task.Code,
                AlarmTypeCode = alarmType.Code,
                ImgPath = r.ImgPath,
                AlarmTypeRemark = alarmType.Remark,
                ReportStatus = r.ReportStatus,
                ReportAddress = r.ReportAddress,
                VideoPath = r.VideoPath,
                ReportNums = r.ReportNums
            })
            .FirstAsync();
        if (null == data)
        {
            throw new ResultMessageException("告警不存在");
        }

        return Ok(data);
    }

    /// <summary>
    /// 删除告警
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpDelete]
    public async Task<IActionResult> DeletedAlarmAsync([FromBody] DeleteAlarmRequest request)
    {
        if (null == request.Ids || !request.Ids.Any())
        {
            throw new ResultMessageException("请选择要删除的告警");
        }

        var existingCount = await sqlSugarClient.Queryable<Tb_Alarm>()
            .Where(r => request.Ids.Contains(r.Id))
            .CountAsync();

        if (existingCount != request.Ids.Count)
        {
            throw new ResultMessageException("部分告警Id不存在");
        }

        await sqlSugarClient.Updateable<Tb_Alarm>()
            .SetColumns(r => new Tb_Alarm
            {
                IsDelete = true
            })
            .Where(r => request.Ids.Contains(r.Id))
            .ExecuteCommandAsync();
        return Ok();
    }
}