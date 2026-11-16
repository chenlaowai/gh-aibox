using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{
    public interface IIdentityService
    {

        /// <summary>
        /// 获取用户ID
        /// </summary>
        string UserId { get; }

        /// <summary>
        /// 获取用户Token
        /// </summary>
        string Token { get; }
    }
}
