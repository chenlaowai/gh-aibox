using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Services;
using HNKC.AIBox.Web;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;
using System.Security.Claims;
using Microsoft.Extensions.Caching.Memory;

namespace HNKC.AIBox.Admin.Api.Controllers
{
    [Route("api/[controller]/[action]")]
    [ApiController]
    public class AuthController(
        ISqlSugarClient sqlSugarClient,
        IAuthService authService,
        IMemoryCache memoryCache,
        IIdentityService identityService) : ControllerBase
    {
        /// <summary>
        /// 登录
        /// </summary>
        /// <returns></returns>
        [HttpPost]
        [ProducesResponseType(typeof(TokenInfoResult), StatusCodes.Status200OK)]
        public async Task<IActionResult> LoginAsync([FromBody] LoginRequest request)
        {
            return await authService.LoginAsync(request);
        }

        /// <summary>
        /// 刷新Token
        /// </summary>
        /// <returns></returns>
        [HttpPost]
        [ProducesResponseType(typeof(TokenInfoResult), StatusCodes.Status200OK)]
        public async Task<IActionResult> RefreshTokenAsync([FromBody] RefreshTokenRequest request)
        {
            return await authService.RefreshTokenAsync(request.RefreshToken);
        }


        /// <summary>
        /// 退出登录
        /// </summary>
        /// <returns></returns>
        [HttpPost]
        [Authorize]
        public async Task<IActionResult> LogoutAsync([FromBody] RefreshTokenRequest request)
        {
            var userId = identityService.UserId;
            memoryCache.Remove(userId);
            if (!memoryCache.TryGetValue(request.RefreshToken, out string refreshTokenUserId) ||
                userId != refreshTokenUserId)
            {
                throw new ResultMessageException("退出登录失败");
            }

            memoryCache.Remove(request.RefreshToken);
            return Ok();
        }
    }
}