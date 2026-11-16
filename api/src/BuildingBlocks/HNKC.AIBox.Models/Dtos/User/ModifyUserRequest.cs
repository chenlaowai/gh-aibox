using System;
using System.Collections.Generic;
using System.ComponentModel.DataAnnotations;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class ModifyUserRequest
    {
        /// <summary>
        /// Id
        /// </summary>
        [Required(ErrorMessage = "Id 是必填项")]
        public string Id { get; set; }

        /// <summary>
        ///  姓名
        ///</summary>
        [Required(ErrorMessage = "姓名 是必填项")]
        public string Name { get; set; }

        [Required(ErrorMessage = "账号是必填项")]
        public string Account { get; set; }

        /// <summary>
        ///  所属组织；人员所属组织 和人员组织查询内容无关
        ///</summary>
        public string? OrgId { get; set; }

        /// <summary>
        ///  备注
        ///</summary>
        public string? Remark { get; set; }

        /// <summary>
        /// 手机号
        /// </summary>
        public string Mobile { get; set; }

    }
}
