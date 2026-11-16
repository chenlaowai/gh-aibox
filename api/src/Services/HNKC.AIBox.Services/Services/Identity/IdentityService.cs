using HNKC.AIBox.Exceptions;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Caching.Memory;
using System.Net.Http;
using System.Security.Claims;

namespace HNKC.AIBox.Services
{
    public class IdentityService(IMemoryCache memoryCache, IHttpContextAccessor contextAccessor) :IIdentityService
    {

        /// <summary>
        /// 获取用户ID
        /// </summary>
        public string UserId
        {
            get
            {
                var userId = contextAccessor?.HttpContext?.User?.FindFirst(ClaimTypes.NameIdentifier)?.Value
                    ?? throw new UnauthorizedException($"token失效");
                return userId;
            }
        }

        /// <summary>
        /// 获取请求token
        /// </summary>
        public string Token
        {
            get { 
            
              var authHeader = contextAccessor?.HttpContext.Request.Headers["Authorization"].FirstOrDefault();

                return authHeader;
            }
        }
    }
}
