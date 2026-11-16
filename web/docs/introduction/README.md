## 项目结构与技术栈解析

### 项目定位

本项目基于 Nova-Admin 进行二次开发，采用 Vue3 + Vite5 + TypeScript + NaiveUI + Unocss 等现代前端技术栈，主打中后台管理系统开发，具备灵活的权限管理、丰富的组件封装、响应式适配、主题切换等特性。

### 核心技术与依赖

- 主框架：Vue3（组合式 API，script setup 语法）
- 构建工具：Vite5（极速开发、按需编译、代码分包优化）
- 状态管理：Pinia（轻量、类型友好）
- UI 组件库：NaiveUI（风格统一，支持暗黑模式）、Unocss（原子化 CSS）
- 辅助库：VueUse（增强响应式）、Axios（请求封装）、Dayjs（日期处理）、Echarts（可视化）、@umijs/openapi（自动生成 API 请求代码）
- 地图与三维：mars3d、mars3d-cesium
- 代码规范：TypeScript、ESLint、Prettier、lint-staged、simple-git-hooks

### 目录结构说明

- src/：主源码目录
  - components/：通用组件（common）、自定义组件（custom）
  - views/：页面视图，按业务模块分子目录（如 system、setting、error、login）
  - layouts/：布局组件（如顶部菜单、左侧菜单、混合菜单、基础布局）
  - router/：路由配置（静态路由、动态路由、路由守卫）
  - store/：状态管理（Pinia，含全局 app、tab、auth 等模块）
  - assets/：静态资源（图片、SVG、样式等）
  - services/：API 请求封装与自动生成代码
  - hooks/：自定义 hooks，逻辑复用
  - utils/：工具函数
  - constants/：常量定义
  - typings/：全局类型定义（DTO、VO、全局类型等）
  - directives/：自定义指令（如权限指令 v-permission）
- public/：静态资源目录
- build/、scripts/：构建与自动化脚本
- docs/：文档相关
- 配置文件：vite.config.ts（Vite 配置）、unocss.config.ts（Unocss 配置）、tsconfig.json（TS 配置）、eslint.config.js（ESLint 配置）

### 主要功能模块

- 权限管理：支持静态和动态路由，前后端权限校验，v-permission 指令控制组件可见性。
- 用户、角色、菜单、按钮管理：setting 目录下分模块实现，支持增删改查、权限分配等。
- 登录注册：login 目录下实现多种登录、注册、重置密码等功能。
- 错误页面：error 目录下提供 403、404、500 等错误页面。
- 主题与适配：支持暗黑模式、rem 适配、autofit.js 适配移动端。
- API 自动生成：通过 @umijs/openapi 自动生成 API 请求代码，提升开发效率。
- 地图与三维可视化：集成 mars3d、mars3d-cesium，支持地图与三维场景展示。

### 开发与构建

- 推荐 Node 18.20.3 及以上，支持 nvm 多版本管理。
- 本地开发：pnpm dev，支持代理配置（VITE_HTTP_PROXY）。
- 打包发布：pnpm zip，自动生成压缩包。
- 代码规范：提交前自动 lint 校验，支持多种提交类型（feat、fix、docs 等）。

### 最佳实践与优化

- 组件与逻辑解耦，推荐使用 hooks 复用逻辑。
- 响应式布局，移动优先，Unocss 原子化样式。
- 动态加载非核心组件，Suspense 包裹异步组件，提升首屏性能。
- 图片优化（WebP、懒加载、尺寸标注）。
- Vite 构建优化（代码分包、依赖预编译、压缩优化）。
- 全局类型集中管理，接口数据与界面数据分离（DTO/VO）。
