import {
  defineConfig,
  presetAttributify,
  presetIcons,
  presetUno,
  transformerDirectives,
  transformerVariantGroup,
} from 'unocss'
import presetRemToPx from '@unocss/preset-rem-to-px'

// https://github.com/unocss/unocss

export default defineConfig({
  presets: [
    presetUno({ dark: 'class' }),
    presetRemToPx() as any,
    presetIcons({
      scale: 1.2,
    }),
    presetAttributify(),
  ],
  theme: {
    colors: {
      primary: '#165DFF',
      hover: '#415A87',
    },
  },
  shortcuts: {
    'wh-full': 'w-full h-full',
    'flex-center': 'flex justify-center items-center',
    'flex-col-center': 'flex-center flex-col',
    'flex-x-center': 'flex justify-center',
    'flex-y-center': 'flex items-center',
    'flex-1-hidden': 'flex-1 overflow-hidden',
  },
  transformers: [transformerVariantGroup(), transformerDirectives()],
})
