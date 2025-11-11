(function () {
  const MAX_LINES = 200;
  const BATCH = 3;
  const INTERVAL = 2500;

  // —— URL 解析：优先 data-text-url / 全局变量 / 同目录 cyrene.txt / 常见绝对路径
  function inferCandidates() {
    const s = document.currentScript;
    const base = s && s.src ? new URL('.', s.src) : new URL('.', location.href);
    const urls = [];

    if (typeof window.CYRENE_TEXT_URL === 'string') urls.push(window.CYRENE_TEXT_URL);
    if (s && s.dataset && s.dataset.textUrl) urls.push(s.dataset.textUrl);

    urls.push(new URL('cyrene.txt', base).toString()); // 同目录
    urls.push('/resources/js/cyrene.txt');             // 常见公开路径
    urls.push('cyrene.txt');                           // 兜底（相对当前页）
    return urls;
  }

  async function loadText() {
    // 回退 1：全局数组
    if (Array.isArray(window.ABOUTCYRENE_LINES)) {
      return window.ABOUTCYRENE_LINES.join('\n');
    }
    // 回退 2：全局字符串
    if (typeof window.ABOUTCYRENE_TEXT === 'string') {
      return window.ABOUTCYRENE_TEXT;
    }
    // 主路径：从 cyrene.txt 获取
    const candidates = inferCandidates();
    for (const url of candidates) {
      try {
        const res = await fetch(url, { cache: 'no-store' });
        if (res.ok) return await res.text();
      } catch (_) {  }
    }
    return null;
  }

  function toLines(txt) {
    return txt
      .replace(/^\uFEFF/, '')      // 去 BOM
      .replace(/\r\n?/g, '\n')     // 标准化换行
      .split('\n')
      .filter(Boolean);            // 去空行（如需保留，删掉这一行即可）
  }

  function ensureOverlay() {
    let overlay = document.querySelector('.code-overlay');
    if (!overlay) {
      overlay = document.createElement('div');
      overlay.className = 'code-overlay';
      overlay.innerHTML = '<pre id="code-stream"></pre>';
      document.body.appendChild(overlay);
    }
    return overlay.querySelector('#code-stream');
  }

  function appendLine(pre, text) {
    const div = document.createElement('div');
    div.className = 'line';
    div.textContent = text;
    pre.appendChild(div);
    while (pre.childElementCount > MAX_LINES) {
      pre.removeChild(pre.firstChild);
    }
    pre.parentElement.scrollTop = pre.parentElement.scrollHeight;
  }

  async function boot() {
    const pre = ensureOverlay();
    const text = await loadText();
    const lines = text ? toLines(text) : ['[info] cyrene.txt 未找到，或 ABOUTCYRENE_* 未提供'];

    let idx = 0;
    setInterval(() => {
      for (let i = 0; i < BATCH; i++) {
        appendLine(pre, lines[idx % lines.length]);
        idx++;
      }
    }, INTERVAL);
  }

  boot();
})();
