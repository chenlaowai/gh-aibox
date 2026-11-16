using Microsoft.OpenApi.Models;
using Swashbuckle.AspNetCore.SwaggerGen;
using System.ComponentModel;
using System.Reflection;

namespace HNKC.AIBox.Admin.Api.Swagger
{
    public class EnumSchemaFilter : ISchemaFilter
    {
        public void Apply(OpenApiSchema schema, SchemaFilterContext context)
        {
            if (context.Type.IsEnum)
            {
                var arr = new List<(string name, int value, string description)>();
                foreach (var item in Enum.GetValues(context.Type))
                {
                    var name = item.ToString() ?? "";
                    var attrs = context.Type.GetField(name)?.GetCustomAttributes<DescriptionAttribute>(true);
                    if (attrs != null && attrs.Any())
                    {
                        var desc = attrs.First().Description;
                        arr.Add((name, (int)item, desc));
                    }
                }

                var desctiption = string.Join(", ", arr.Select(a => $"{a.value}={a.description}"));
                if (!string.IsNullOrWhiteSpace(schema.Description))
                {
                    desctiption = schema.Description + "： " + desctiption;
                }

                schema.Description = desctiption;
            }
        }
    }
}
