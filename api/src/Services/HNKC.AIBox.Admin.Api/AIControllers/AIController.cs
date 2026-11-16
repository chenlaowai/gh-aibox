using HNKC.AIBox.Models;
using HNKC.AIBox.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Admin.Api.AIControllers
{
    /// <summary>
    /// 与AI算法对接的接口
    /// </summary>
    /// <param name="alarmService"></param>
    /// <param name="taskService"></param>
    [Route("api/[controller]/[action]")]
    [ApiController]
    [Authorize]
    public class AIController(IAlarmService alarmService,ITaskService taskService) : ControllerBase
    {

        /// <summary>
        /// 添加AI预警事件
        /// </summary>
        /// <param name="request"></param>
        /// <returns></returns>
        [HttpPost]
        public async Task<IActionResult> ReportAlarmEventAsync([FromBody] AddAIAlarmRequest request)
        {
            var result = await alarmService.AddAIAlarmEventAsync(request);
            return result;
        }

        /// <summary>
        /// 获取任务算法配置详情
        /// </summary>
        /// <param name="task_name"></param>
        /// <returns></returns>
        [HttpGet]
        [AllowAnonymous]
        [ProducesResponseType(typeof(TaskAlgoConfigDetail), StatusCodes.Status200OK)]
        public async Task<IActionResult> TaskAlgoConfigDetailAsync([FromQuery] string task_id)
        {
            return Ok(await taskService.GetTaskAlgoConfigDetailAsync(task_id));
        }
    }
}
