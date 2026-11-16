using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class LoginRequest
    {
        /// <summary>
        /// 账号
        /// </summary>
        [Required(ErrorMessage = "账号 是必填项")]
        public string Account { get; set; }

        /// <summary>
        /// 密码
        /// </summary>
        [Required(ErrorMessage = "密码 是必填项")]
        public string Password { get; set; }

        /// <summary>
        /// 滑块验证码
        /// </summary>
        [Required(ErrorMessage = "滑块验证码 是必填项")]
        public List<int> SliderCaptcha { get; set; }
    }

}
