
using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Web
{
    public static class ControllerBaseExtensions
    {
        public static ObjectResult OkMessage(this ControllerBase controller, string message)
        {
            return new OkMessageObjectResult(message);
        }
    }
}
