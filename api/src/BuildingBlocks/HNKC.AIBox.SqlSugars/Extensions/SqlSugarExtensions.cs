using HNKC.AIBox.Models;
using HNKC.AIBox.SqlSugars.UnitOfTransaction;
using HNKC.AIBox.Utils;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using SqlSugar;
using System.Linq.Expressions;
using System.Security.Claims;

namespace HNKC.AIBox.SqlSugars.Extensions
{
    public static class SqlSugarExtensions
    {
        /// <summary>
        /// 注入SqlSugar
        /// </summary>
        /// <param name="services"></param>
        /// <param name="conn"></param>
        public static void AddSqlSugarContext(this IServiceCollection services, string conn)
        {
            services.AddScoped<IUnitOfWork, SqlSugarUnitOfWork>();
            services.AddSingleton<IHttpContextAccessor, HttpContextAccessor>();
            //注册上下文：AOP里面可以获取IOC对象，如果有现成框架比如Furion可以不写这一行
            //services.AddHttpContextAccessor();
            //注册SqlSugar用AddScoped
            services.AddScoped<ISqlSugarClient>(s =>
            {
                //Scoped用SqlSugarClient 
                SqlSugarClient sqlSugar = new SqlSugarClient(new ConnectionConfig()
                {
                    DbType = DbType.Sqlite,
                    ConnectionString = conn,
                    IsAutoCloseConnection = true,
                },
               db =>
               {
                   db.Aop.OnLogExecuting = (sql, pars) =>
                   {
                       switch (Environment.GetEnvironmentVariable("ASPNETCORE_ENVIRONMENT"))
                       {
                           case "Development":
                               {
                                   //Console.WriteLine(sql + "\r\n" + JsonSerializer.Serialize(pars.ToDictionary(it => it.ParameterName, it => it.Value)));
                                   //Console.WriteLine();
                                   break;
                               }
                           case "Production": break;

                       }
                   };
                   db.Aop.DataExecuting = (value, entityInfo) =>
                   {
                       switch (entityInfo.OperationType)
                       {
                           case DataFilterType.InsertByObject:
                               if (entityInfo.PropertyName == "Id" && (value == null || (value is string s && string.IsNullOrWhiteSpace(s))))
                               {
                                   entityInfo.SetValue(PKManager.UUID());
                               }
                               else if (entityInfo.PropertyName == "Created")//创建时间
                               {
                                   entityInfo.SetValue(DateTime.Now);
                               }
                               else if (entityInfo.PropertyName == "CreateBy")//创建人
                               {
                                   var serviceBuilder = services.BuildServiceProvider();
                                   var _context = serviceBuilder.GetService<IHttpContextAccessor>();
                                   entityInfo.SetValue(_context?.HttpContext?.User?.FindFirst(ClaimTypes.NameIdentifier)?.Value ?? null);
                               }
                               break;
                           case DataFilterType.UpdateByObject:
                               if (entityInfo.PropertyName == "Modified")//修改时间
                               {
                                   entityInfo.SetValue(DateTime.Now);
                               }
                               else if (entityInfo.PropertyName == "ModifiedBy")//修改人
                               {
                                   var serviceBuilder = services.BuildServiceProvider();
                                   var _context = serviceBuilder.GetService<IHttpContextAccessor>();
                                   entityInfo.SetValue(_context?.HttpContext?.User?.FindFirst(ClaimTypes.NameIdentifier)?.Value ?? null);
                               }
                               break;
                       }
                   };
               });
                return sqlSugar;
            });
        }

        /// <summary>
        /// SqlSugar分页查询 同步
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="sugarQueryable"></param>
        /// <param name="pageIndex"></param>
        /// <param name="pageSize"></param>
        /// <param name="totalNumber"></param>
        /// <returns></returns>
        public static Page<T> ToPageResult<T>(this ISugarQueryable<T> sugarQueryable, int pageIndex, int pageSize)
        {
            int total = 0;
            var data = sugarQueryable.ToPageList(pageIndex, pageSize, ref total);
            return new Page<T>
            {
                PageIndex = pageIndex,
                PageSize = pageSize,
                TotalCount = total,
                List = data
            };
        }

        /// <summary>
        /// SqlSugar分页查询 异步
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="sugarQueryable"></param>
        /// <param name="pageIndex"></param>
        /// <param name="pageSize"></param>
        /// <param name="totalNumber"></param>
        /// <returns></returns>
        public static async Task<Page<T>> ToPageResultAsync<T>(this ISugarQueryable<T> sugarQueryable, int pageIndex, int pageSize)
        {
            // 使用RefAsync<int>接收总数
            RefAsync<int> total = 0;
            var data = await sugarQueryable.ToPageListAsync(pageIndex, pageSize, total);
            return new Page<T>
            {
                PageIndex = pageIndex,
                PageSize = pageSize,
                TotalCount = total,
                List = data
            };
        }

        public static async Task<Page<T>> ToPageResultAsync<T>(this ISugarQueryable<T> sugarQueryable, PageRequest input)
        {
            RefAsync<int> totalNumber = 0;

            var data = await sugarQueryable.ToPageListAsync(input.PageIndex, input.PageSize, totalNumber);
            return new Page<T>
            {
                PageIndex = input.PageIndex,
                PageSize = input.PageSize,
                TotalCount = totalNumber.Value,
                List = data
            };
        }

        /// <summary>
        /// 查询第一条记录
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="client"></param>
        /// <param name="funcWhere"></param>
        /// <returns></returns>
        public static async Task<T> FirstAsync<T>(this ISqlSugarClient client, Expression<Func<T, bool>> funcWhere)
        {
            return await client.Queryable<T>().Where(funcWhere).FirstAsync();
        }

    }
}
