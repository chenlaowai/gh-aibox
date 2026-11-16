using Microsoft.AspNetCore.Mvc;

namespace HNKC.AIBox.Models
{
    /// <summary>
    /// 
    /// </summary>
    public class Result:ActionResult
    {
        public int Code { get; set; }
        public string? Message { get; set; }
        public object? Data { get; set; }
        public bool IsSuccess { get => Code == 200; }
        /// <summary>
        /// 
        /// </summary>
        public Result() { }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="code"></param>
        /// <param name="message"></param>
        public Result(int code, string? message)
        {
            Code = code;
            Message = message;
        }

        /// <summary>
        /// 返回成功
        /// </summary>
        /// <returns></returns>
        public static Result Success(string? message = "success")
        {
            var r = new Result
            {
                Code = 200,
                Message = message,
                Data = null
            };
            return r;
        }

        /// <summary>
        /// 返回成功,带数据返回
        /// </summary>
        /// <param name="t">数据</param>
        /// <param name="message">提示信息</param>
        /// <returns></returns>
        public static Result Success(object data, string? message = null)
        {
            return new Result
            {
                Data = data,
                Code = 200,
                Message = message
            };
        }

        /// <summary>
        /// 返回成功,带数据返回
        /// </summary>
        /// <param name="t">数据</param>
        /// <param name="message">提示信息</param>
        /// <returns></returns>
        public static Result<T> Success<T>(T data, string? message = null)
        {
            return new Result<T>(200, data, message ?? "success");
        }

        /// <summary>
        /// 错误
        /// </summary>
        /// <returns></returns>
        public static Result Fail(string? message)
        {
            return new Result
            {
                Code = 500,
                Message = message,
                Data = null
            };
        }

        /// <summary>
        /// 自定义错误码
        /// </summary>
        /// <param name="message"></param>
        /// <param name="code"></param>
        /// <returns></returns>
        public static Result Fail(string? message, int code)
        {
            return new Result
            {
                Code = code,
                Message = message,
                Data = null
            };
        }

        /// <summary>
        /// 未认证
        /// </summary>
        /// <returns></returns>
        public static Result NoAuth(string? message = null)
        {
            var r = new Result
            {
                Code = 401,
                Message = message,
                Data = null
            };
            if (string.IsNullOrEmpty(message))
                r.Message = "未通过认证";
            return r;
        }
    }

    /// <summary>
    /// 带数据的泛型结果
    /// </summary>
    /// <typeparam name="T"></typeparam>
    public class Result<T> : Result
    {
        public new T? Data { get; set; }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="code"></param>
        /// <param name="data"></param>
        /// <param name="message"></param>
        public Result(int code, T? data, string? message)
            : base(code, message)
        {
            Data = data;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="data"></param>
        /// <returns></returns>
        public static Result<T> Success(T data) => new(200, data, "success");
        /// <summary>
        /// 
        /// </summary>
        /// <param name="message"></param>
        /// <returns></returns>
        public static new Result<T> Fail(string? message) => new(500, default, message);
    }

    /// <summary>
    /// 分页返回结果格式
    /// </summary>
    public class Page<T>
    {
        private int _PageIndex;
        /// <summary>
        /// 页码
        /// </summary>
        public int PageIndex
        {
            get
            {
                return _PageIndex;
            }
            set
            {
                if (value <= 0)
                    _PageIndex = 1;
                else
                    _PageIndex = value;
            }
        }

        private int _PageSize;
        /// <summary>
        /// 页大小
        /// </summary>
        public int PageSize
        {
            get
            {
                return _PageSize;
            }
            set
            {
                if (value > 100)
                    _PageSize = 100;
                else if (value <= 0)
                    _PageSize = 5;
                else
                    _PageSize = value;
            }
        }
        /// <summary>
        /// 
        /// </summary>
        public long TotalCount { get; set; }
        /// <summary>
        /// 
        /// </summary>
        public IEnumerable<T>? List { get; set; }
    }

    /// <summary>
    /// 
    /// </summary>
    public static class ResultExtension
    {
        public static IQueryable<TSource> _Page<TSource>(this IQueryable<TSource> queryable, int pageIndex, int pageSize)
        {
            if (pageIndex <= 0)
                pageIndex = 1;

            if (pageSize > 100)
                pageSize = 100;

            if (pageSize <= 0)
                pageSize = 5;

            return queryable.Skip((pageIndex - 1) * pageSize).Take(pageSize);
        }

        public static IEnumerable<TSource> _Page<TSource>(this IEnumerable<TSource> enu, int pageIndex, int pageSize)
        {
            if (pageIndex <= 0)
                pageIndex = 1;

            if (pageSize > 100)
                pageSize = 100;

            if (pageSize <= 0)
                pageSize = 5;

            return enu.Skip((pageIndex - 1) * pageSize).Take(pageSize);
        }
    }
}
