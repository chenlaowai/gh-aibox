#See https://aka.ms/customizecontainer to learn how to customize your debug container and how Visual Studio uses this Dockerfile to build your images for faster debugging.

FROM 10.10.140.21/basic/mcr.microsoft.com/dotnet/aspnet:8.0 AS base
WORKDIR /app
EXPOSE 80

FROM 10.10.140.21/basic/mcr.microsoft.com/dotnet/sdk:8.0 AS build
ARG BUILD_CONFIGURATION=Release
WORKDIR /src
COPY ["src/Services/HNKC.AIBox.Admin.Api/HNKC.AIBox.Admin.Api.csproj", "src/Services/HNKC.AIBox.Admin.Api/"]
COPY ["src/Services/HNKC.AIBox.Services/HNKC.AIBox.Services.csproj", "src/Services/HNKC.AIBox.Services/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.Entities/HNKC.AIBox.Entities.csproj", "src/BuildingBlocks/HNKC.AIBox.Entities/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.Enums/HNKC.AIBox.Enums.csproj", "src/BuildingBlocks/HNKC.AIBox.Enums/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.Exceptions/HNKC.AIBox.Exceptions.csproj", "src/BuildingBlocks/HNKC.AIBox.Exceptions/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.Models/HNKC.AIBox.Models.csproj", "src/BuildingBlocks/HNKC.AIBox.Models/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.SqlSugars/HNKC.AIBox.SqlSugars.csproj", "src/BuildingBlocks/HNKC.AIBox.SqlSugars/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.Utils/HNKC.AIBox.Utils.csproj", "src/BuildingBlocks/HNKC.AIBox.Utils/"]
COPY ["src/BuildingBlocks/HNKC.AIBox.Web/HNKC.AIBox.Web.csproj", "src/BuildingBlocks/HNKC.AIBox.Web/"]

RUN dotnet nuget remove source nuget.org
RUN dotnet nuget add source https://rep.ccgdc.com/repository/nuget-group/index.json -n "hnkcch.nuget"

RUN dotnet restore "./src/Services/HNKC.AIBox.Admin.Api/HNKC.AIBox.Admin.Api.csproj"
COPY . .
WORKDIR "/src/src/Services/HNKC.AIBox.Admin.Api"
RUN dotnet build "./HNKC.AIBox.Admin.Api.csproj" -c $BUILD_CONFIGURATION -o /app/build

FROM build AS publish
ARG BUILD_CONFIGURATION=Release
RUN dotnet publish "./HNKC.AIBox.Admin.Api.csproj" -c $BUILD_CONFIGURATION -o /app/publish /p:UseAppHost=false

ENV TZ=Asia/Shanghai
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

FROM base AS final
WORKDIR /app
# RUN apt-get update && apt-get install -y procps
COPY --from=publish /app/publish .
COPY src/Services/HNKC.AIBox.Admin.Api/wwwroot ./wwwroot
ENTRYPOINT ["dotnet", "HNKC.AIBox.Admin.Api.dll"]