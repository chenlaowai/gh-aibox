using Microsoft.AspNetCore.Http;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Utils
{
    public class HttpContextClient
    {

        public static string GetClientOperatingSystem(HttpContext httpContext)
        {
            var userAgent = httpContext.Request.Headers["User-Agent"].ToString();

            if (userAgent.Contains("Windows NT"))
            {
                return "Windows";
            }
            else if (userAgent.Contains("Macintosh") || userAgent.Contains("Mac OS X"))
            {
                return "macOS";
            }
            else if (userAgent.Contains("Linux"))
            {
                return "Linux";
            }
            else if (userAgent.Contains("Android"))
            {
                return "Android";
            }
            else if (userAgent.Contains("iPhone") || userAgent.Contains("iPad"))
            {
                return "iOS";
            }
            else
            {
                return "Unknown";
            }
        }



        /// <summary>
        /// 获取客户端浏览器信息
        /// </summary>
        public static string GetClientBrowser(HttpContext httpContext)
        {
            var userAgent = httpContext.Request.Headers["User-Agent"].ToString();

            if (userAgent.Contains("Edg"))
            {
                return "Microsoft Edge";
            }
            else if (userAgent.Contains("Chrome"))
            {
                return "Google Chrome";
            }
            else if (userAgent.Contains("Firefox"))
            {
                return "Mozilla Firefox";
            }
            else if (userAgent.Contains("Safari"))
            {
                return "Apple Safari";
            }
            else if (userAgent.Contains("Opera"))
            {
                return "Opera";
            }
            else if (userAgent.Contains("MSIE") || userAgent.Contains("Trident"))
            {
                return "Internet Explorer";
            }
            else
            {
                return "Unknown";
            }
        }

    }
}
