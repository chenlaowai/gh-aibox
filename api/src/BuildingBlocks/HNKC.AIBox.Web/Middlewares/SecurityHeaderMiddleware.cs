using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;

namespace HNKC.AIBox.Web
{
    public class SecurityHeaderMiddleware
    {
        private readonly RequestDelegate _next;
        public SecurityHeaderMiddleware(RequestDelegate next) => _next = next;

        public async Task InvokeAsync(HttpContext context)
        {
            context.Response.Headers.Add("X-Content-Type-Options", "nosniff");
            context.Response.Headers.Add("X-Frame-Options", "SAMEORIGIN");
            context.Response.Headers.Add("X-Download-Options", "noopen");
            context.Response.Headers.Add("X-Permitted-Cross-Domain-Policies", "none");
            context.Response.Headers.Add("Referrer-Policy", "strict-origin-when-cross-origin");
            context.Response.Headers.Add("Content-Security-Policy", "default-src 'self'");
            context.Response.Headers.Add("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
            context.Response.Headers.Add("X-XSS-Protection", "1; mode=block");
            await _next(context);
        }
    }

    public static class SecurityHeaderExtensions
    {
        /// <summary>
        /// 添加安全相关请求头
        /// </summary>
        /// <param name="app"></param>
        /// <returns></returns>
        public static IApplicationBuilder UseSecurityHeader(this IApplicationBuilder app)
        {
            return app.UseMiddleware<SecurityHeaderMiddleware>();
        }
    }
}
