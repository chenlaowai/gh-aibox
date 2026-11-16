using HNKC.AIBox.Models;
using HNKC.AIBox.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Admin.Api.Controllers
{
    [Route("api/[controller]/[action]")]
    [ApiController]
    [Authorize]
    public class SystemController(ISystemService systemService) : ControllerBase
    {
        /// <summary>
        /// 获取系统字典枚举
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        [ProducesResponseType(typeof(Dictionary<string, DictResultDto>), StatusCodes.Status200OK)]
        public async Task<IActionResult> SystemDict()
        {
            return Ok(await systemService.GetSystemDict());
        }
    }
}
