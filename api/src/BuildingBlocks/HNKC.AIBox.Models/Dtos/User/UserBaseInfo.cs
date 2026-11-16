using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class UserBaseInfo
    {

        /// <summary>
        /// 主键ID
        /// </summary>
        public string? Id { get; set; }
        /// <summary>
        ///  账号
        ///</summary>
        public string Account { get; set; }

        /// <summary>
        ///  姓名
        ///</summary>
        public string Name { get; set; }

        /// <summary>
        ///  手机号
        ///</summary>
        public string? Mobile { get; set; }


        /// <summary>
        ///  备注
        ///</summary>
        public string? Remark { get; set; }
    }
}
