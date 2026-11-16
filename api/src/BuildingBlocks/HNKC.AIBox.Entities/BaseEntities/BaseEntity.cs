using SqlSugar;


namespace HNKC.AIBox.Entities
{
    public class BaseEntity
    {
        /// <summary>
        ///  主键
        ///</summary>
        [SugarColumn(IsPrimaryKey = true, ColumnName = "id")]
        public string Id { get; set; }

        /// <summary>
        ///  创建时间
        ///</summary>
        [SugarColumn(ColumnName = "created")]
        public DateTime Created { get; set; }

        /// <summary>
        ///  创建人
        ///</summary>
        [SugarColumn(ColumnName = "create_by")]
        public string? CreateBy { get; set; }

        /// <summary>
        ///  更新时间
        ///</summary>
        [SugarColumn(ColumnName = "modified")]
        public DateTime? Modified { get; set; }

        /// <summary>
        ///  更新人
        ///</summary>
        [SugarColumn(ColumnName = "modified_by")]
        public string? ModifiedBy { get; set; }

        /// <summary>
        ///  是否删除
        ///</summary>
        [SugarColumn(ColumnName = "is_delete")]
        public bool IsDelete { get; set; }
    }
}
