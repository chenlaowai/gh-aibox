
using HNKC.AIBox.Services;
using HNKC.AIBox.Web;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Admin.Api.Controllers;

/// <summary>
/// 预览管理
/// </summary>
[Route("api/[controller]/[action]")]
[ApiController]
public class PreviewController(IPreviewService previewService,IJwtService jwtService) : ControllerBase
{
    /// <summary>
    /// 获取预览视频（WebSocket）
    /// </summary>
    /// <returns></returns>
    [HttpGet]
    public async Task GetVideoAsync([FromQuery] string token,CancellationToken cancellationToken = default)
    {
        jwtService.ValidateToken(token);
        if (HttpContext.WebSockets.IsWebSocketRequest)
        {
            using var webSocket = await HttpContext.WebSockets.AcceptWebSocketAsync();
            await previewService.GetVideoStreamAsync(webSocket, cancellationToken);
        }
        else
        {
            HttpContext.Response.StatusCode = StatusCodes.Status400BadRequest;
        }
    }
}