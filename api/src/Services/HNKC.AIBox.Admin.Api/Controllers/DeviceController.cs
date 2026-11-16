using HNKC.AIBox.AITask;
using HNKC.AIBox.Entities;
using HNKC.AIBox.Models;
using HNKC.AIBox.Models.Dtos.Task;
using HNKC.AIBox.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;
using System.Diagnostics;

namespace HNKC.AIBox.Admin.Api.Controllers
{
    /// <summary>
    /// 首页设备
    /// </summary>
    /// <param name="deviceService"></param>
    [Route("api/[controller]/[action]")]
    [ApiController]
     [Authorize]
    public class DeviceController(IConfiguration configuration,
        IDeviceService deviceService,
        ISqlSugarClient sqlSugarClient,
        IAITaskService aiTaskService,
        IIdentityService identityService,
        ILogger<DeviceController> logger) : ControllerBase
    {
        /// <summary>
        /// 获取首页基本信息
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        [ProducesResponseType(typeof(DeviceInfoResult), StatusCodes.Status200OK)]
        public async Task<IActionResult> HomeBasicInfoAsync()
        {
            // 获取设备标识（MAC地址）
            var deviceIdentification = deviceService.GetDeviceIdentification();

            // TODO 获取授权状态
            var authorizationStatus = 1;

            // 获取软件版本
            var softwareVersion = configuration["Version"] ?? "Unknown";

            // 获取资源信息
            var resourceInformation = deviceService.GetDiskInfo();


            // 获取系统版本
            var systemVersion = await deviceService.GetSystemVersionAsync();

            Console.WriteLine("获取cpu温度");
            // 获取CPU温度
            var chipTemperature = await deviceService.GetCpuTemperatureAsync();


            // 获取显卡温度
            var graphicsCardTemperature = await deviceService.GetGpuTemperatureAsync();


            var usages = new List<UsageResult>()
            {
                new()
                {
                    Name = "CPU使用率",
                    Percent = await deviceService.GetCpuUsageAsync()
                }
            };

            return Ok(new DeviceInfoResult
            {
                DeviceIdentification = deviceIdentification,
                AuthorizationStatus = authorizationStatus,
                SoftwareVersion = softwareVersion,
                ResourceInformation = resourceInformation,
                SystemVersion = systemVersion,
                BoxTime = DateTime.Now,
                ChipTemperature = chipTemperature,
                GraphicsCardTemperature = graphicsCardTemperature,
                Usages = usages
            });
        }


        /// <summary>
        /// 获取首页预警信息
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        public async Task<IActionResult> GetDeviceAlarmsAsync()
        {
            var result = await sqlSugarClient.Queryable<Tb_Alarm>()
                .LeftJoin<Tb_Alarm_Type>((p, t) => p.TypeId == t.Id)
               .Where((p, t) => p.IsDelete == false)
               .OrderByDescending((p, t) => p.AlarmTime)
               .Take(10)
               .Select((p, t) => new DeviceAlarm
               {
                   Time = p.AlarmTime,
                   Image = p.ImgPath,
                   ReportStatus  =(int) p.ReportStatus,
                   Description = p.ReportStatus.ToString(),
                   Title = t.Remark
               })
               .ToListAsync();
            return Ok(result);
        }

        /// <summary>
        /// 获取首页任务日志
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        public async Task<IActionResult> GetTaskLogsAsync()
        {
            var result = await sqlSugarClient.Queryable<Tb_Task_Logs>()
               .Where(t => !t.IsDelete)
               .Select(t => new
               {
                   t.Created,
                   t.Remark,
                   t.Id,
                   t.LogMsg,
                   t.Status
               })
               .OrderByDescending(t => t.Created)
               .Take(10)
               .ToListAsync();

            return Ok(result);
        }

        /// <summary>
        /// 获取任务告警排行
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        public async Task<IActionResult> GetTopTaskAlarmAsync()
        {
            var result = await sqlSugarClient.Queryable<Tb_Alarm>()
                .LeftJoin<Tb_Task>((a, t) => a.TaskId == t.Id)
                .Select((a, t) => new {
                    a.TaskId,
                    t.Code,
                    a.Id
                })
                .MergeTable()
                .GroupBy((g) => new { g.TaskId, g.Code }) // 分组键中包含 t.Code
                .Select((g) => new
                {
                    TaskId = g.TaskId,
                    TaskCode = g.Code,
                    TaskAlarmCount = SqlFunc.AggregateCount(g.Id)
                })
                .OrderByDescending(t => t.TaskAlarmCount)
                .ToListAsync();
            return Ok(result);
        }


        /// <summary>
        /// 获取算法告警统计
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        public async Task<IActionResult> GetAlogAlarmStatsAsync()
        {      
            var query = await sqlSugarClient.Queryable<Tb_Alarm>()
                .Where(x => !x.IsDelete)
                .InnerJoin<Tb_Alarm_Type>((a, t) => a.TypeId == t.Id && !t.IsDelete)
                .GroupBy((a, t) => new { t.Id, t.Code })
                .Select((a, t) => new
                {
                    TypeCode = t.Code,
                    AlarmCount = SqlFunc.AggregateCount(a.Id)
                })
                .ToListAsync();

            if (!query.Any())
                return Ok(new List<AlertTypeInfo>());

            var totalAlarms = query.Sum(x => x.AlarmCount);
    
            var result = query.Select(x => new 
            {
                AlertType = x.TypeCode,
                AlertCount = x.AlarmCount,
                Percentage = (double)x.AlarmCount / totalAlarms * 100
            }).ToList();

            return Ok(result);
        }


        /// <summary>
        /// 重启服务
        /// </summary>
        /// <returns></returns>
        [HttpPost]
        public IActionResult RestartService()
        {
            // 直接退出进程，搭配Docker重启策略使用
            try
            {
                logger.LogInformation("RestartService...");
                Environment.Exit(0);
                return Ok();
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "服务重启失败：{message}", ex.Message);
                throw;
            }
        }


        /// <summary>
        /// 重启设备
        /// </summary>
        /// <returns></returns>
        [HttpPost]
        public IActionResult Reboot()
        {
            try
            {
                var processInfo = new ProcessStartInfo
                {
                    FileName = "/bin/bash",
                    Arguments =
                        "-c \"mount -o remount,rw /proc/sys && echo 1 > /proc/sys/kernel/sysrq && echo b > /proc/sysrq-trigger\"",
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                Process.Start(processInfo);
                return Ok();
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "设备重启失败：{message}", ex.Message);
                throw;
            }
        }

        /// <summary>
        /// 获取资源占用率
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        public async Task<IActionResult> GetOccupancyAsync()
        {

            var token = identityService.Token;
            var occupancy = await aiTaskService.GetOccupancyAsync(token);
            
            // 转换为ECharts友好的格式
            var chartData = new {
                series = new [] {
                    new {
                        name = "资源占用率",
                        type = "pie",
                        data = new [] {
                            new { name = "CPU", value = occupancy.Data.cpu },
                            new { name = "内存", value = occupancy.Data.memory },
                            new { name = "TPU", value = occupancy.Data.tpu },
                            new { name = "VPU_1", value = occupancy.Data.vpu_1 },
                            new { name = "VPU_2", value = occupancy.Data.vpu_2 },
                            new { name = "VPU_3", value = occupancy.Data.vpu_3 },
                            new { name = "VPP_1", value = occupancy.Data.vpp_1 },
                            new { name = "VPP_2", value = occupancy.Data.vpp_2 }
                        },
                        home = new {
						  occupancy.Data.disk_free,
                          occupancy.Data.board_temp,
                          occupancy.Data.chip_temp
						}

                    }
                }
            };
            
            return Ok(chartData);
        }
    }
}