using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class IdTokenResult: TokenInfoResult
    {
        /// <summary>
        /// 交建通登录门户返回token
        /// </summary>
        public string IdToken { get; set; }
    }
}
