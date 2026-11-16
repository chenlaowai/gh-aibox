using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class AddUserRequest
    {
        /// <summary>
        ///  账号
        ///</summary>
        [Required(ErrorMessage = "账号 是必填项")]
        public string Account { get; set; }

        /// <summary>
        ///  密码
        ///</summary>
        public string Password { get; set; }

        /// <summary>
        /// 姓名
        ///</summary>
        public string Name { get; set; }

        /// <summary>
        ///  手机号
        ///</summary>
        public string Mobile { get; set; }

        /// <summary>
        ///  备注
        ///</summary>
        public string Remark { get; set; }
    }
}
