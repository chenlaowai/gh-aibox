using HNKC.AIBox.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{
    public interface IUserService
    {

        /// <summary>
        /// 用户信息
        /// </summary>
        Task<UserInfo> GetUserInfoAsync();

    }
}
