using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HNKC.AIBox.Models
{
    public class DictResultDto
    {
        /// <summary>
        /// 名称
        /// </summary>
        public string Desc { get; set; }

        /// <summary>
        /// 字典值
        /// </summary>
        public List<DictDto> Data { get; set; }
    }
}
