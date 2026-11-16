using System;
using SqlSugar;
using System.Linq;
using System.Text;

namespace HNKC.AIBox.Entities
{
    /// <summary>
    ///  用户表
    /// </summary>
    [SugarTable("tb_user", "用户表")]
    public class Tb_User : BaseEntity
    {
        /// <summary>
        ///  姓名
        ///</summary>
        [SugarColumn(ColumnName = "name")]
        public string Name { get; set; }

        /// <summary>
        ///  账号
        ///</summary>
        [SugarColumn(ColumnName = "account")]
        public string Account { get; set; }

        /// <summary>
        ///  密码
        ///</summary>
        [SugarColumn(ColumnName = "password")]
        public string Password { get; set; }

        /// <summary>
        ///  手机号
        ///</summary>
        [SugarColumn(ColumnName = "mobile")]
        public string? Mobile { get; set; }



        /// <summary>
        ///  描述备注
        ///</summary>
        [SugarColumn(ColumnName = "remark")]
        public string? Remark { get; set; }



    }
}