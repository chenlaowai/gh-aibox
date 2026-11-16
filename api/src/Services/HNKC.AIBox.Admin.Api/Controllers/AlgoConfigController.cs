using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.SqlSugars.Extensions;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 算法模型阈值管理
/// </summary>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class AlgoConfigController(ISqlSugarClient sqlSugarClient) : ControllerBase
{
    /// <summary>
    /// 获取算法模型阈值分页列表
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(Page<GetAlgoConfigPageResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetAlgoConfigPageAsync([FromQuery] PageRequest request)
    {
        var data = await sqlSugarClient.Queryable<Tb_Algo_Config>()
            .Where(r => !r.IsDelete)
            .LeftJoin<Tb_Algo_Mode>((r, algoMode) => r.AlgoId == algoMode.Id)
            .Select((r, algoMode) => new GetAlgoConfigPageResult
            {
                Id = r.Id,
                Name = r.Name,
                ConfigValue = r.ConfigValue,
                AlgoId = r.AlgoId,
                AlgoModeName = algoMode.Name,
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);
        return Ok(data);
    }


    /// <summary>
    /// 更新算法模型阈值
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpPut]
    public async Task<IActionResult> UpdateAlgoConfigAsync([FromBody] UpdateAlgoConfigRequest request)
    {
        var data = await sqlSugarClient.FirstAsync<Tb_Algo_Config>(r => !r.IsDelete && r.Id == request.Id);
        if (null == data)
        {
            throw new ResultMessageException("算法模型阈值不存在");
        }

        data.ConfigValue = request.ConfigValue;
        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
        return Ok();
    }

    /// <summary>
    /// 创建算法模型阈值
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    // [HttpPost]
    // public async Task<IActionResult> AddAlgoConfigAsync([FromBody] AddAlgoConfigRequest request)
    // {
    //     var add = new Tb_Algo_Config
    //     {
    //         AlgoId = request.AlgoId,
    //         Name = request.Name,
    //         ConfigValue = request.ConfigValue,
    //     };
    //     await sqlSugarClient.Insertable(add).ExecuteCommandAsync();
    //     return Ok();
    // }

    /// <summary>
    /// 删除算法模型阈值
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    // [HttpDelete("{id}")]
    // public async Task<IActionResult> DeleteAlgoConfigAsync([FromQuery] string id)
    // {
    //     var data = await sqlSugarClient.FirstAsync<Tb_Algo_Config>(r => !r.IsDelete && r.Id == id);
    //     if (null == data)
    //     {
    //         throw new ResultMessageException("算法模型阈值不存在");
    //     }
    //
    //     data.IsDelete = true;
    //     await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
    //     return Ok();
    // }
}