
using System.Net.Http;
using System.Text;

namespace HNKC.BPM.Base.Sdk
{
    public class HttpClientHelper(IHttpClientFactory httpClientFactory)
    {
        private readonly HttpClient httpClient = httpClientFactory.CreateClient();

        /// <summary>
        /// 发送Http请求
        /// </summary>
        /// <param name="url">链接</param>
        /// <param name="method">HTTP方法</param>
        /// <param name="encoding">字符格式</param>
        /// <param name="contentType">请求内容类型，默认application/json"</param>
        /// <param name="content">内容</param>
        /// <param name="headers">请求头</param>
        /// <returns></returns>
        public async Task<string> SendAsync(string url, HttpMethod method, Encoding encoding, string contentType = "application/json", string content = null, Dictionary<string, string> headers = null, int timeoutSeconds = 100)
        {
            using (var request = new HttpRequestMessage(method, url))
            {
                if (content != null)
                {
                    request.Content = new StringContent(content, encoding, contentType);

                }

                if (headers != null)
                {
                    foreach (var header in headers)
                    {
                        if (!request.Headers.TryAddWithoutValidation(header.Key, header.Value))
                        {
                            throw new InvalidOperationException($"无法添加请求头: {header.Key}");
                        }
                    }
                }
                
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds));
                using (var response = await httpClient.SendAsync(request, cts.Token))
                {
                    if (response.IsSuccessStatusCode)
                    {
                        return await response.Content.ReadAsStringAsync();
                    }
                    else
                    {
                        string errorMessage = response == null ? $"请求{url}返回空" : await response.Content.ReadAsStringAsync();
                        throw new HttpRequestException($"请求失败: {errorMessage}");
                    }
                }
            }

        }


        /// <summary>
        /// 发送Http Post请求
        /// </summary>
        /// <param name="url">链接</param>
        /// <param name="contentType">请求内容类型，默认application/json"</param>
        /// <param name="content">内容</param>
        /// <param name="headers">请求头</param>
        /// <returns></returns>
        public async Task<string> PostAsync(string url, Dictionary<string, string> headers = null, string content = null, string contentType = "application/json", int timeoutSeconds = 100)
        {


            using (var request = new HttpRequestMessage(HttpMethod.Post, url))
            {
                if (content != null)
                {
                    request.Content = new StringContent(content, Encoding.UTF8, contentType);
                }
                if (headers != null)
                {
                    foreach (var header in headers)
                    {
                        if (!request.Headers.TryAddWithoutValidation(header.Key, header.Value))
                        {
                            throw new InvalidOperationException($"无法添加请求头: {header.Key}");
                        }
                    }
                }
                
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds));
                using (var response = await httpClient.SendAsync(request, cts.Token))
                {
                    if (response.IsSuccessStatusCode)
                    {
                        return await response.Content.ReadAsStringAsync();
                    }
                    else
                    {
                        string errorMessage = response == null ? $"请求{url}返回空" : await response.Content.ReadAsStringAsync();
                        throw new HttpRequestException($"请求失败: {errorMessage}");
                    }
                }
            }

        }


        /// <summary>
        /// 发送Http Get请求
        /// </summary>
        /// <param name="url">链接</param>
        /// <param name="contentType">请求内容类型，默认application/json"</param>
        /// <param name="content">内容</param>
        /// <param name="headers">请求头</param>
        /// <returns></returns>
        public async Task<string> GetAsync(string url, Dictionary<string, string> headers = null, string contentType = "application/json", int timeoutSeconds = 100)
        {

            using (var request = new HttpRequestMessage(HttpMethod.Get, url))
            {

                if (headers != null)
                {
                    foreach (var header in headers)
                    {
                        if (!request.Headers.TryAddWithoutValidation(header.Key, header.Value))
                        {
                            throw new InvalidOperationException($"无法添加请求头: {header.Key}");
                        }
                    }
                }
                
                using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(timeoutSeconds));
                using (var response = await httpClient.SendAsync(request, cts.Token))
                {
                    if (response.IsSuccessStatusCode)
                    {
                        return await response.Content.ReadAsStringAsync();
                    }
                    else
                    {
                        string errorMessage = response == null ? $"请求{url}返回空" : await response.Content.ReadAsStringAsync();
                        throw new HttpRequestException($"请求失败: {errorMessage}");
                    }
                }
            }

        }

    }
}