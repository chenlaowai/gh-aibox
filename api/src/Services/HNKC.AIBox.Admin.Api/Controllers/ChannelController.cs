using HNKC.AIBox.Entities;
using HNKC.AIBox.Enums;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.SqlSugars.Extensions;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 通道管理
/// </summary>
/// <param name="sqlSugarClient"></param>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class ChannelController(ISqlSugarClient sqlSugarClient) : ControllerBase
{
    /// <summary>
    /// 获取通道列表
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(Page<GetChannelPageResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetChannelsPageAsync([FromQuery] GetChannelPageRequest request)
    {
        // TODO 通道状态判断依据？
        var data = await sqlSugarClient.Queryable<Tb_Channel>()
            .Where(r => !r.IsDelete)
            .Select(r => new GetChannelPageResult
            {
                Id = r.Id,
                Code = r.Code,
                Name= r.Name,
                ChannelStatus = ChannelStatusEnum.Online,
                Address = r.Address
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);
        return Ok(data);
    }

    /// <summary>
    /// 获取通道详情
    /// </summary>
    /// <returns></returns>
    [HttpGet("{id}")]
    [ProducesResponseType(typeof(GetChannelDetailResult), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetChannelDetailAsync([FromRoute] string id)
    {
        var data = await sqlSugarClient.Queryable<Tb_Channel>()
            .Where(r => !r.IsDelete)
            .Where(r => r.Id == id)
            .Select(r => new GetChannelDetailResult
            {
                Id = r.Id,
                ProtocolType = r.ProtocolType,
                Address = r.Address,
                Code = r.Code,
                Name =r.Name,
                Remark = r.Remark,
                ServiceAddress = r.ServiceAddress,
                AccountId = r.AccountId,
                AccountKey = r.AccountKey,
            })
            .FirstAsync();
        if (null == data)
        {
            throw new ResultMessageException("通道不存在");
        }

        return Ok(data);
    }


    /// <summary>
    /// 创建通道
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpPost]
    public async Task<IActionResult> AddChannelAsync([FromBody] AddChannelRequest request)
    {
        var add = new Tb_Channel
        {
            ProtocolType = request.ProtocolType,
            Address = request.Address,
            Code = request.Code,
            Name = request.Name,
            Remark = request.Remark,
            ServiceAddress = request.ServiceAddress,
            AccountId = request.AccountId,
            AccountKey = request.AccountKey,
        };

        var exists = await sqlSugarClient.Queryable<Tb_Channel>()
            .AnyAsync(r => r.Code == request.Code && !r.IsDelete);
        if (exists)
        {
            throw new ResultMessageException("监控编码已存在");
        }
        if (await sqlSugarClient.Queryable<Tb_Channel>()
            .AnyAsync(r => r.Name == request.Name && !r.IsDelete))
        {
            throw new ResultMessageException("通道名称已存在");
        }
        await sqlSugarClient.Insertable(add).ExecuteCommandAsync();
        return Ok();
    }

    /// <summary>
    /// 更新通道
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpPut]
    public async Task<IActionResult> UpdateChannelAsync([FromBody] UpdateChannelRequest request)
    {
        var data = await sqlSugarClient.FirstAsync<Tb_Channel>(r => !r.IsDelete && r.Id == request.Id);
        if (null == data)
        {
            throw new ResultMessageException("通道不存在");
        }

        //编码重复校验（排除自身）
        if (data.Code != request.Code)
        {
            var exists = await sqlSugarClient.Queryable<Tb_Channel>()
                .AnyAsync(r => r.Code == request.Code &&
                               !r.IsDelete &&
                               r.Id != request.Id);
            if (exists)
            {
                throw new ResultMessageException("监控编码已存在");
            }
        }
        //名称重复校验（排除自身）
        if (data.Name != request.Name)
        {
            var exists = await sqlSugarClient.Queryable<Tb_Channel>()
                .AnyAsync(r => r.Name == request.Name &&
                               !r.IsDelete &&
                               r.Id != request.Id);
            if (exists)
            {
                throw new ResultMessageException("通道名称已存在");
            }
        }
        data.ProtocolType = request.ProtocolType;
        data.Address = request.Address;
        data.Name = request.Name;
        data.Code = request.Code;
        data.Remark = request.Remark;
        data.ServiceAddress = request.ServiceAddress;
        data.AccountId = request.AccountId;
        data.AccountKey = request.AccountKey;

        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();

        return Ok();
    }

    /// <summary>
    /// 删除通道
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpDelete("{id}")]
    public async Task<IActionResult> DeleteChannelAsync([FromRoute] string id)
    {
        if (string.IsNullOrWhiteSpace(id))
        {
            throw new ResultMessageException("删除操作时通道Id不能为空");
        }

        var data = await sqlSugarClient.FirstAsync<Tb_Channel>(r => !r.IsDelete && r.Id == id);
        if (null == data)
        {
            throw new ResultMessageException("通道不存在");
        }

        data.IsDelete = true;
        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
        return Ok();
    }

    /// <summary>
    /// 获取所有通道名称和ID（用于筛选框）
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(List<GetChannelDropdownResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetChannelDropdownAsync()
    {
        var data = await sqlSugarClient.Queryable<Tb_Channel>()
            .Where(r => !r.IsDelete)
            .Select(r => new GetChannelDropdownResult
            {
                Id = r.Id,
                Name = r.Name
            })
            .ToListAsync();
        return Ok(data);
    }
}