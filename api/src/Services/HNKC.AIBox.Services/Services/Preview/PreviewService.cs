using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using HNKC.AIBox.Entities;
using HNKC.AIBox.Models;
using SqlSugar;

namespace HNKC.AIBox.Services;

public class PreviewService(ISqlSugarClient sqlSugarClient,ImageTest imageTest) : IPreviewService
{
    private string _currentTaskId = "all";

    public async Task GetVideoStreamAsync(WebSocket webSocket, CancellationToken cancellationToken = default)
    {
        var frameDatas = await sqlSugarClient.Queryable<Tb_Task>()
            .Where(r => !r.IsDelete)
            .ToListAsync();

        try
        {
            // 独立的接收任务
            var receiveTask = Task.Run(async () =>
            {
                while (!cancellationToken.IsCancellationRequested && webSocket.State == WebSocketState.Open)
                {
                    try
                    {
                        var receiveBuffer = new byte[1024];
                        var result =
                            await webSocket.ReceiveAsync(new ArraySegment<byte>(receiveBuffer), cancellationToken);
                        if (result.MessageType == WebSocketMessageType.Text)
                        {
                            var receivedMessage = Encoding.UTF8.GetString(receiveBuffer, 0, result.Count);
                            // 处理接收到的消息
                            _currentTaskId = receivedMessage.Trim();
                            Console.WriteLine(receivedMessage);
                        }
                        else if (result.MessageType == WebSocketMessageType.Close)
                        {
                            await webSocket.CloseAsync(WebSocketCloseStatus.NormalClosure,
                                "Connection closed by client", cancellationToken);
                            break;
                        }
                    }
                    catch (WebSocketException)
                    {
                        break;
                    }
                }
            }, cancellationToken);

            // 发送逻辑独立运行
            while (!cancellationToken.IsCancellationRequested && webSocket.State == WebSocketState.Open)
            {
                var cameraFrameData = new CameraFrameData();
                
                    cameraFrameData.ImageData = await imageTest.getImage(_currentTaskId);
                    
                var json = JsonSerializer.Serialize(cameraFrameData);
                var sendBuffer = Encoding.UTF8.GetBytes(json);
                await webSocket.SendAsync(
                    new ArraySegment<byte>(sendBuffer),
                    WebSocketMessageType.Text,
                    true,
                    cancellationToken
                );

                await Task.Delay(1000, cancellationToken);
            }

            await receiveTask;
        }
        catch (WebSocketException)
        {
        }
    }

}