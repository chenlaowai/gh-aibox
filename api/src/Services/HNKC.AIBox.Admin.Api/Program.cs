using HNKC.AIBox.Services.Admin.Api;
using HNKC.AIBox.Web;

var builder = WebApplication.CreateBuilder(args);
builder.AddProgramExtensions();


var app = builder.Build();


app.UseSwagger();
app.UseSwaggerUI();

if (app.Environment.IsDevelopment())
{
  //  app.UseSwaggerBasicAuth();
    app.UseSwagger();
    app.UseSwaggerUI();
 
}
app.UseEnableBuffering();

app.UseWebSockets(new WebSocketOptions
{
    KeepAliveInterval = TimeSpan.FromSeconds(120),
});

app.UseCors("CorsPolicy");
app.UseStaticFiles();
app.UseAuthentication();
app.UseAuthorization();
app.UseSecurityHeader();

app.MapControllers();

app.Run();