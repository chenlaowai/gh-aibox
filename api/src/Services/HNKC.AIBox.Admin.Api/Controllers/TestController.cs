
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Services;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Admin.Api.Controllers
{
    [Route("api/[controller]/[action]")]
    [ApiController]
    [Authorize]
    public class TestController(IFileService fileService) : ControllerBase
    {
        /// <summary>
        /// 操作日志测试
        /// </summary>
        /// <returns></returns>
        [HttpPost]
  
        public async Task<IActionResult> DeleteFilePathTest(string path)
        {

            throw new ResultMessageException("删除文件测试");

         //   fileService.Delete(path);

            return Ok();
        }



    }
}
