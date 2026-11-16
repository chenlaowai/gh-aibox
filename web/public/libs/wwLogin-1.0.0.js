(function (window, document, _undefined) {
  function WwLogin(data) {
    let host = data.host || 'https://jjt.ccccltd.cn'
    const frame = document.createElement('iframe')
    let url = `${host}/wwopen/sso/qrConnect?appid=${data.appid}&agentid=${data.agentid}&redirect_uri=${data.redirect_uri}&state=${data.state}&login_type=jssdk`
    url += data.style ? (`&style=${data.style}`) : ''
    url += data.href ? (`&href=${data.href}`) : ''
    frame.src = url
    frame.frameBorder = '0'
    frame.allowTransparency = 'true'
    frame.scrolling = 'no'
    frame.width = '300px'
    frame.height = '400px'
    const el = document.getElementById(data.id)
    el.innerHTML = ''
    el.appendChild(frame)

    frame.onload = function () {
      if (frame.contentWindow.postMessage && window.addEventListener) {
        window.addEventListener('message', (event) => {
          const hostArr = host.split(':')
          if (Number(hostArr[1]) === 80)
            host = hostArr[0]
          if (event.data && event.origin.includes(host)) {
            window.location.href = event.data
          }
        })
        frame.contentWindow.postMessage('ask_usePostMessage', '*')
      }
    }
  }

  window.WwLogin = WwLogin
})(window, document)
