using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.Filters;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using System.Net;
using System.Security.Claims;
using System.Text;

namespace HNKC.AIBox.Web
{
    public class HttpGlobalExceptionFilter(ILogger<HttpGlobalExceptionFilter> logger, IConfiguration configuration) : IExceptionFilter
    {

        private readonly bool EnableES = configuration.GetValue<bool>("LogConf:EnableES");
        public void OnException(ExceptionContext context)
        {
            if (EnableES)
            {
                LogToElasticsearch(context);
            }
            else
            {   //控制台输出错误信息
                logger.LogError(JsonConvert.SerializeObject(context.Exception));
            }
            switch (context.Exception)
            {
                case ResultMessageException:
                    context.Result = new OkObjectResult(Result.Fail("服务器错误,请重试."));
                    context.HttpContext.Response.StatusCode = (int)HttpStatusCode.OK;
                    context.ExceptionHandled = true;
                    break;
                case UnauthorizedException:
                    context.Result = new UnauthorizedObjectResult(Result.NoAuth(context.Exception.Message));
                    context.HttpContext.Response.StatusCode = (int)HttpStatusCode.Unauthorized;
                    context.ExceptionHandled = true;
                    break;
                default:

                    context.Result = new InternalServerErrorObjectResult(Result.Fail("服务器错误,请重试."));
                    context.HttpContext.Response.StatusCode = (int)HttpStatusCode.InternalServerError;
                    return;
            }
        }

        /// <summary>
        /// 记录日志到ElasticSearch
        /// </summary>
        /// <param name="context"></param>
        /// <param name="logger"></param>
        private  void LogToElasticsearch(ExceptionContext context)
        {
            var body = SafelyReadRequestBody(context);
            var queryParams = context.HttpContext.Request.QueryString.HasValue
          ? context.HttpContext.Request.QueryString.Value
          : string.Empty;
            var User = new
            {
                Id = context?.HttpContext?.User?.FindFirst(ClaimTypes.NameIdentifier)?.Value,
                Name = context.HttpContext.User.Identity?.Name
            };
            logger.LogError("{exception}{message}{query}{body}{user}", context.Exception, context.Exception.Message, queryParams, body, User);
        }
        /// <summary>
        /// 获取请求Body的内容
        /// </summary>
        /// <param name="context"></param>
        /// <returns></returns>
        private  string SafelyReadRequestBody(ExceptionContext context)
        {
            try
            {
                var request = context.HttpContext.Request;

                // 跳过文件上传等大请求体
                if (request.ContentType?.Contains("multipart/form-data") == true ||
                    request.ContentLength > 1024 * 1024) // 1MB以上不记录
                {
                    return "请求过大，已跳过";
                }
                if (request.Body != null && request.Body.CanRead &&
                    request.ContentLength.GetValueOrDefault() > 0)
                {
                    request.EnableBuffering();
                    request.Body.Position = 0;

                    using (var reader = new StreamReader(request.Body, Encoding.UTF8, true, 1024, true))
                    {
                        var body = reader.ReadToEndAsync().GetAwaiter().GetResult();
                        request.Body.Position = 0; // 重置流位置
                        return body;
                    }
                }
            }
            catch (Exception ex)
            {
                return $"获取请求报文出错: {ex.Message}";
            }
            return string.Empty;
        }
    }
}
