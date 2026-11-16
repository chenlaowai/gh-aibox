using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.SqlSugars.Extensions;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 告警类别管理
/// </summary>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class AlarmTypeController(ISqlSugarClient sqlSugarClient) : ControllerBase
{
    /// <summary>
    /// 获取告警类别分页列表
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(Page<GetAlarmTypePageResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetAlarmTypePageAsync([FromQuery] PageRequest request)
    {
        var data = await sqlSugarClient.Queryable<Tb_Alarm_Type>()
            .Where(r => !r.IsDelete)
            .LeftJoin<Tb_Algo_Mode>((r, algoMode) => r.AlgoId == algoMode.Id)
            .Select((r, algoMode) => new GetAlarmTypePageResult
            {
                Id = r.Id,
                Code = r.Code,
                AlgoModeName = algoMode.Name,
                Remark = r.Remark
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);
        return Ok(data);
    }

    /// <summary>
    /// 创建告警类别
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpPost]
    public async Task<IActionResult> AddAlarmTypeAsync([FromBody] AddAlarmTypeRequest request)
    {
        var algoExists = await sqlSugarClient.Queryable<Tb_Algo_Mode>()
            .Where(r => r.Id == request.AlgoId && !r.IsDelete)
            .AnyAsync();

        if (!algoExists)
        {
            return BadRequest("模型不存在或已被删除，请刷新后重试！");
        }

        var add = new Tb_Alarm_Type
        {
            AlgoId = request.AlgoId,
            Code = request.Code,
            Remark = request.Remark,
        };
        await sqlSugarClient.Insertable(add).ExecuteCommandAsync();
        return Ok();
    }

    /// <summary>
    /// 删除告警类别
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpDelete("{id}")]
    public async Task<IActionResult> DeleteAlarmTypeAsync([FromRoute] string id)
    {
        var data = await sqlSugarClient.FirstAsync<Tb_Alarm_Type>(r => !r.IsDelete && r.Id == id);
        if (null == data)
        {
            throw new ResultMessageException("告警类别不存在");
        }

        data.IsDelete = true;
        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
        return Ok();
    }
    
    
    /// <summary>
    /// 获取所有告警类别（用于筛选框）
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(List<GetAlarmTypeDropdownResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetAlarmTypeDropdownAsync()
    {
        var result = await sqlSugarClient.Queryable<Tb_Alarm_Type>()
            .Where(r => !r.IsDelete)
            .Select(r => new GetAlarmTypeDropdownResult
            {
                Id = r.Id,
                Code = r.Code,
                Remark = r.Remark
            })
            .ToListAsync();
        return Ok(result);
    }
}