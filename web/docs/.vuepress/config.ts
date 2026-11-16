import { viteBundler } from '@vuepress/bundler-vite'
import { defaultTheme } from '@vuepress/theme-default'
import { defineUserConfig } from 'vuepress'
import { registerComponentsPlugin } from '@vuepress/plugin-register-components'
import { getDirname, path } from 'vuepress/utils'

const __dirname = import.meta.dirname || getDirname(import.meta.url)

export default defineUserConfig({
  bundler: viteBundler(),
  theme: defaultTheme({
    colorMode: 'dark',
    colorModeSwitch: false,
    sidebar: [
      {
        text: '指南',
        collapsible: false,
        children: [
          {
            text: '介绍',
            link: '/introduction/',
          },
          {
            text: '重要文件',
            link: '/important/',
          },
          {
            text: '编码风格',
            link: '/encoding-style/',
          },
          {
            text: '项目概览',
            link: '/project/',
          },
          {
            text: '特殊表单控件',
            link: '/special-form/',
          },
          {
            text: '密码加密',
            link: '/encryption/',
          },
        ],
      },
    ],
  }),

  lang: 'zh-CN',
  title: '基础工程文档',
  description: '',

  plugins: [
    registerComponentsPlugin({
      componentsDir: path.resolve(__dirname, './components'),
    }),
  ],
})
