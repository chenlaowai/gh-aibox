const path = require('node:path')
const { execSync } = require('node:child_process')
const fs = require('fs-extra')
const archiver = require('archiver')
const pkg = require('../package.json')

const dirName = new Date().toLocaleDateString().replace(/\//g, '-')

const targetPath = path.resolve(__dirname, `../archive/${dirName}`)

if (fs.existsSync(targetPath)) {
  buildPackage()
}
else {
  execSync(`mkdir ${targetPath}`)
  buildPackage()
}

function buildPackage() {
  const output = fs.createWriteStream(path.resolve(__dirname, `../archive/${dirName}/${pkg.name}(${getCurrentTime()}).zip`))
  const archive = archiver('zip', {
    zlib: { level: 6 }, // Sets the compression level.
  })

  output.on('close', () => {
    console.log(`压缩包尺寸：${archive.pointer()}`)
    console.log('文件压缩成功')
  })

  output.on('end', () => {
    console.log('Data has been drained')
  })

  archive.on('warning', (err) => {
    console.log(`output->warning`, err)
  })

  archive.on('error', (err) => {
    console.log(`output->error`, err)
  })

  archive.pipe(output)
  archive.directory(path.resolve(__dirname, '../dist'), false)
  archive.finalize()
}

function getCurrentTime() {
  const date = new Date()
  const hours = date.getHours() > 12 ? `下午${date.getHours() - 12}点` : `上午${date.getHours()}点`
  const minutes = date.getMinutes() < 10 ? `0${date.getMinutes()}分` : `${date.getMinutes()}分`

  return `${hours}${minutes}`
}
