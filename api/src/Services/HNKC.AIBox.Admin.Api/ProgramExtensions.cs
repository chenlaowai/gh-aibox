using HNKC.AIBox.Admin.Api.Swagger;
using HNKC.AIBox.AITask;
using HNKC.AIBox.Entities;
using HNKC.AIBox.Services.Admin.Api;
using HNKC.AIBox.SqlSugars.Extensions;
using HNKC.AIBox.SqlSugars.UnitOfTransaction;
using HNKC.AIBox.Web;
using HNKC.BPM.Base.Sdk;
using HNKCCH.Base.Nuget.Files.Common.Storage;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.AspNetCore.Identity;
using Microsoft.IdentityModel.Tokens;
using Microsoft.OpenApi.Models;
using Serilog;
using Serilog.Events;
using Serilog.Sinks.Elasticsearch;
using System.Text;

namespace HNKC.AIBox.Services.Admin.Api
{
    public static class ProgramExtensions
    {

        /// <summary>
        /// 扩展方法，总入口
        /// </summary>
        /// <param name="builder"></param>
        public static void AddProgramExtensions(this WebApplicationBuilder builder)
        {
            builder.Configuration
                .AddJsonFile("appsettings.json")
                .AddJsonFile(
                    $"appsettings.{(Environment.GetEnvironmentVariable("DOTNET_RUNNING_IN_CONTAINER") == "true" ? "Docker" : "Development")}.json",
                    optional: true);
            builder.AddControllers();
            builder.AddJwtService();
            builder.AddSqlSugar();
            builder.AddMemoryCacheServices();
            builder.AddAuthentication();
            builder.AddCors();
            builder.AddApplicationServices();
            builder.AddSwagger();
            builder.AddSerilogEsClient();
        }


        /// <summary>
        /// 注入服务
        /// </summary>
        /// <param name="builder"></param>
        public static void AddApplicationServices(this WebApplicationBuilder builder)
        {
            builder.Services.AddHttpClient();
            builder.Services.AddSingleton<IHttpContextAccessor, HttpContextAccessor>();
            builder.Services.AddScoped<IPasswordHasher<Tb_User>, PasswordHasher<Tb_User>>();
            builder.Services.AddScoped<IJwtService, JwtService>();
            builder.Services.AddTransient<IIdentityService, IdentityService>();
            builder.Services.AddTransient<ISystemService, SystemService>();
            builder.Services.AddTransient<IUserService, UserService>();
            builder.Services.AddTransient<IAuthService, AuthService>();
            builder.Services.AddScoped<IFileUploadService, FileUploadService>();
            builder.Services.AddSingleton<IDeviceService, DeviceService>();
            builder.Services.AddTransient<ICpuMetricsService, CpuMetricsService>();
            builder.Services.AddScoped<ITaskService, TaskService>();
            builder.Services.AddScoped<IPreviewService, PreviewService>();
            builder.Services.AddTransient<IStorage, Storage>();
            builder.Services.AddTransient<IFileService, FileService>();
            builder.Services.AddTransient<ImageTest>();
            builder.Services.AddScoped<IAlarmService,AlarmService>();
            builder.Services.AddScoped<IAITaskService,AITaskService>();
            builder.Services.AddScoped<HttpClientHelper>();
            builder.Services.AddScoped<OperationLogFilter>();
            builder.Services.AddHostedService<AlarmReportingWorker>();

        }


        /// <summary>
        /// 注入Controllers
        /// </summary>
        /// <param name="builder"></param>
        private static void AddControllers(this WebApplicationBuilder builder)
        {
            builder.Services.AddControllers(options =>
            {
                options.Filters.Add<HttpGlobalExceptionFilter>();
                options.Filters.Add(typeof(ValidateModelFilter));
                options.Filters.Add(new APIResultFilter());
                options.Filters.Add(typeof(TransactionalFilter));
            }).AddJsonOptions(jsonOptions =>
            {
                jsonOptions.JsonSerializerOptions.Converters.Add(new DatetimeJsonConverterExtension());
            });
        }


        /// <summary>
        /// 添加swagger
        /// </summary>
        /// <param name="builder"></param>
        private static void AddSwagger(this WebApplicationBuilder builder)
        {
            //Swagger
            builder.Services.AddSwaggerGen(options =>
            {
                options.AddSecurityDefinition("Bearer", new OpenApiSecurityScheme()
                {
                    Description = "请输入token格式为Bearer xxxxxx(中间必须有空格)",
                    Name = "Authorization",
                    In = ParameterLocation.Header,
                    BearerFormat = "JWT",
                    Scheme = "Bearer"
                });
                options.AddSecurityRequirement(new OpenApiSecurityRequirement{
                {
                     new OpenApiSecurityScheme()
                     {
                          Reference=new OpenApiReference
                          {
                              Type=ReferenceType.SecurityScheme,
                              Id="Bearer"
                          }
                     },new string[]{ }
                    }
                });

                options.SwaggerDoc("v1", new OpenApiInfo { Title = $"AI盒子", Version = "v1" });

                // 为 Swagger JSON and UI设置xml文档注释路径
                options.IncludeXmlComments(Path.Combine(AppContext.BaseDirectory, "HNKC.AIBox.Admin.Api.xml"));
                options.IncludeXmlComments(Path.Combine(AppContext.BaseDirectory, "HNKC.AIBox.Models.xml"));
                options.IncludeXmlComments(Path.Combine(AppContext.BaseDirectory, "HNKC.AIBox.Enums.xml"));

                options.SchemaFilter<EnumSchemaFilter>();
            });
        }



        /// <summary>
        /// JwtService
        /// </summary>
        /// <param name="builder"></param>
        private static void AddJwtService(this WebApplicationBuilder builder)
        {
            builder.Services.AddJwtService(options =>
            {
                options.SecurityKey = builder.Configuration["Authentication:SecurityKey"];
                options.Issuer = builder.Configuration["Authentication:Issuer"];
                options.Audience = builder.Configuration["Authentication:Audience"];
            });
        }

        /// <summary>
        /// 注入SqlSugar
        /// </summary>
        /// <param name="builder"></param>
        private static void AddSqlSugar(this WebApplicationBuilder builder)
        {
            builder.Services.AddSqlSugarContext(builder.Configuration.GetConnectionString("Sqlite"));
        }


        /// <summary>
        /// MemoryCache
        /// </summary>
        /// <param name="builder"></param>
        private static void AddMemoryCacheServices(this WebApplicationBuilder builder)
        {
            builder.Services.AddMemoryCache();
        }



        /// <summary>
        /// 认证
        /// </summary>
        /// <param name="builder"></param>
        public static void AddAuthentication(this WebApplicationBuilder builder)
        {
            //配置认证
            builder.Services.AddAuthentication(JwtBearerDefaults.AuthenticationScheme).AddJwtBearer(x =>
            {
                var securyKeyByte = Encoding.UTF8.GetBytes(builder.Configuration["Authentication:SecurityKey"]);
                //验证token相关参数
                x.TokenValidationParameters = new TokenValidationParameters()
                {
                    RequireExpirationTime = true,
                    ValidateIssuer = true,
                    ValidateAudience = true,
                    ValidateLifetime = true,
                    ValidIssuer = builder.Configuration["Authentication:Issuer"],
                    ValidAudience = builder.Configuration["Authentication:Audience"],
                    ClockSkew = TimeSpan.Zero,
                    IssuerSigningKey = new SymmetricSecurityKey(securyKeyByte)
                };
                x.Events = new JwtBearerEvents()
                {
                    OnAuthenticationFailed = fail =>
                    {
                        return Task.CompletedTask;
                    }
                };
            });
        }

        /// <summary>
        /// 跨域
        /// </summary>
        /// <param name="builder"></param>
        public static void AddCors(this WebApplicationBuilder builder)
        {
            builder.Services.AddCors(policy =>
            {
                policy.AddPolicy("CorsPolicy", opt => opt
                .AllowAnyOrigin()
                .AllowAnyHeader()
                .AllowAnyMethod()
                .WithExposedHeaders("Authorization"));
            });
        }


        /// <summary>
        /// 添加日志应用服务
        /// </summary>
        /// <param name="builder"></param>
        public static void AddSerilogEsClient(this WebApplicationBuilder builder)
        {
            if (bool.Parse(builder.Configuration["LogConf:EnableES"]))
            {
                var baseIndexName = builder.Configuration["LogConf:EsIndex"];
                // 创建按月滚动的索引格式
                var indexFormat = $"{baseIndexName}-{{0:yyyy.MM}}";
                Log.Logger = new LoggerConfiguration().WriteTo.Elasticsearch(new ElasticsearchSinkOptions(new Uri(builder.Configuration["LogConf:ESUrl"]))
                {
                    IndexFormat = indexFormat,
                    AutoRegisterTemplate = true,
                    AutoRegisterTemplateVersion = AutoRegisterTemplateVersion.ESv7,
                    ModifyConnectionSettings = x => x.BasicAuthentication(builder.Configuration["LogConf:ESUserName"], builder.Configuration["LogConf:ESPassword"])
                }).MinimumLevel.Warning() // 设置全局最小日志级别为Warning
                  .CreateLogger();
                //配置serilog
                builder.Services.AddLogging(option =>
                {
                //    option.ClearProviders();  //移除原有的日志服务
                    option.AddSerilog();// 注册 Serilog
                });
            }

        }

    }
}
