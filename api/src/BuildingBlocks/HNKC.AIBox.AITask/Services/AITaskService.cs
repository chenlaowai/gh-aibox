using HNKC.AIBox.Models;
using HNKC.AIBox.Models.Dtos.Task;
using HNKC.BPM.Base.Sdk;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.AITask
{
    public class AITaskService(IConfiguration configuration, HttpClientHelper httpClient) : IAITaskService
    {

        private readonly string baseUrl = configuration["AITask:BaseUrl"];

        /// <summary>
        /// 启动任务
        /// </summary>
        /// <returns></returns>
        public async Task<Result> StartTaskAsync(string taskId, string token)
        {
            try
            {
                var url = baseUrl + "task/start";
                var headers = new Dictionary<string, string>
            {
                { "Authorization", token }
            };
                var model = new
                {
                    taskId = taskId
                };
                var result = await httpClient.PostAsync(url, headers, JsonConvert.SerializeObject(model), timeoutSeconds: 10);
           
                if (result != null)
                {
					Console.WriteLine("启动任务成功：调用的地址为：" + url);
					return Result.Success();
                }
                return Result.Fail("启动任务失败");
            }
            catch (Exception ex)
            {
                return Result.Fail(ex.Message);
            }

        }


        /// <summary>
        /// 停止任务
        /// </summary>
        /// <param name="taskName"></param>
        /// <returns></returns>
        public async Task<Result> StopTaskAsync(string taskId, string token)
        {
            try
            {
                var url = baseUrl + "task/stop";
                var headers = new Dictionary<string, string>
            {
                { "Authorization", token }
            };
                var model = new
                {
                    taskId = taskId
                };
                var result = await httpClient.PostAsync(url, headers, JsonConvert.SerializeObject(model), timeoutSeconds: 10);

                if (result != null)
                {
					Console.WriteLine("停止任务成功：调用的地址为：" + url);
					return Result.Success();
                }
                return Result.Fail("停止任务失败");
            }
            catch (Exception ex)
            {
                return Result.Fail(ex.Message);
            }

        }




        /// <summary>
        /// 获取任务区域图片
        /// </summary>
        /// <param name="taskId"></param>
        /// <param name="token"></param>
        /// <returns></returns>
        public async Task<Result<string>> GetAreaImagesAsync(string taskId, string token)
        {
            try
            {
                var url = baseUrl + $@"task/get_area_frame?taskId={taskId}";
                var headers = new Dictionary<string, string>
            {
                { "Authorization", token }
            };
                var result = await httpClient.GetAsync(url, headers, timeoutSeconds: 5);

                if (result != null)
                {
                   var imgresult = JsonConvert.DeserializeObject<AreaImgModel>(result);
                    if (imgresult.status != "ok")
                    {
                        return Result<string>.Fail(imgresult.msg);
                    }


                    return Result.Success<string>(imgresult.frame_area);
                }
                return Result<string>.Fail("获取任务状态失败");
            }
            catch (Exception ex)
            {
                return Result<string>.Fail(ex.Message);
            }
        }

        /// <summary>
        /// 获取资源占用率
        /// </summary>
        /// <param name="token"></param>
        /// <returns></returns>
        public async Task<Result<OccupancyModel>> GetOccupancyAsync(string token)
        {
            try
            {
                var url = baseUrl + "get_occupancy";
                var headers = new Dictionary<string, string>
                {
                    { "Authorization", token }
                };
                var result = await httpClient.GetAsync(url, headers, timeoutSeconds: 2);

                if (result != null)
                {
                    var occupancyResult = JsonConvert.DeserializeObject<OccupancyModel>(result);
                    if (occupancyResult.status != "ok")
                    {
                        return Result<OccupancyModel>.Fail(occupancyResult.msg);
                    }
                    return Result<OccupancyModel>.Success(occupancyResult);
                }
                return Result<OccupancyModel>.Fail("获取资源占用率失败");
            }
            catch (Exception ex)
            {
               Console.WriteLine(ex.Message);
                return Result<OccupancyModel>.Fail(ex.Message);
            }
        }
    }
}