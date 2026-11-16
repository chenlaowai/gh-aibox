<script setup lang="ts">
import type { FormInst } from 'naive-ui'
import { darkTheme } from 'naive-ui'
import CryptoJS from 'crypto-js'
import { useAuthStore } from '@/store'
import DialogModal from '@/components/custom/DialogModal.vue'

const dialogModalRef = ref()
const authStore = useAuthStore()
const rules = computed(() => {
  return {
    account: {
      required: true,
      trigger: ['blur', 'input'],
      message: '请输入账号',
    },
    pwd: {
      required: true,
      trigger: ['blur', 'input'],
      message: '请输入密码',
    },
  }
})
const formValue = ref({
  account: '',
  pwd: '',
  sliderCaptcha: [] as number[],
})

const formRef = ref<FormInst | null>(null)

async function handleLogin() {
  const { account, pwd, sliderCaptcha } = formValue.value
  const key = 'LIf6HaF90YVHWWwqcHiMN9IpjkGPPm7Q'
  const iv = 'BE56E057F20F883E'
  const password = CryptoJS.AES.encrypt(pwd, CryptoJS.enc.Utf8.parse(key), {
    iv: CryptoJS.enc.Utf8.parse(iv),
    mode: CryptoJS.mode.CBC, // 加密模式：CBC
    padding: CryptoJS.pad.Pkcs7, // 补全模式：pkcs7
  }).toString()
  dialogModalRef.value.close()
  authStore.login(account, password, sliderCaptcha)
}

async function handleVertify() {
  formRef.value?.validate(async (errors) => {
    if (errors)
      return
    dialogModalRef.value?.open('请完成安全验证')
    await nextTick()

    // @ts-expect-error
    const captcha = window.sliderCaptcha({
      id: 'captcha',
      width: 380,
      height: 200,
      sliderL: 42,
      sliderR: 9,
      offset: 10,
      loadingText: '加载中...',
      failedText: '重试',
      barText: '向右拖动滑块填充拼图',
      // repeatIcon: 'fa fa-redo',
      setSrc() {
        const urls = [
          new URL(`@/assets/images/Pic0.jpg`, import.meta.url).href,
          new URL(`@/assets/images/Pic1.jpg`, import.meta.url).href,
          new URL(`@/assets/images/Pic2.jpg`, import.meta.url).href,
          new URL(`@/assets/images/Pic3.jpg`, import.meta.url).href,
          new URL(`@/assets/images/Pic4.jpg`, import.meta.url).href,
        ]
        return urls[Math.round(Math.random() * 4)]
      },
      onSuccess() {
        formValue.value.sliderCaptcha = captcha.trail
        handleLogin()
      },
    })
  })
}
</script>

<template>
  <div>
    <n-config-provider class="wh-full" :theme="darkTheme">
      <n-form
        ref="formRef" :rules="rules" :model="formValue" :show-label="false" size="large"
        class="mt-58px"
      >
        <n-form-item path="account" class="mb-5px" feedback-class="text-16px!">
          <n-input
            v-model:value="formValue.account" v-trim
            class="custom-login-input" placeholder="请输入账号"
            :input-props="{ autocomplete: 'new-password' }"
          >
            <template #prefix>
              <div class="custom-input-prefix">
                <svg-icons-user class="h-20px w-20px" />
              </div>
            </template>
          </n-input>
        </n-form-item>
        <n-form-item path="pwd" class="mb-35px" feedback-class="text-16px!">
          <n-input
            v-model:value="formValue.pwd" v-trim class="custom-login-input"
            type="password" placeholder="请输入密码" show-password-on="click"
            :input-props="{ autocomplete: 'new-password' }"
            @keyup.enter="handleVertify"
          >
            <template #prefix>
              <div class="custom-input-prefix">
                <svg-icons-psd class="h-20px w-20px" />
              </div>
            </template>
            <template #password-invisible-icon>
              <icon-park-outline-preview-close-one class="h-20px w-20px" />
            </template>
            <template #password-visible-icon>
              <icon-park-outline-preview-open class="h-20px w-20px" />
            </template>
          </n-input>
        </n-form-item>
        <n-space vertical :size="20">
          <div
            class="h-55px flex-center cursor-pointer rounded-2px bg-[#0054FFFF] bg-gradient-linear text-20px text-#fff font-500 hover:text-#fff! hover:opacity-80!"
            @click="handleVertify"
          >
            登录
          </div>
        </n-space>
      </n-form>
    </n-config-provider>
    <!-- <div v-show="type === 'code'" id="wx_reg" class="mx-auto mt-47px w-360px flex-center border-rounded-16px bg-#fff pb-40px" /> -->
    <DialogModal ref="dialogModalRef">
      <div id="captcha" style="height: 240px;" />
    </DialogModal>
  </div>
</template>

<style lang="scss" scoped>
input:-internal-autofill-previewed,
input:-internal-autofill-selected {
  -webkit-text-fill-color: #fff !important;
  transition: background-color 50000s ease-in-out 0s !important;
}

#wx_reg {
  > iframe {
    height: 290px;
  }
}

/* 自定义登录输入框样式 */
.custom-login-input {
  height: 50px !important;
  border-radius: 0 !important;

  :deep(.n-input__input-el) {
    color: #ffffffe6 !important;
    font-size: 16px !important;
    background: transparent !important;
    height: 48px !important;
    line-height: 48px !important;

    &::placeholder {
      color: rgba(255, 255, 255, 0.7) !important;
    }

    &:focus {
      color: #ffffffe6 !important;
    }
  }

  :deep(.n-input-wrapper) {
    background: #0054ff29 !important;
    border: 1px solid #0054ff29 !important;
    padding-left: 0 !important;
    height: 50px !important;
  }

  :deep(.n-input__prefix) {
    margin-right: 0 !important;
    padding-right: 16px !important;
  }

  &:focus-within {
    :deep(.n-input-wrapper) {
      border-color: #0054ffff !important;
      background: rgba(0, 84, 255, 0.25) !important;
      box-shadow: 0 0 0 2px rgba(0, 84, 255, 0.2) !important;
    }
  }
}

/* 自定义图标前缀样式 */
.custom-input-prefix {
  width: 50px;
  height: 50px;
  background: #0054ffff;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 0;

  svg {
    color: #fff !important;
    fill: #fff !important;
  }
}

/* 解决 Chrome 自动填充反色问题 */
.custom-login-input :deep(input:-webkit-autofill),
.custom-login-input :deep(input:-webkit-autofill:focus),
.custom-login-input :deep(input:-webkit-autofill:hover),
.custom-login-input :deep(input:-webkit-autofill:active) {
  -webkit-text-fill-color: #ffffffe6 !important; /* 字体色与输入框一致 */
  color: #ffffffe6 !important;
  transition: background-color 5000s ease-in-out 0s !important;
}
</style>
