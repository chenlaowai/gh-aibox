using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.SqlSugars.Extensions;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 参数配置
/// </summary>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class ParamConfigController(ISqlSugarClient sqlSugarClient) : ControllerBase
{
    /// <summary>
    /// 获取参数配置分页列表
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(Page<GetParamConfigPageResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetParamConfigPageAsync([FromQuery] PageRequest request)
    {
        var data = await sqlSugarClient.Queryable<Tb_Param_Config>()
            .Where(r => !r.IsDelete && r.IsShow)
            .Select(r => new GetParamConfigPageResult
            {
                Id = r.Id,
                Name = r.Name,
                Code = r.Code,
                ParamType = r.ParamType,
                ParamValue = r.ParamValue,
                Remark = r.Remark
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);

        return Ok(data);
    }

    /// <summary>
    /// 添加参数配置
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpPost]
    [Obsolete]
    public async Task<IActionResult> AddParamConfigAsync([FromBody] AddParamConfigRequest request)
    {
        var data = await sqlSugarClient.Insertable(new Tb_Param_Config
        {
            Name = request.Name,
            Code =request.Code,
            ParamType = "用户参数",
            ParamValue = request.ParamValue,
            Remark = request.Remark,
            IsShow = true
        }).ExecuteCommandAsync();
        return Ok(data);
    }

    /// <summary>
    /// 更新参数配置
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpPost]
    public async Task<IActionResult> UpdateParamConfigAsync([FromBody] UpdateParamConfigRequest request)
    {
        var data = await sqlSugarClient.FirstAsync<Tb_Param_Config>(r => !r.IsDelete && r.Id == request.Id);
        if (null == data)
        {
            throw new ResultMessageException("参数配置不存在");
        }
        data.ParamValue = request.ParamValue;
        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();

        return Ok();
    }

    /// <summary>
    /// 删除参数配置
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpDelete("{id}")]
    [Obsolete]
    public async Task<IActionResult> DeleteParamConfigAsync([FromRoute] string id)
    {
        var data = await sqlSugarClient.FirstAsync<Tb_Param_Config>(r => !r.IsDelete && r.Id == id);
        if (null == data)
        {
            throw new ResultMessageException("参数配置不存在");
        }

        data.IsDelete = true;
        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
        return Ok();
    }
}