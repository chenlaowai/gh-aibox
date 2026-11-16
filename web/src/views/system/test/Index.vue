<script setup lang='ts'>
function initializeCanvas() {
  const canvas = document.getElementById('canvas') as HTMLCanvasElement
  const ctx = canvas.getContext('2d')
  const dpr = window.devicePixelRatio || 1

  const { width, height } = canvas.getBoundingClientRect()
  canvas.width = width * dpr
  canvas.height = height * dpr
  ctx?.scale(dpr, dpr)

  const numRectangles = 10 // 要生成的矩形数量
  const w = 80 // 矩形的宽度
  const h = 40 // 矩形的高度
  const spacing = 10 // 矩形之间的间距

  if (!ctx)
    return

  createRectangles(ctx, numRectangles, w, h, spacing, w * 3 + spacing * 3, 0) // 在画布的右上角绘制矩形
  createRectangles(ctx, numRectangles, w, h, spacing, 0, h + spacing)
  createRectangles(ctx, numRectangles, w, h, spacing, 0, h * 2 + spacing * 2, 'vertical')

  canvas.addEventListener('mousemove', (e) => {
    const rect = canvas.getBoundingClientRect()
    const x = e.clientX - rect.left
    const y = e.clientY - rect.top
  })
}

function createRectangles(ctx: CanvasRenderingContext2D, num: number, width: number, height: number, spacing: number, firstX = 0, firstY = 0, derection = 'horizontal' as 'horizontal' | 'vertical') {
  for (let i = 0; i < num; i++) {
    const x = firstX + (derection === 'horizontal' ? i * (width + spacing) : 0)
    const y = firstY + (derection === 'vertical' ? i * (height + spacing) : 0)
    ctx.fillStyle = `hsl(${Math.random() * 360}, 100%, 50%)` // 随机颜色
    ctx.fillRect(x, y, width, height)

    ctx.fillStyle = '#fff'
    ctx.font = '12px Arial'
    ctx.textAlign = 'center'
    ctx.textBaseline = 'middle'
    ctx.fillText('E56', width / 2 + x, height / 2 + y) // 在矩形中间绘制文本
  }
}
onMounted(initializeCanvas)
</script>

<template>
  <div>测试页面</div>
  <div>组件路径：/system/test/Index.vue</div>

  <div class="h-800px w-1200px">
    <canvas id="canvas" class="wh-full bg-#fff" />
  </div>
</template>

<style></style>
