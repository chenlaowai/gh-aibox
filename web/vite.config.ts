import { resolve } from 'node:path'
import { defineConfig, loadEnv } from 'vite'
import { createVitePlugins } from './build/plugins'

// https://vitejs.dev/config/
export default defineConfig(({ mode }) => {
  // 根据当前工作目录中的 `mode` 加载 .env 文件
  const env = loadEnv(mode, __dirname, '') as ImportMetaEnv

  return {
    base: env.VITE_BASE_URL,
    plugins: createVitePlugins(env),
    resolve: {
      alias: {
        '@': resolve(__dirname, 'src'),
      },
    },
    server: {
      host: '0.0.0.0',
      proxy:
        env.VITE_HTTP_PROXY === 'Y' && env.VITE_HTTP_PROXY_URL
          ? {
              [env.VITE_HTTP_PROXY_URL]: {
                target: env.VITE_BASE_API,
                changeOrigin: true,
                rewrite: (path: string) => path.replace(new RegExp(`^${env.VITE_HTTP_PROXY_URL}`), ''),
              },
            }
          : undefined,
    },
    build: {
      target: 'esnext',
      reportCompressedSize: false, // 启用/禁用 gzip 压缩大小报告
    },
    optimizeDeps: {
      include: ['echarts', 'md-editor-v3', 'quill'],
    },
    esbuild: {
      drop: ['debugger'],
      pure: env.VITE_NODE_ENV === 'production' ? ['console.log', 'console.warn'] : [],
    },
  }
})
