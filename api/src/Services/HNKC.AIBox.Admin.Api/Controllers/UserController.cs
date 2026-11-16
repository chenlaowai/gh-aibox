using HNKC.AIBox.Entities;
using HNKC.AIBox.Exceptions;
using HNKC.AIBox.Models;
using HNKC.AIBox.Services;
using HNKC.AIBox.SqlSugars.Extensions;
using HNKC.AIBox.Utils;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Identity;
using Microsoft.AspNetCore.Mvc;
using SqlSugar;

namespace HNKC.AIBox.Admin.Api.Controllers
{
    [Route("api/[controller]/[action]")]
    [ApiController]
    [Authorize]
    public class UserController(IUserService userService, ISqlSugarClient sqlSugarClient, IPasswordHasher<Tb_User> passwordHasher) : ControllerBase
    {

        /// <summary>
        /// 获取用户信息
        /// </summary>
        /// <returns></returns>
        [HttpGet]
        [ProducesResponseType(typeof(UserInfo), StatusCodes.Status200OK)]
        public async Task<IActionResult> UserInfoAsync()
        {
            var result = await userService.GetUserInfoAsync();

            return Ok(result);
        }

        /// <summary>
        /// 用户分页列表
        /// </summary>
        /// <param name="request"></param>
        /// <returns></returns>
        [HttpGet]
        [ProducesResponseType(typeof(Page<UserBaseInfo>), StatusCodes.Status200OK)]
        public async Task<IActionResult> UserPageAsync([FromQuery] UserPageRequest request)
        {
            var result =await  sqlSugarClient.Queryable<Tb_User>()
                .Where(p => !p.IsDelete)
                .WhereIF(!string.IsNullOrEmpty(request.Name), p => p.Name.Contains(request.Name))
                .WhereIF(!string.IsNullOrEmpty(request.Mobile), p => p.Mobile.Contains(request.Mobile))
                .WhereIF(!string.IsNullOrEmpty(request.Account), p => p.Account.Contains(request.Account))
                .Select(p => new UserBaseInfo
                {
                    Account = p.Account,
                    Id = p.Id,
                    Mobile = p.Mobile,
                    Name = p.Name,
                    Remark = p.Remark
                })
                .ToPageResultAsync(request.PageIndex, request.PageSize);
            return Ok(result);

        }

        /// <summary>
        /// 新增用户
        /// </summary>
        /// <param name="request"></param>
        /// <returns></returns>
        [HttpPost]
        public async Task<IActionResult> AddAsync([FromBody]AddUserRequest request)
        {
            var accountVaild = await sqlSugarClient.FirstAsync<Tb_User>(r => r.Account == request.Account && r.IsDelete == false);
            if (accountVaild != null)
            {
                throw new ResultMessageException($"账号【{request.Account}】重复");
            }
            //校验手机号是否重复
            var mobileVaild = await sqlSugarClient.Queryable<Tb_User>().FirstAsync(r => r.Mobile == request.Mobile && r.IsDelete == false);
            if (mobileVaild != null)
            {
                throw new ResultMessageException($"账号【{request.Account}】重复");
            }
            var add = new Tb_User
            {
                Id = PKManager.UUID(),
                Name = request.Name,
                Account = request.Account,
                Password = passwordHasher.HashPassword(new Tb_User(), request.Password),
                Mobile = request.Mobile
            };
            await sqlSugarClient.Insertable(add).ExecuteCommandAsync();
            return Ok();
        }


        /// <summary>
        /// 编辑用户
        /// </summary>
        /// <param name="request"></param>
        /// <returns></returns>
        /// <exception cref="ResultMessageException"></exception>
        [HttpPost]
        public async Task<IActionResult> ModifyAsync([FromBody]ModifyUserRequest request)
        {
            //用户
            var user = await sqlSugarClient.Queryable<Tb_User>()
                .Where(r => !r.IsDelete)
                .Where(r => r.Id == request.Id)
                .FirstAsync();

            if (user.Account != request.Account)
            {
                var accountVaild = await sqlSugarClient.FirstAsync<Tb_User>(r => r.Account == request.Account);
                if (accountVaild != null)
                {
                    throw new ResultMessageException($"账号【{request.Account}】重复");
                }

            }
            if (null == user)
            {
                throw new ResultMessageException($"用户不存在");
            }
            user.Name = request.Name;
            user.Account = request.Account;
            user.Remark = request.Remark;
            user.Mobile = request.Mobile;
            await sqlSugarClient.Updateable(user).ExecuteCommandAsync();
            return Ok();
        }


        /// <summary>
        /// 删除用户
        /// </summary>
        /// <param name="id"></param>
        /// <returns></returns>
        /// <exception cref="ResultMessageException"></exception>
        [HttpDelete("{id}")]
        public async Task<IActionResult> DeleteAsync([FromRoute] string id)
        {
            var data = await sqlSugarClient.FirstAsync<Tb_User>(r => r.Id == id);
            if (null == data)
            {
                throw new ResultMessageException($"用户不存在");
            }
            data.IsDelete = true;
            await sqlSugarClient.Updateable(data).ExecuteCommandAsync();
            return Ok();
        }


        /// <summary>
        /// 重置密码
        /// </summary>
        /// <returns></returns>
        [HttpPost]
        public async Task<IActionResult> ResetPasswordAsync([FromBody] ResetPasswordRequest request)
        {
            var user = await sqlSugarClient.FirstAsync<Tb_User>(r => r.Id == request.UserId);
            if (null == user)
            {
                throw new ResultMessageException($"用户不存在");
            }
            user.Password = passwordHasher.HashPassword(new Tb_User(), request.NewPassword);
            await sqlSugarClient.Updateable(user).ExecuteCommandAsync();
            return Ok();
        }


    }
}
