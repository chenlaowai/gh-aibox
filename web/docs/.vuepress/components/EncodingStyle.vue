<script setup lang='ts'>
import { onMounted, ref } from 'vue'

const html = ref<string>()

function init() {
  fetch('/docs/《数智技术中心 JavaScript 风格指南》.html')
    .then(response => response.text())
    .then((data) => {
      console.log('Encoding Style Data:', data)
      // Process the data as needed
      html.value = data
    })
    .catch((error) => {
      console.error('Error fetching encoding style:', error)
    })
}

onMounted(() => {
  document.title = '编码风格 - 基础工程'

  init()
})
</script>

<template>
  <div v-html="html" />
</template>

<style>

</style>
