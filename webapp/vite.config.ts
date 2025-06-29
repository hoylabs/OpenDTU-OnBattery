import { fileURLToPath, URL } from 'node:url'

import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

import viteCompression from 'vite-plugin-compression';
import cssInjectedByJsPlugin from 'vite-plugin-css-injected-by-js'
import VueI18nPlugin from '@intlify/unplugin-vue-i18n/vite'

import path from 'path'

// example 'vite.user.ts': export const proxy_target = '192.168.16.107'
let proxy_target;
try {
    // eslint-disable-next-line
    proxy_target = require('./vite.user.ts').proxy_target;
} catch {
    proxy_target = '192.168.20.110';
}

// https://vitejs.dev/config/
export default defineConfig(({ command }) => { return {
  plugins: [
    vue(),
    viteCompression({ deleteOriginFile: true, threshold: 0 }),
    cssInjectedByJsPlugin(),
    VueI18nPlugin({
        /* options */
        include: path.resolve(path.dirname(fileURLToPath(import.meta.url)), './src/locales/**.json'),
        runtimeOnly: false,
        fullInstall: false,
        forceStringify: true,
        strictMessage: false,
    }),
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      '~bootstrap': path.resolve(__dirname, 'node_modules/bootstrap'),
    }
  },
  build: {
    // Prevent vendor.css being created
    cssCodeSplit: false,
    outDir: '../webapp_dist',
    emptyOutDir: true,
    minify: 'terser',
    chunkSizeWarningLimit: 1024,
    rollupOptions: {
      output: {
        // Only create one js file
        inlineDynamicImports: true,
        // Get rid of hash on js file
        entryFileNames: 'js/app.js',
        // Get rid of hash on css file
        assetFileNames: "assets/[name].[ext]",
      },
    },
    target: 'es2022',
  },
  esbuild: {
    drop: command !== 'serve' ? ['console', 'debugger'] : []
  },
  server: {
    proxy: {
      '^/api': {
        target: 'http://' + proxy_target
      },
      '^/livedata': {
        target: 'ws://' + proxy_target,
        ws: true,
        changeOrigin: true
      },
      '^/solarchargerlivedata': {
        target: 'ws://' + proxy_target,
        ws: true,
        changeOrigin: true
      },
      '^/batterylivedata': {
        target: 'ws://' + proxy_target,
        ws: true,
        changeOrigin: true
      },
      '^/gridchargerlivedata': {
        target: 'ws://' + proxy_target,
        ws: true,
        changeOrigin: true
      },
      '^/console': {
        target: 'ws://' + proxy_target,
        ws: true,
        changeOrigin: true
      }
    }
  }
} })
