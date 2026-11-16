using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using Microsoft.AspNetCore.Http.HttpResults;
using SqlSugar;

namespace HNKC.AIBox.Services
{
    public class UserService(IAuthService authService, IIdentityService identityService, ISqlSugarClient sqlSugarClient) : IUserService
    {

        /// <summary>
        /// 用户信息
        /// </summary>
        public async Task<UserInfo> GetUserInfoAsync()
        {
            var userId = identityService.UserId;
            var userInfo = await sqlSugarClient.Queryable<Tb_User>()
            .Where(r => r.Id == userId && !r.IsDelete)
             .Select(r => new UserInfo
             {
                 Id = r.Id,
                 Name = r.Name,
                 Account = r.Account,
                 Mobile = r.Mobile,
                 Remark= r.Remark
             }).FirstAsync();
            if (userInfo == null)
            {
                throw new ResultMessageException("用户不存在或已被删除");
            }
            return userInfo;
        }
    }
}
