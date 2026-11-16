
using System.Collections.Concurrent;
using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Utils;
using HNKC.AIBox.Web;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Identity;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using SqlSugar;
using System.Security.Claims;
using Microsoft.Extensions.Caching.Memory;

namespace HNKC.AIBox.Services
{
    public class AuthService(ISqlSugarClient sqlSugarClient, IMemoryCache memoryCache,
        IHttpContextAccessor contextAccessor, IConfiguration configuration,
        IPasswordHasher<Tb_User> passwordHasher, IIdentityService identityService, IJwtService jwtService, IWebHostEnvironment environment) : IAuthService
    {
        private static readonly ConcurrentDictionary<string, SemaphoreSlim> _keySemaphores = new();

        /// <summary>
        /// 登录
        /// </summary>
        /// <param name="model"></param>
        /// <returns></returns>
        public async Task<Result<TokenInfoResult>> LoginAsync(LoginRequest model)
        {
            var clientIP = contextAccessor.HttpContext?.Connection?.RemoteIpAddress?.ToString();
            if (!environment.IsDevelopment() && !await ValidateSlider(model.SliderCaptcha, model.Account))
            {
                return Result<TokenInfoResult>.Fail("滑块校验失败，请重试");
            }
 
            var user = await sqlSugarClient.Queryable<Tb_User>()
                 .Where(r => !r.IsDelete)
                 .Where(r => r.Account == model.Account)
                 .FirstAsync();
            if (user == null)
            {
                return Result<TokenInfoResult>.Fail("用户名或密码错误");
            }
            // 限制多次尝试
            var isLimit = await IsLoginLimitedAsync(user.Id, clientIP);
            if (isLimit)
            {
                return Result<TokenInfoResult>.Fail("登录过于频繁,请稍微再试");
            }
            if (string.IsNullOrEmpty(model.Password) || !Base64Util.IsBase64(model.Password))
            {
                return Result<TokenInfoResult>.Fail("用户名或密码错误");
            }
            // 密码AES解密
            string password;
            try
            {
                var key = configuration["AES:key"];
                var iv = configuration["AES:iv"];
                password = AESUtils.Decrypt(model.Password, key, iv);
            }
            catch (Exception ex)
            {
                throw new ResultMessageException("密码解析失败，请检查密码格式");
            }
            // 密码验证
            var passwordValid = user != null
                ? passwordHasher.VerifyHashedPassword(user, user.Password, password)
                : PasswordVerificationResult.Failed;
            if (passwordValid != PasswordVerificationResult.Success)
            {
                await RecordFailedLoginAsync(user.Id, clientIP);
                return Result<TokenInfoResult>.Fail("用户名或密码错误");
            }
            //清除登录错误计数
            await ClearLoginRecordsAsync(user.Id, clientIP);
            var refreshToken = jwtService.CreateRefreshToken();
            var refreshExpires = configuration.GetValue<int>("Authentication:RefreshTokenExpiration");
            var expires = configuration.GetValue<int>("Authentication:AccessTokenExpiration");
            memoryCache.Set(refreshToken, user.Id, new MemoryCacheEntryOptions
            {
                AbsoluteExpirationRelativeToNow = TimeSpan.FromSeconds(refreshExpires)
            });
            var claims = new[] {
              new Claim(ClaimTypes.NameIdentifier,user.Id),
              new Claim(ClaimTypes.Name,user.Account),
              new Claim(ClaimTypes.Expired ,expires.ToString())
            };
            var token = jwtService.CreateAccessToken(claims, expires);
            var result = new TokenInfoResult()
            {
                AccessToken = token,
                RefreshToken = refreshToken,
                Expires = expires
            };
            return Result.Success(result);
        }




        /// <summary>
        /// 刷新Token
        /// </summary>
        /// <param name="refreshToken"></param>
        /// <returns></returns>
        public async Task<Result<TokenInfoResult>> RefreshTokenAsync(string refreshToken)
        {
            if (string.IsNullOrWhiteSpace(refreshToken))
            {
                return Result<TokenInfoResult>.Fail("登录信息已过期，请重新登录");
            }
            // 1. 验证并获取用户ID
            memoryCache.TryGetValue(refreshToken, out string userId);
            if (string.IsNullOrEmpty(userId))
            {
                return Result<TokenInfoResult>.Fail("登录信息已过期，请重新登录");
            }
            // 2. 获取用户最新信息
            var user = await sqlSugarClient.Queryable<Tb_User>()
                .Where(r => !r.IsDelete && r.Id == userId)
                .FirstAsync();
            if (user == null)
            {
                return Result<TokenInfoResult>.Fail("获取用户信息失败");
            }
            var newRefreshToken = jwtService.CreateRefreshToken();
            var refreshExpires = configuration.GetValue<int>("Authentication:RefreshTokenExpiration");
            var expires = configuration.GetValue<int>("Authentication:AccessTokenExpiration");
            memoryCache.Set(newRefreshToken, user.Id, new MemoryCacheEntryOptions
            {
                AbsoluteExpirationRelativeToNow = TimeSpan.FromSeconds(refreshExpires)
            });
            var claims = new[] {
              new Claim(ClaimTypes.NameIdentifier,user.Id),
              new Claim(ClaimTypes.Name,user.Account),
              new Claim(ClaimTypes.Expired ,expires.ToString())
            };
            var token = jwtService.CreateAccessToken(claims, expires);
            var result = new TokenInfoResult()
            {
                AccessToken = token,
                RefreshToken = newRefreshToken,
                Expires = expires
            };
            memoryCache.Remove(refreshToken);
            return Result.Success(result);
        }





        #region 登录校验
        //登录限制key
        private string GetLockKey(string userId, string ip) => $"login:locked:{userId}:{ip}";
        //登录错误计数
        private string GetCountKey(string userId, string ip) => $"login:attempts:{userId}:{ip}";
        //短信限制key
        private string GetSMSLockKey(string mobile, string ip) => $"login:sms:{mobile}:{ip}";
        //短信获取计数
        private string GetSMSCountKey(string mobile, string ip) => $"login:smsattempts:{mobile}:{ip}";

        /// <summary>
        /// 滑块校验
        /// </summary>
        /// <param name="captchaData"></param>
        /// <returns></returns>
        private async Task<bool> ValidateSlider(List<int> captchaData, string account)
        {
            // 基础校验：数据有效性验证
            if (captchaData.All(x => x == captchaData.First()))
                return false;

            // 数学特征校验
            var sum = captchaData.Sum();
            var avg = sum * 1.0 / captchaData.Count;
            var stddev = captchaData.Select(v => Math.Pow(v - avg, 2)).Sum() / captchaData.Count;
            if (stddev == 0)
                return false;

            // 用户级防重用校验
            var cacheKey = $"slider:{account}:{string.Join(",", captchaData)}";
            memoryCache.TryGetValue(cacheKey, out string existing);
            if (existing != null) return false;
            // 验证通过后标记为已使用
            memoryCache.Set(cacheKey, "Used", new MemoryCacheEntryOptions
            {
                AbsoluteExpirationRelativeToNow = TimeSpan.FromMinutes(5)
            });
            return true;
        }





        /// <summary>
        /// 检查是否被限制登录
        /// </summary>
        /// <returns></returns>
        private async Task<bool> IsLoginLimitedAsync(string userId, string ip)
        {
            var lockKey = GetLockKey(userId, ip);
            return memoryCache.TryGetValue(lockKey, out _);
        }


        /// <summary>
        /// 清除登录失败记录（登录成功时调用）
        /// </summary>
        /// <returns></returns>
        private async Task ClearLoginRecordsAsync(string userId, string ip)
        {
            var countKey = GetCountKey(userId, ip);
            var lockKey = GetLockKey(userId, ip);
            memoryCache.Remove(countKey);
            memoryCache.Remove(lockKey);
        }

        /// <summary>
        /// 记录登录失败
        /// </summary>
        /// <returns></returns>
        private async Task<long> RecordFailedLoginAsync(string userId, string ip)
        {
            var countKey = GetCountKey(userId, ip);
            var lockKey = GetLockKey(userId, ip);
            var semaphore = _keySemaphores.GetOrAdd(countKey, _ => new SemaphoreSlim(1, 1));
    
            await semaphore.WaitAsync();
            try
            {
            // 获取或创建计数
            if (!memoryCache.TryGetValue(countKey, out long currentCount))
            {
                currentCount = 0;
                var cacheEntryOptions = new MemoryCacheEntryOptions()
                    .SetSlidingExpiration(TimeSpan.FromMinutes(5)); // 5分钟内计数有效
                memoryCache.Set(countKey, currentCount, cacheEntryOptions);
            }
        
            var newCount = currentCount + 1;
            
            // 更新计数
            memoryCache.Set(countKey, newCount);

            // 如果达到5次失败，设置锁定
            if (newCount >= 5)
            {
                var lockOptions = new MemoryCacheEntryOptions()
                    .SetAbsoluteExpiration(TimeSpan.FromMinutes(10));
                memoryCache.Set(lockKey, "locked", lockOptions);
                memoryCache.Remove(countKey);
                _keySemaphores.TryRemove(countKey, out _); // 清理信号量
            }
                return newCount;
            }
            finally
            {
                semaphore.Release();
            }
        }
        #endregion



    }
}
