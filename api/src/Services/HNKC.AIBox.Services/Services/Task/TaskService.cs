using System.Text.Json;
using HNKC.AIBox.AITask;
using HNKC.AIBox.Entities;
using HNKC.AIBox.Enums;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.SqlSugars.Extensions;
using HNKC.AIBox.Utils;
using Microsoft.Extensions.Logging;
using SqlSugar;

namespace HNKC.AIBox.Services;

public class TaskService(ISqlSugarClient sqlSugarClient,IAITaskService aiTaskService, IIdentityService identityService, ILogger<TaskService> logger) : ITaskService
{

    /// <summary>
    /// 获取任务列表
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    public async Task<Page<GetTaskPageResult>> GetTaskPageAsync(PageRequest request)
    {
        var pagedTasks = await sqlSugarClient.Queryable<Tb_Task>()
            .Where(r => !r.IsDelete)
            .LeftJoin<Tb_Channel>((r, channel) => r.ChannelId == channel.Id)
            .Select<GetTaskPageResult>((r, channel) => new GetTaskPageResult
            {
                Id = r.Id,
                Code = r.Code,
                ChannelCode = channel.Code,
                ChannelName= channel.Name,
                Status = r.Status,
            })
            .ToPageResultAsync(request.PageIndex, request.PageSize);

        if (null != pagedTasks.List && pagedTasks.List.Any())
        {
            var taskIds = pagedTasks.List.Select(x => x.Id).ToList();

            // 一次性获取所有需要的算法名称信息
            var algoInfos = await sqlSugarClient.Queryable<Tb_Task_Algo>()
                .Where(r => !r.IsDelete)
                .Where(r => taskIds.Contains(r.TaskId))
                .LeftJoin<Tb_Algo_Mode>((r, algoMode) => r.AlgoId == algoMode.Id)
                .Select((r, algoMode) => new
                {
                    TaskId = r.TaskId,
                    AlgoName = algoMode.Name
                })
                .ToListAsync();

            // 按任务ID分组算法名称
            var algoNamesByTask = algoInfos
                .GroupBy(r => r.TaskId)
                .ToDictionary(
                    g => g.Key,
                    g => g.Select(x => x.AlgoName).Where(name => !string.IsNullOrEmpty(name)).ToList()
                );

            // 填充每个任务的算法名称列表
            foreach (var task in pagedTasks.List)
            {
                task.ModelNames = algoNamesByTask.TryGetValue(task.Id, out var names)
                    ? names
                    : new List<string>();
            }
        }
        return pagedTasks;
    }

    /// <summary>
    /// 获取任务详情
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    public async Task<GetTaskDetailResult> GetTaskDetailAsync(string id)
    {
        var data = await sqlSugarClient.Queryable<Tb_Task>()
            .Where(r => !r.IsDelete)
            .Where(r => r.Id == id)
            .LeftJoin<Tb_Channel>((r, channel) => r.ChannelId == channel.Id)
            .Select((r, channel) => new GetTaskDetailResult
            {
                Id = r.Id,
                Code = r.Code,
                Remark = r.Remark,
                ChannelId = r.ChannelId,
                ChannelCode = channel.Code,
                ReportAddress = r.ReportAddress,
                 CooldownTime = r.CooldownTime,
                 DetectSection = r.DetectSection,   
                 FrameInterval = r.FrameInterval,
                 VideoDuration = r.VideoDuration,
                 DetectThreshold = r.DetectThreshold
            })
            .FirstAsync();

        if (null == data)
        {
            throw new ResultMessageException("任务不存在");
        }

        // 任务算法关联
        var taskAlgoResults = await sqlSugarClient.Queryable<Tb_Task_Algo>()
            .Where(r => !r.IsDelete)
            .Where(r => r.TaskId == id)
            .LeftJoin<Tb_Algo_Mode>((r, algoMode) => r.AlgoId == algoMode.Id)
            .Select((r, algoMode) => new
            {
                AlgoId = r.AlgoId,
                AlgoName = algoMode.Name,
                ValueConfigJson = r.ValueConfig // 先获取JSON字符串
            })
            .ToListAsync();

        data.TaskAlgoResult = taskAlgoResults.Select(r => new TaskAlgoResult
        {
            AlgoId = r.AlgoId,
            AlgoName = r.AlgoName,
            ValueConfig = JsonSerializer.Deserialize<List<TaskAlgoConfig>>(r.ValueConfigJson)
        }).ToList();
        return data;
    }

    /// <summary>
    /// 获取任务算法配置详情
    /// </summary>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    public async Task<TaskAlgoConfigDetail> GetTaskAlgoConfigDetailAsync(string task_id)
    {
        TaskAlgoConfigDetail result = new TaskAlgoConfigDetail();
        var task = await sqlSugarClient.Queryable<Tb_Task>()
            .Where(r => r.Id == task_id && !r.IsDelete)
            .FirstAsync();
        if (task == null)
        {
            throw new ResultMessageException("任务不存在");
        }

        var channelInfo = await sqlSugarClient.Queryable<Tb_Channel>()
            .Where(r => r.Id == task.ChannelId && !r.IsDelete)
            .FirstAsync();
        result.TaskId= task.Id;
        result.TaskName = task.Code;
        result.ChannelAddress = channelInfo?.Address;
        result.FrameInterval = task.FrameInterval;
        result.DetectThreshold = task.DetectThreshold;
        result.DetectSection = task.DetectSection;
        result.VideoDuration = task.VideoDuration;
        result.CooldownTime = task.CooldownTime;
        result.Model = new List<TaskAlgoModel>();
        //任务对应的模型算法以及参数
        var taskAlgo = await sqlSugarClient.Queryable<Tb_Task_Algo>()
            .Where(r => r.TaskId == task.Id && !r.IsDelete)
            .LeftJoin<Tb_Algo_Mode>((r, algoMode) => r.AlgoId == algoMode.Id)
            .Select((r, algoMode) => new 
            {
                r.AlgoId,
                algoMode.Code,
                algoMode.Name,
                r.ValueConfig
            })
            .ToListAsync();
        foreach (var algo in taskAlgo)
        {
            TaskAlgoModel model = new TaskAlgoModel();
            model.AlogId= algo.AlgoId;
            model.AlgoName=algo.Name;
            model.AlgoCode = algo.Code;
            if (algo.ValueConfig != null)
            {
                model.AlgoConfig = JsonSerializer.Deserialize<List<TaskAlgoConfig>>(algo.ValueConfig);
            }
            model.TaskArea = await sqlSugarClient.Queryable<Tb_Task_Area>()
                .Where(r => r.TaskId == task.Id && !r.IsDelete)
                .Select((r) => new TaskArea
                {
                    RegionType = (int)r.RegionType,
                    Points = r.Points,
                    AlgoModeId = r.AlgoModeId
                })
                .ToListAsync();
            result.Model.Add(model);
        }
        return result;
    }

    /// <summary>
    /// 添加任务
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    public async Task AddTaskAsync(AddTaskRequest request)
    {
        var codeExists = await sqlSugarClient.Queryable<Tb_Task>()
            .Where(r => r.Code == request.Code && !r.IsDelete)
            .AnyAsync();

        if (codeExists)
        {
            throw new ResultMessageException("任务编号已存在，请修改后重试！");
        }

        if (!await IsChannelExistsAsync(request.ChannelId))
        {
            throw new ResultMessageException("选择的通道不存在或已被删除，请刷新后重试！");
        }

        var algoIds = request.TaskAlgoRequest.Select(r => r.AlgoId).Distinct().ToList();
        if (!await AreAlgoModelsExistAsync(algoIds))
        {
            throw new ResultMessageException("部分算法模型不存在或已被删除，请刷新后重试！");
        }

        var add = new Tb_Task
        {
            Id = PKManager.UUID(),
            Code = request.Code,
            Remark = request.Remark,
            ChannelId = request.ChannelId,
            ReportAddress = request.ReportAddress,
            Status = TaskStatusEnum.Stopped,
            FrameInterval = request.FrameInterval,
            DetectThreshold = request.DetectThreshold,
            DetectSection = request.DetectSection,
            VideoDuration = request.VideoDuration,
            CooldownTime = request.CooldownTime
        };
        var addList = request.TaskAlgoRequest.Select(r => new Tb_Task_Algo()
        {
            TaskId = add.Id,
            AlgoId = r.AlgoId,
            ValueConfig = JsonSerializer.Serialize(r.ValueConfig),
        }).ToList();

        await sqlSugarClient.Insertable(add).ExecuteCommandAsync();
        await sqlSugarClient.Insertable(addList).ExecuteCommandAsync();
    }

    /// <summary>
    /// // 修改任务
    /// </summary>
    /// <param name="request"></param>
    /// <returns></returns>
    /// <exception cref="ResultMessageException"></exception>
    public async Task UpdateTaskAsync(UpdateTaskRequest request)
    {
        var taskData = await sqlSugarClient.FirstAsync<Tb_Task>(r => !r.IsDelete && r.Id == request.Id);
        if (null == taskData)
        {
            throw new ResultMessageException("任务不存在");
        }

        if (taskData.Code != request.Code)
        {
            var codeExists = await sqlSugarClient.Queryable<Tb_Task>()
                .Where(r => r.Code == request.Code && !r.IsDelete && r.Id != request.Id)
                .AnyAsync();

            if (codeExists)
            {
                throw new ResultMessageException("任务编号已存在，请修改后重试！");
            }
        }

        if (!await IsChannelExistsAsync(request.ChannelId))
        {
            throw new ResultMessageException("选择的通道不存在或已被删除，请刷新后重试！");
        }

        var algoIds = request.TaskAlgoRequest.Select(r => r.AlgoId).Distinct().ToList();
        if (!await AreAlgoModelsExistAsync(algoIds))
        {
            throw new ResultMessageException("部分算法模型不存在或已被删除，请刷新后重试！");
        }

        taskData.Code = request.Code;
        taskData.Remark = request.Remark;
        taskData.ChannelId = request.ChannelId;
        taskData.ReportAddress = request.ReportAddress;
        taskData.CooldownTime = request.CooldownTime;
        taskData.FrameInterval = request.FrameInterval;
        taskData.DetectThreshold = request.DetectThreshold;
        taskData.DetectSection = request.DetectSection;
        taskData.VideoDuration = request.VideoDuration;


        await sqlSugarClient.Updateable(taskData).ExecuteCommandAsync();

        // 任务算法关联
        var taskAlgoData = await sqlSugarClient.Queryable<Tb_Task_Algo>()
            .Where(r => !r.IsDelete)
            .Where(r => r.TaskId == request.Id)
            .ToListAsync();
        if (null != taskAlgoData && taskAlgoData.Any())
        {
            // 软删除旧关联数据
            await sqlSugarClient.Updateable<Tb_Task_Algo>()
                .SetColumns(r => r.IsDelete == true)
                .Where(r => r.TaskId == request.Id)
                .ExecuteCommandAsync();
        }

        // 新增新关联数据
        var addList = request.TaskAlgoRequest.Select(r => new Tb_Task_Algo()
        {
            TaskId = request.Id,
            AlgoId = r.AlgoId,
            ValueConfig = JsonSerializer.Serialize(r.ValueConfig),
        }).ToList();

        await sqlSugarClient.Insertable(addList).ExecuteCommandAsync();
    }

    // <summary>
    /// 删除任务
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    public async Task DeleteTaskAsync(string id)
    {
        var taskData = await sqlSugarClient.FirstAsync<Tb_Task>(r => !r.IsDelete && r.Id == id);
        if (null == taskData)
        {
            throw new ResultMessageException("任务不存在");
        }

        // 软删除任务
        await sqlSugarClient.Updateable<Tb_Task>()
            .SetColumns(r => r.IsDelete == true)
            .Where(r => r.Id == id)
            .ExecuteCommandAsync();

        // 软删除任务算法关联
        await sqlSugarClient.Updateable<Tb_Task_Algo>()
            .SetColumns(r => r.IsDelete == true)
            .Where(r => !r.IsDelete)
            .Where(r => r.TaskId == id)
            .ExecuteCommandAsync();
    }

    /// <summary>
    /// 修改任务状态
    /// </summary>
    /// <param name="id"></param>
    /// <param name="status"></param>
    /// <returns></returns>
    public async Task UpdateTaskStatusAsync(string id, TaskStatusEnum status)
    {
        var token = identityService.Token;
        var taskData = await sqlSugarClient.FirstAsync<Tb_Task>(r => !r.IsDelete && r.Id == id);

        var taskLog = new Tb_Task_Logs()
        {
            TaskId = taskData.Id,
            Remark = "任务" + taskData.Code,
        };
        if (null == taskData)
        {
            throw new ResultMessageException("任务不存在");
        }
        if (status == TaskStatusEnum.Running)
        {
           var result = await aiTaskService.StartTaskAsync(taskData.Id,token);

            if (result.IsSuccess == false)
            {
                taskLog.LogMsg = result.Message;
                taskLog.Remark += "启动失败";
                taskLog.Status = 2;
                await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
                throw new ResultMessageException(result.Message);
            }
            taskLog.Remark +="启动成功";
            taskLog.Status = 1;
            await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
        }
        else if(status== TaskStatusEnum.Stopped)
        {
            var result = await aiTaskService.StopTaskAsync(taskData.Id,token);


            if (result.IsSuccess == false)
            {
                taskLog.LogMsg = result.Message;
                taskLog.Remark += "停止失败";
                taskLog.Status= 2;
                await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
                throw new ResultMessageException(result.Message);
            }
            taskLog.Remark += "停止成功";
            taskLog.Status = 1;
            await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
        }
        await sqlSugarClient.Updateable<Tb_Task>().SetColumns(r => r.Status == status).Where(r => r.Id == id).ExecuteCommandAsync();
    }

    /// <summary>
    /// 停用任务
    /// </summary>
    /// <param name="id"></param>
    /// <returns></returns>
    public async Task StopTaskAsync(string id)
    {
        var taskData = await sqlSugarClient.FirstAsync<Tb_Task>(r => !r.IsDelete && r.Id == id);
        if (null == taskData)
        {
            throw new ResultMessageException("任务不存在");
        }

        if (taskData.Status == TaskStatusEnum.Stopped)
        {
            throw new ResultMessageException("任务已经是停用状态");
        }

        await sqlSugarClient.Updateable<Tb_Task>()
            .SetColumns(r => r.Status == TaskStatusEnum.Stopped)
            .Where(r => r.Id == id)
            .ExecuteCommandAsync();
    }

    /// <summary>
    /// 获取任务下拉列表
    /// </summary>
    public async Task<List<GetTaskDropdownResult>> GetTaskDropdownAsync()
    {
        var result = new List<GetTaskDropdownResult>();

        // 1. 合流通道
        var videoRouteConfig = await sqlSugarClient.Queryable<Tb_Param_Config>()
            .Where(r => r.Code == SysConfCode.VideoRoute)
            .FirstAsync();

        var mergeChannels = new List<GetTaskDropdownResult>();
        if (videoRouteConfig != null && int.TryParse(videoRouteConfig.ParamValue, out int videoRoute))
        {
            int channelCount = videoRoute / 9;
            for (int i = 1; i <= channelCount; i++)
            {
                mergeChannels.Add(new GetTaskDropdownResult
                {
                    Id = $"all_{i}",
                    Code = $"通道{i}"
                });
            }
        }

        result.Add(new GetTaskDropdownResult
        {
            Id = "root_merge",
            Code = "合流通道",
            Children = mergeChannels
        });

        // 2. 任务通道
        var tasks = await sqlSugarClient.Queryable<Tb_Task>()
            .Where(r => !r.IsDelete)
            .Select(r => new GetTaskDropdownResult
            {
                Id = r.Id,
                Code = r.Code
            })
            .ToListAsync();

        result.Add(new GetTaskDropdownResult
        {
            Id = "root_task",
            Code = "任务通道",
            Children = tasks
        });

        return result;
    }

    /// <summary>
    /// 获取任务区域列表
    /// </summary>
    public async Task<GetTaskAreaResult> GetTaskAreasAsync(string taskId)
    {
        var taskData = await sqlSugarClient.FirstAsync<Tb_Task>(r => !r.IsDelete && r.Id == taskId);
        if (null == taskData)
        {
            throw new ResultMessageException("任务不存在");
        }

        var areas = await sqlSugarClient.Queryable<Tb_Task_Area>()
            .Where(r => !r.IsDelete && r.TaskId == taskId)
            .ToListAsync();

        var areaRules = areas.Select(area => new AreaRule
        {
            Id = area.Id,
            Name = area.Name,
            RuleType = area.RuleType,
            RegionType = area.RegionType,
            AlgoModeId = string.IsNullOrEmpty(area.AlgoModeId)
                ? new List<string>()
                : area.AlgoModeId.Split(',').ToList(),
            Points = string.IsNullOrEmpty(area.Points)
                ? new List<Point>()
                : JsonSerializer.Deserialize<List<Point>>(area.Points)
        }).ToList();

        // 获取相关的算法模型信息
        var taskAlgoList = await sqlSugarClient.Queryable<Tb_Task_Algo>()
            .Where(r => !r.IsDelete && r.TaskId == taskId)
            .ToListAsync();

        var algoModeIds = taskAlgoList.Select(r => r.AlgoId).Distinct().ToList();

        var algoModes = new List<AlgoMode>();
        if (algoModeIds.Any())
        {
            var algoModeList = await sqlSugarClient.Queryable<Tb_Algo_Mode>()
                .Where(r => !r.IsDelete && algoModeIds.Contains(r.Id))
                .Select(r => new AlgoMode
                {
                    Id = r.Id,
                    Name = r.Name,
                    MajorOnly = r.MajorOnly,
                    ZoneRequired = r.ZoneRequired
                })
                .ToListAsync();
            algoModes = algoModeList;
        }
        var photoResult = await aiTaskService.GetAreaImagesAsync(taskId,identityService.Token);
        if (!photoResult.IsSuccess)
        {
            throw new ResultMessageException("获取区域图片失败");
        }
        return new GetTaskAreaResult
        {
            Photo = "data:image/jpeg;base64,"+photoResult.Data,
            TaskCode = taskData.Code,
            TaskId = taskId,
            AreaRules = areaRules,
            AlgoModes = algoModes
        };
    }

    /// <summary>
    /// 重启任务
    /// </summary>
    /// <param name="taskId">任务ID</param>
    /// <param name="taskCode">任务代码</param>
    /// <param name="token">认证令牌</param>
    private async Task RestartTaskAsync(string taskId, string taskCode, string token)
    {
        var taskLog = new Tb_Task_Logs()
        {
            TaskId = taskId,
            Remark = $"任务{taskCode}重启",
            Status = 1
        };
        
        try
        {
            // 主动重启任务，无论当前状态如何
            // 停止AI任务
            logger.LogInformation($"开始停止任务: {taskCode}({taskId})");
            var stopResult = await aiTaskService.StopTaskAsync(taskId, token);
            
            if (!stopResult.IsSuccess)
            {
                logger.LogError($"停止任务失败: {taskCode}({taskId}), 错误信息: {stopResult.Message}");
                taskLog.LogMsg = $"停止任务失败: {stopResult.Message}";
                taskLog.Status = 2;
                await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
                return;
            }
            
            logger.LogInformation($"任务停止成功: {taskCode}({taskId})");
            
            // 延迟等待任务完全停止
            await Task.Delay(5000);
            
            // 启动AI任务
            logger.LogInformation($"开始启动任务: {taskCode}({taskId})");
            var startResult = await aiTaskService.StartTaskAsync(taskId, token);
            
            if (!startResult.IsSuccess)
            {
                logger.LogError($"启动任务失败: {taskCode}({taskId}), 错误信息: {startResult.Message}");
                taskLog.LogMsg = $"启动任务失败: {startResult.Message}";
                taskLog.Status = 2;
                  
                // 更新任务状态为停止
                await sqlSugarClient.Updateable<Tb_Task>()
                    .SetColumns(r => r.Status == TaskStatusEnum.Stopped)
                    .Where(r => r.Id == taskId)
                    .ExecuteCommandAsync();
            }
            else
            {
                logger.LogInformation($"任务启动成功: {taskCode}({taskId})");
                taskLog.LogMsg = "任务重启成功";
                  
                // 更新任务状态为运行
                await sqlSugarClient.Updateable<Tb_Task>()
                    .SetColumns(r => r.Status == TaskStatusEnum.Running)
                    .Where(r => r.Id == taskId)
                    .ExecuteCommandAsync();
            }
            
            await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
        }
        catch (Exception ex)
        {
            logger.LogError(ex, $"重启任务时发生异常: {taskCode}({taskId})");
            taskLog.LogMsg = $"重启任务时发生异常: {ex.Message}";
            taskLog.Status = 2;
            await sqlSugarClient.Insertable(taskLog).ExecuteCommandAsync();
            
            // 发生异常时确保任务状态正确
            await sqlSugarClient.Updateable<Tb_Task>()
                .SetColumns(r => r.Status == TaskStatusEnum.Stopped)
                .Where(r => r.Id == taskId)
                .ExecuteCommandAsync();
        }
    }
    
    /// <summary>
    /// 新增任务区域
    /// </summary>
    public async Task AddTaskAreasAsync(AddTaskAreaRequest request)
    {
        var taskData = await sqlSugarClient.FirstAsync<Tb_Task>(r => !r.IsDelete && r.Id == request.TaskId);
        if (null == taskData)
        {
            throw new ResultMessageException("任务不存在");
        }

        // 软删除旧的任务区域数据
        await sqlSugarClient.Updateable<Tb_Task_Area>()
            .SetColumns(r => r.IsDelete == true)
            .Where(r => r.TaskId == request.TaskId && !r.IsDelete)
            .ExecuteCommandAsync();

        var addList = request.AreaRules.Select(rule => new Tb_Task_Area()
        {
            Id = PKManager.UUID(),
            TaskId = request.TaskId,
            Name = rule.Name,
            RuleType = rule.RuleType,
            RegionType = rule.RegionType,
            Points = JsonSerializer.Serialize(rule.Points),
            AlgoModeId = string.Join(",", rule.AlgoModeId)
        }).ToList();

        if (addList.Any())
        {
            await sqlSugarClient.Insertable(addList).ExecuteCommandAsync();
        }
    }

    /// <summary>
    /// 是否存在该通道
    /// </summary>
    /// <param name="channelId"></param>
    /// <returns></returns>

    private async Task<bool> IsChannelExistsAsync(string channelId)
    {
        return await sqlSugarClient.Queryable<Tb_Channel>()
            .Where(r => r.Id == channelId && !r.IsDelete)
            .AnyAsync();
    }

    /// <summary>
    /// 验证算法模型是否存在
    /// </summary>
    /// <param name="algoIds"></param>
    /// <returns></returns>
    private async Task<bool> AreAlgoModelsExistAsync(List<string>? algoIds)
    {
        if (algoIds == null || !algoIds.Any())
            return true;

        var existingCount = await sqlSugarClient.Queryable<Tb_Algo_Mode>()
            .Where(r => algoIds.Contains(r.Id) && !r.IsDelete)
            .CountAsync();

        return existingCount == algoIds.Count;
    }
}