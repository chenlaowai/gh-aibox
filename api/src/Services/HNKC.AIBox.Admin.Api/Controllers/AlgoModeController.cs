using HNKC.AIBox.Entities;
using HNKC.AIBox.Enums;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Services;
using HNKC.AIBox.SqlSugars.Extensions;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 算法模型管理
/// </summary>
/// <param name="sqlSugarClient"></param>
[Route("api/[controller]/[action]")]
[ApiController]
[Authorize]
public class AlgoModeController(
    ISqlSugarClient sqlSugarClient,
    IFileUploadService fileUploadService,
    IConfiguration configuration) : ControllerBase
{
    /// <summary>
    /// 获取算法模型分页列表
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(Page<GetAlgoModePageResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetAlgoModePageAsync([FromQuery] PageRequest request)
    {
        var data = await sqlSugarClient.Queryable<Tb_Algo_Mode>()
            .Where(r => !r.IsDelete)
            .Select(r => new GetAlgoModePageResult
            {
                Id = r.Id,
                Name = r.Name,
                FilePath = r.FilePath,
                VersionDate = r.VersionDate,
                Remark = r.Remark
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);

        return Ok(data);
    }

    /// <summary>
    /// 上传算法模型
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpPost]
    [RequestFormLimits(MultipartBodyLengthLimit = 262144000)] // 表单数据的最大长度为250MB
    [RequestSizeLimit(262144000)] // 整个HTTP请求的最大大小为250MB
    public async Task<IActionResult> AddAlgoModeAsync([FromForm] AddAlgoModeRequest request)
    {
        string uploadedFilePath = null;
        try
        {
            var uploadSettings = configuration.GetSection("ModelUploadSettings");
            var allowedExtensions = uploadSettings["AllowedExtensions"]?.Split(',');
            var targetDirectory = uploadSettings["UploadDirectory"];
            var maxFileSize = int.Parse(uploadSettings["MaxFileSize"] ?? "0");

            if (allowedExtensions == null || allowedExtensions.Length == 0 ||
                string.IsNullOrWhiteSpace(targetDirectory))
            {
                throw new ArgumentException("模型上传配置无效");
            }

            // 开始事务
            await sqlSugarClient.Ado.BeginTranAsync();

            uploadedFilePath =
                await fileUploadService.UploadFileAsync(request.Model, allowedExtensions, targetDirectory,
                    maxFileSize > 0 ? maxFileSize : null);

            var data = await sqlSugarClient.FirstAsync<Tb_Algo_Mode>(r => !r.IsDelete && r.Name == request.Name);
            if (null != data)
            {
                data.FilePath = uploadedFilePath;
                data.VersionDate = DateTime.UtcNow;
                data.Remark = request.Remark;
                await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
            }
            else
            {
                var add = new Tb_Algo_Mode()
                {
                    Name = request.Name,
                    FilePath = uploadedFilePath,
                    VersionDate = DateTime.Now,
                    Remark = request.Remark
                };
                await sqlSugarClient.Insertable(add).ExecuteCommandAsync();
            }

            await sqlSugarClient.Ado.CommitTranAsync();
        }
        catch (Exception ex)
        {
            if (!string.IsNullOrEmpty(uploadedFilePath))
            {
                await fileUploadService.DeleteFileAsync(uploadedFilePath);
            }

            await sqlSugarClient.Ado.RollbackTranAsync();
            throw;
        }

        return Ok();
    }

    /// <summary>
    /// 删除算法模型
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    [HttpDelete("{id}")]
    public async Task<IActionResult> DeleteAlgoModeAsync([FromRoute] string id)
    {
        var data = await sqlSugarClient.FirstAsync<Tb_Algo_Mode>(r => !r.IsDelete && r.Id == id);
        if (null == data)
        {
            throw new ResultMessageException("算法模型不存在");
        }

        data.IsDelete = true;
        await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
        return Ok();
    }

    /// <summary>
    /// 获取所有算法模型的名称和ID（用于筛选框）
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    [ProducesResponseType(typeof(List<GetAlgoModeDropdownResult>), StatusCodes.Status200OK)]
    public async Task<IActionResult> GetAlgoModeDropdownAsync()
    {
        //TODO 模型授权状态判断依据？
        var data = await sqlSugarClient.Queryable<Tb_Algo_Mode>()
            .Where(r => !r.IsDelete)
            .Select(r => new GetAlgoModeDropdownResult
            {
                Id = r.Id,
                AuthorizationStatus = ModeAuthorizationEnum.Authorized,
                Name = r.Name
            })
            .ToListAsync();

        return Ok(data);
    }


    /// <summary>
    /// 获取算法模型配置
    /// </summary>
    /// <param name="algoId"></param>
    /// <returns></returns>
    [HttpGet]
    public async Task<IActionResult> GetAlgoModelConfig([FromQuery]string algoId)
    {
        var data = await sqlSugarClient.Queryable<Tb_Algo_Config>().Where(p => !p.IsDelete && p.AlgoId == algoId)
            .Select(p => new TaskAlgoConfig
            {
                ConfigId = p.Id,
                Name = p.Name,
                Remark = p.Remark,
                Code = p.Code,
                ConfigValue = p.ConfigValue,
                ValueType = p.ValueType
            }).ToListAsync();
        return Ok(data);
    }
}