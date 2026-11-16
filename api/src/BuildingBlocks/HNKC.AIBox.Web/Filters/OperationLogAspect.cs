using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Utils;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.Filters;
using Microsoft.Extensions.DependencyInjection;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Claims;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Web
{
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Class, Inherited = false, AllowMultiple = false)]
    public class OperationLogAttribute : Attribute, IFilterFactory
    {
        /// <summary>
        /// 操作详情，可另外补充字段
        /// </summary>
        public string _operationDetail { get; }
        public bool IsReusable => false; // 确保每次请求都创建新实例

        public OperationLogAttribute(string operationDetail)
        {
            _operationDetail = operationDetail;
        }

        // 通过 DI 容器创建 Filter 实例
        public IFilterMetadata CreateInstance(IServiceProvider serviceProvider)
        {
            // 从 DI 容器中获取 OperationLogFilter 实例
            var filter = serviceProvider.GetRequiredService<OperationLogFilter>();
            // 动态设置操作信息
            filter.SetOperationInfo(_operationDetail);
            return filter;
        }
    }

    public class OperationLogFilter(IHttpContextAccessor httpContextAccessor) : IAsyncActionFilter
    {

        private string _operationDetail;

        // 动态设置操作信息
        public void SetOperationInfo(string operationDetail)
        {
            _operationDetail = operationDetail;
        }

        public async Task OnActionExecutionAsync(ActionExecutingContext context, ActionExecutionDelegate next)
        {

            // 获取方法信息
            var methodName = context.ActionDescriptor.DisplayName; // 方法名
            var className = context.Controller.GetType().Name; // 类名
            var parameters = context.ActionArguments; // 方法参数
            var startTime = DateTime.Now; // 开始时间

            // 模拟获取当前用户（实际可以从上下文或认证信息中获取）
            var currentUser = GetCurrentUserId(httpContextAccessor);
            var operateSystem = HttpContextClient.GetClientOperatingSystem(httpContextAccessor.HttpContext) + "|" +
            HttpContextClient.GetClientBrowser(httpContextAccessor.HttpContext);
            try
            {
                var resultContext = await next();

                // 获取操作结果
                var operationResult = "成功";
                var outputParam = string.Empty;

                if (resultContext.Exception != null)
                {
                    // 如果发生异常，记录失败日志
                    operationResult = "失败";
                    if (resultContext.Exception is ResultMessageException resultMessageException)
                    {
                        // 如果是 ResultMessageException，记录异常消息
                        outputParam = resultMessageException.Message;
                    }
                    else
                    {
                        // 如果是其他异常，记录异常信息
                        outputParam = resultContext.Exception.Message;
                    }
                }
                else if (resultContext.Result != null)
                {
                    if (resultContext.Result is ObjectResult objectResult)
                    {
                        // 如果是 ObjectResult，序列化返回值
                        outputParam = JsonConvert.SerializeObject(objectResult.Value);
                    }
                    else if (resultContext.Result is StatusCodeResult statusCodeResult)
                    {
                        // 如果是状态码结果，记录状态码
                        outputParam = $"StatusCode: {statusCodeResult.StatusCode}";
                        if (statusCodeResult.StatusCode >= 400)
                        {
                            operationResult = "失败";
                        }
                    }
                }

                // 根据方法名生成日志信息
                await GenerateLogMessage(methodName, className, _operationDetail, currentUser, operateSystem, startTime, parameters, operationResult, outputParam);


            }
            catch (ResultMessageException ex)
            {
                // 捕获 ResultMessageException 并记录失败日志
                await GenerateLogMessage(methodName, className, _operationDetail, currentUser, operateSystem, startTime, parameters, "失败", ex.Message);
                throw; // 重新抛出异常，确保全局异常处理仍然生效
            }
            catch (Exception ex)
            {
                // 记录其他异常日志
                await GenerateLogMessage(methodName, className, _operationDetail, currentUser, operateSystem, startTime, parameters, "失败", ex.Message);
                throw; // 重新抛出异常，确保全局异常处理仍然生效
            }
        }

        /// <summary>
        /// 模拟获取当前用户ID
        /// </summary>
        private string GetCurrentUserId(IHttpContextAccessor httpContextAccessor)
        {
            // 实际可以从 HttpContext.User 中获取用户信息
            var userIdClaim = httpContextAccessor.HttpContext?.User?.Claims
        .FirstOrDefault(c => c.Type == ClaimTypes.NameIdentifier);

            return userIdClaim?.Value ?? "Anonymous";
        }



        private async Task GenerateLogMessage(string methodName, string className, string operationDetail, string currentUser,
            string operateSystem, DateTime startTime, IDictionary<string, object> parameters, string operationResult, string outputParam)
        {
            //添加操作记录到日志，或到数据库
           
        }
    }
}
