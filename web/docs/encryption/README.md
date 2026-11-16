# 密码加密

AES加密所需要的key和iv直接明文写死在代码当中不符合网络安全要求。因此，我们通过抽象表达，抽离出来一个单独的js文件——keyiv.js。所在路径:@/utils/keyiv.js

用法如下：

```js
// 第一步，引入CrytoJs和keyiv.js
import CryptoJS from 'crypto-js'
import '@/utils/keyiv.js'

// 第二步，密码加密
const password = CryptoJS.AES.encrypt(modelRef.value.password, CryptoJS.enc.Utf8.parse(key), {
  // @ts-ignore
  iv: CryptoJS.enc.Utf8.parse(iv),
  mode: CryptoJS.mode.CBC, // 加密模式：CBC
  padding: CryptoJS.pad.Pkcs7, // 补全模式：pkcs7
}).toString()
```
