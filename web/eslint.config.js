// eslint.config.js
import antfu from '@antfu/eslint-config'
import checkFile from 'eslint-plugin-check-file'

// https://github.com/antfu/eslint-config
export default antfu({
  vue: true,
  unocss: true,
  formatters: true,
  plugins: { 'check-file': checkFile },
  rules: {
    'no-console': 'off',
    '@typescript-eslint/consistent-type-definitions': 'off',
    'eslint-comments/no-unlimited-disable': 'off',
    'ts/ban-ts-comment': 'off',
    'ts/no-unused-expressions': 'off',
    'style/max-statements-per-line': 'off',
  },
}, {
  files: ['src/**'],
  rules: {
    'check-file/filename-naming-convention': [
      'error',
      {
        '**/*.{vue}': 'PASCAL_CASE', // 大驼峰
        '**/*.{jsx,tsx,js,ts}': 'CAMEL_CASE', // 小驼峰
      },
      {
        ignoreMiddleExtensions: true,
      },
    ],
    'check-file/folder-naming-convention': [
      'error',
      {
        '**/': 'KEBAB_CASE',
      },
    ],
  },
})
