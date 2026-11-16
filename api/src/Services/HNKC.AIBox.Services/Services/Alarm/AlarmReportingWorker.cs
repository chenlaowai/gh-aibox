using HNKC.AIBox.Entities;
using HNKC.AIBox.Enums;
using HNKC.AIBox.Models.Dtos.Alarm;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using SqlSugar;
using System;
using System.IO;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{
    /// <summary>
    /// 告警上报后台服务
    /// </summary>
    public class AlarmReportingWorker : BackgroundService
    {
        private readonly IServiceScopeFactory _serviceScopeFactory;
        private readonly ILogger<AlarmReportingWorker> _logger;

        public AlarmReportingWorker(IServiceScopeFactory serviceScopeFactory, ILogger<AlarmReportingWorker> logger)
        {
            _serviceScopeFactory = serviceScopeFactory;
            _logger = logger;
        }

        protected override async Task ExecuteAsync(CancellationToken stoppingToken)
        {
            _logger.LogInformation("AlarmReportingWorker starting.");

            while (!stoppingToken.IsCancellationRequested)
            {
                try
                {
                    await ProcessAlarmsAsync(stoppingToken);
                }
                catch (Exception ex)
                {
                    _logger.LogError(ex, "Error processing alarms.");
                }

                // Wait for a short period before next poll (e.g., 5 seconds)
                // This is the loop interval, distinct from the alarm delay.
                await Task.Delay(5000, stoppingToken);
            }
        }

        private async Task ProcessAlarmsAsync(CancellationToken stoppingToken)
        {
            using var scope = _serviceScopeFactory.CreateScope();
            var sqlSugar = scope.ServiceProvider.GetRequiredService<ISqlSugarClient>();
            var httpClientFactory = scope.ServiceProvider.GetRequiredService<IHttpClientFactory>();
            var env = scope.ServiceProvider.GetRequiredService<IWebHostEnvironment>();

            // 1. Get configuration
            var configDelay = await sqlSugar.Queryable<Tb_Param_Config>()
                .FirstAsync(p => p.Code == SysConfCode.AlarmReportDelaySeconds, stoppingToken);
            var configRetry = await sqlSugar.Queryable<Tb_Param_Config>()
                .FirstAsync(p => p.Code == SysConfCode.AlarmReportRetryIntervalSeconds, stoppingToken);

            int delaySeconds = 0;
            int retrySeconds = 60; // Default retry 60s

            if (configDelay != null && int.TryParse(configDelay.ParamValue, out int d)) delaySeconds = d;
            if (configRetry != null && int.TryParse(configRetry.ParamValue, out int r)) retrySeconds = r;

            var now = DateTime.Now;
            var pendingCutoff = now.AddSeconds(-delaySeconds);
            var retryCutoff = now.AddSeconds(-retrySeconds);

            // 2. Query eligible alarms
            // Pending and older than delay OR Failed and older than retry
            var alarms = await sqlSugar.Queryable<Tb_Alarm>()
                .Where(a => !a.IsDelete && !string.IsNullOrEmpty(a.ReportAddress))
                .Where(a => (a.ReportStatus == ReportStatusEnum.Pending && a.AlarmTime <= pendingCutoff) ||
                            (a.ReportStatus == ReportStatusEnum.Failed && a.Modified <= retryCutoff))
                .OrderBy(a => a.AlarmTime)
                .Take(10) // Batch size
                .ToListAsync(stoppingToken);

            if (alarms.Count == 0) return;

            var client = httpClientFactory.CreateClient();

            foreach (var alarm in alarms)
            {
                if (stoppingToken.IsCancellationRequested) break;

                try
                {
                    // Fetch related names
                    var task = await sqlSugar.Queryable<Tb_Task>().FirstAsync(t => t.Id == alarm.TaskId);
                    var channel = await sqlSugar.Queryable<Tb_Channel>().FirstAsync(c => c.Id == alarm.ChannelId);
                    var type = await sqlSugar.Queryable<Tb_Alarm_Type>().FirstAsync(t => t.Id == alarm.TypeId);

                    // Prepare Files
                    string imgPath = GetPhysicalPath(env, alarm.ImgPath);
                    string videoPath = GetPhysicalPath(env, alarm.VideoPath);
                    string imgBase64 = await GetFileBase64Async(imgPath);
                    string videoBase64 = await GetFileBase64Async(videoPath);

                    // Create DTO
                    var dto = new AlarmReportDto
                    {
                        AlarmId = alarm.Id,
                        TaskName = task?.Code, // Using Code as requested or Remark? Standard seems to be Code for ID/Name in this system.
                        ChannelName = channel?.Name,
                        AlarmTypeName = type?.Remark, // Remark is usually the Chinese name for types
                        AlarmTime = alarm.AlarmTime,
                        ImageBase64 = imgBase64,
                        VideoBase64 = videoBase64
                    };

                    // Send
                    var json = JsonConvert.SerializeObject(dto);
                    var content = new StringContent(json, Encoding.UTF8, "application/json");
                    var response = await client.PostAsync(alarm.ReportAddress, content, stoppingToken);

                    if (response.IsSuccessStatusCode)
                    {
                        alarm.ReportStatus = ReportStatusEnum.Success;
                        _logger.LogInformation($"Alarm {alarm.Id} reported successfully.");
                    }
                    else
                    {
                        alarm.ReportStatus = ReportStatusEnum.Failed;
                        _logger.LogWarning($"Alarm {alarm.Id} report failed: {response.StatusCode}");
                    }
                }
                catch (Exception ex)
                {
                    alarm.ReportStatus = ReportStatusEnum.Failed;
                    _logger.LogError(ex,($"Alarm {alarm.Id} report exception."));
                }
                finally
                {
                    alarm.Modified = DateTime.Now; // Update modified time for retry logic
                    await sqlSugar.Updateable(alarm).UpdateColumns(a => new { a.ReportStatus, a.Modified }).ExecuteCommandAsync();
                }
            }
        }

        private string GetPhysicalPath(IWebHostEnvironment env, string path)
        {
            if (string.IsNullOrEmpty(path)) return null;
            if (Path.IsPathRooted(path)) return path;
            return Path.Combine(env.WebRootPath, path.TrimStart('/', '\\'));
        }

        private async Task<string> GetFileBase64Async(string path)
        {
            if (string.IsNullOrEmpty(path) || !File.Exists(path)) return null;
            try
            {
                byte[] bytes = await File.ReadAllBytesAsync(path);
                return Convert.ToBase64String(bytes);
            }
            catch
            {
                return null;
            }
        }
    }
}
