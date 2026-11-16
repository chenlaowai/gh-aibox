using HNKC.AIBox.Web;
using Microsoft.Extensions.DependencyInjection;

namespace HNKC.AIBox
{
    public static class JwtExtensions
    {
        public static IServiceCollection AddJwtService(this IServiceCollection services, Action<JwtOptions> options)
        {
            options = options ?? throw new ArgumentNullException(nameof(options));
            services = services ?? throw new ArgumentNullException(nameof(services));

            //将实例注入到服务容器中，并将获取的选项值赋值给 options 参数。
            services.Configure("JwtOptions", options);

            services.AddScoped<IJwtService, JwtService>();
            return services;
        }
    }
}
