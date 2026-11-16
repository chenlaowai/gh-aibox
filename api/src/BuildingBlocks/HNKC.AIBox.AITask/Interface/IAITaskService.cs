using HNKC.AIBox.Models;
using HNKC.AIBox.Models.Dtos.Task;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.AITask
{
    public interface IAITaskService
    {

        /// <summary>
        /// 启动任务
        /// </summary>
        /// <param name="taskName"></param>
        /// <returns></returns>
        Task<Result> StartTaskAsync(string taskId, string token);


        /// <summary>
        /// 停止任务
        /// </summary>
        /// <param name="taskName"></param>
        /// <returns></returns>
        Task<Result> StopTaskAsync(string taskId, string token);




        /// <summary>
        /// 获取任务区域图片
        /// </summary>
        /// <param name="taskId"></param>
        /// <param name="token"></param>
        /// <returns></returns>
        Task<Result<string>> GetAreaImagesAsync(string taskId, string token);

        /// <summary>
        /// 获取资源占用率
        /// </summary>
        /// <param name="token"></param>
        /// <returns></returns>
        Task<Result<OccupancyModel>> GetOccupancyAsync(string token);
    }
}
