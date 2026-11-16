using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Web
{

    /// <summary>
    /// 启用缓冲区
    /// </summary>
    /// <param name="next"></param>
    /// <param name="config"></param>
    public class EnableBufferingMiddleware(RequestDelegate next, IConfiguration config)
    {
        private readonly bool ShouldEnableBuffering = bool.Parse(config["LogConf:EnableES"]);
        public async Task InvokeAsync(HttpContext context)
        {
            if (ShouldEnableBuffering)
            {
                context.Request.EnableBuffering();
            }
            await next(context);
        }
    }


    public static class EnableBufferingMiddlewareExtensions
    {
        public static IApplicationBuilder UseEnableBuffering(this IApplicationBuilder app)
        {
            // 依赖注入容器会自动解析中间件构造函数所需的 IConfiguration
            return app.UseMiddleware<EnableBufferingMiddleware>();
        }
    }
}
