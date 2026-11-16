using HNKC.AIBox.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Services
{
    public interface IAuthService
    {

        /// <summary>
        /// 刷新Token
        /// </summary>
        /// <param name="refreshToken"></param>
        /// <returns></returns>
        Task<Result<TokenInfoResult>> RefreshTokenAsync(string refreshToken);


        /// <summary>
        /// 登录
        /// </summary>
        /// <param name="model"></param>
        /// <returns></returns>
        Task<Result<TokenInfoResult>> LoginAsync(LoginRequest model);




    }
}
