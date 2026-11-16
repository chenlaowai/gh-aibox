using System.Net.WebSockets;
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Services;

public interface IPreviewService
{
    /// <summary>
    /// 获取预览视频
    /// </summary>
    /// <returns></returns>
    Task GetVideoStreamAsync(WebSocket webSocket, CancellationToken cancellationToken = default);
    
}