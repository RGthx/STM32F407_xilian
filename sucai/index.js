// === Global music helpers (accessible from anywhere) ===
window.__musicState = { isPlaying: false };

window.playBgMusic = function () {
  const audio = document.getElementById('bg-music');
  if (!audio) return;

  try {
    const p = audio.play(); // must be called in a user gesture for mobile autoplay policies
    if (p && typeof p.then === 'function') {
      p.then(() => {
        window.__musicState.isPlaying = true;
        const btn = document.getElementById('music-btn');
        if (btn) btn.setAttribute('src', 'resources/images/musicon.webp');
      }).catch(() => {});
    } else {
      window.__musicState.isPlaying = !audio.paused;
      if (window.__musicState.isPlaying) {
        const btn = document.getElementById('music-btn');
        if (btn) btn.setAttribute('src', 'resources/images/musicon.webp');
      }
    }
  } catch (_) {}
};

window.pauseBgMusic = function () {
  const audio = document.getElementById('bg-music');
  if (!audio) return;
  try { audio.pause(); } catch (_) {}
  window.__musicState.isPlaying = false;
  const btn = document.getElementById('music-btn');
  if (btn) btn.setAttribute('src', 'resources/images/musicoff.webp');
};


// === Main page logic ===
$(function () {
  let timerId = null;

  const texts = {
    en: {
      title: "H E L L O  W O R L D",
      subTitle1: "「This time, together with me…」",
      subTitle2: "「Let’s write a new chapter of the story♪」",
      days: "days",
      hours: "hours",
      min: "min",
      sec: "sec",
      countingUp: "♪"
    },
    zh: {
      title: "H E L L O  W O R L D",
      subTitle1: "「这次，与我一起……」",
      subTitle2: "「为故事写下新的篇章吧♪」",
      days: "days",
      hours: "hours",
      min: "min",
      sec: "sec",
      countingUp: "♪"
    }
  };

  function getUserLang() {
    const lang = navigator.language || navigator.userLanguage || "";
    return lang.toLowerCase().startsWith("zh") ? "zh" : "en";
  }

  let currentLang = getUserLang();

  function setLangText(lang = currentLang) {
    const t = texts[lang];
    $(".title").text(t.title);
    $(".sub-title").eq(0).text(t.subTitle1);
    $(".sub-title").eq(1).text(t.subTitle2);
    $(".days .text").text(t.days);
    $(".hours .text").text(t.hours);
    $(".min .text").text(t.min);
    $(".sec .text").text(t.sec);
    currentLang = lang;
  }

  function parseAsUTC8(dateTimeStr) {
    const iso = dateTimeStr.replace(" ", "T") + "+08:00";
    const d = new Date(iso);
    if (!isNaN(d.getTime())) return d;
    const m = dateTimeStr.match(/^(\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2}):(\d{2})$/);
    if (!m) return new Date(NaN);
    const [_, Y, M, D, h, mnt, s] = m.map(Number);
    return new Date(Date.UTC(Y, M - 1, D, h - 8, mnt, s, 0));
  }

  function splitDHMS(ms) {
    ms = Math.max(0, Math.floor(ms));
    const dayMs = 24 * 60 * 60 * 1000;
    const hourMs = 60 * 60 * 1000;
    const minMs = 60 * 1000;
    const days = Math.floor(ms / dayMs);
    ms %= dayMs;
    const hours = Math.floor(ms / hourMs);
    ms %= hourMs;
    const minutes = Math.floor(ms / minMs);
    ms %= minMs;
    const seconds = Math.floor(ms / 1000);
    const pad = (n) => (n < 10 ? "0" + n : String(n));
    return { d: pad(days), h: pad(hours), m: pad(minutes), s: pad(seconds) };
  }

  function renderDHMS({ d, h, m, s }) {
    $("p.day").text(d);
    $("p.hour").text(h);
    $("p.min").text(m);
    $("p.sec").text(s);
  }

  function startTicker(targetDateUTC) {
    if (timerId) {
      clearInterval(timerId);
      timerId = null;
    }

    const setModeVisual = (isCountUp) => {
      const $root = $(".time");
      $root.toggleClass("counting-up", isCountUp);
      const t = texts[currentLang];
      const tipBase = $(".tips").attr("data-tip-base") || $(".tips").text();
      $(".tips").attr("data-tip-base", tipBase);
      if (isCountUp) {
        $(".tips").text(`${tipBase} · ${t.countingUp}`);
      } else {
        $(".tips").text(tipBase);
      }
    };

    const tick = () => {
      const now = new Date();
      const diff = targetDateUTC.getTime() - now.getTime();
      if (diff >= 0) {
        setModeVisual(false);
        renderDHMS(splitDHMS(diff));
      } else {
        setModeVisual(true);
        renderDHMS(splitDHMS(-diff));
      }
    };

    tick();
    timerId = setInterval(tick, 1000);
  }

  const futureTime = "2025-11-05 12:00:00";
  const targetDate = parseAsUTC8(futureTime);

  setLangText();
  $(".tips").text(`${futureTime} (UTC+8)`);
  startTicker(targetDate);

  const videos = [
    "resources/images/cyrene.mp4",
    "resources/images/cyrene.mp4"
  ];
  const randomVideo = videos[Math.floor(Math.random() * videos.length)];
  const videoElement = document.getElementById("bg-video");
  if (videoElement) {
    videoElement.src = randomVideo;
    videoElement.muted = true;
    videoElement.playsInline = true;
    const tryPlay = () => {
      const p = videoElement.play();
      if (p && typeof p.catch === "function") p.catch(() => {});
    };
    videoElement.load();
    tryPlay();
  }

  $("#switch-en").on("click", function () { setLangText("en"); });
  $("#switch-zh").on("click", function () { setLangText("zh"); });

  // Music button logic (uses global helpers)
  const musicBtn = $("#music-btn");
  if (musicBtn.length) {
    // initial icon: not playing
    musicBtn.attr("src", "resources/images/musicoff.webp");
    musicBtn.on("click", function () {
      if (window.__musicState.isPlaying) {
        window.pauseBgMusic();
      } else {
        window.playBgMusic();
      }
    });
  }
});

// === Intro overlay logic (black screen at top of page) with 6s auto-dismiss ===
(function () {
  function run() {
    const intro = document.getElementById('intro');
    if (!intro) return;

    document.body.classList.add('no-scroll');
    let autoTimer = null;

    // 在无手势场景下也尽力播音乐；若被拦截，则等到下一次用户手势再补播
    function ensureMusic() {
      const audio = document.getElementById('bg-music');
      if (!audio) return;

      // 直接尝试；若被浏览器策略拦截，下面挂的手势会兜底
      window.playBgMusic();

      const resumeOnGesture = () => {
        ['click','touchstart','keydown'].forEach(evt =>
          document.removeEventListener(evt, resumeOnGesture)
        );
        window.playBgMusic();
      };
      ['click','touchstart','keydown'].forEach(evt =>
        document.addEventListener(evt, resumeOnGesture, { once: true })
      );
    }

    function dismissIntro(isAuto = false) {
      if (!intro || intro.classList.contains('hidden')) return;

      if (autoTimer) {
        clearTimeout(autoTimer);
        autoTimer = null;
      }

      if (isAuto) {
        ensureMusic();
      } else {
        // 手动关闭时一定是手势回调里，直接播音乐
        window.playBgMusic();
      }

      intro.classList.add('hidden');
      intro.addEventListener('transitionend', () => {
        intro.remove();
      }, { once: true });

      document.body.classList.remove('no-scroll');
    }

    // 用户手势立即关闭
    ['click','touchstart','keydown'].forEach(evt =>
      intro.addEventListener(evt, () => dismissIntro(false), { once: true })
    );

    // 6 秒后自动关闭
    autoTimer = setTimeout(() => dismissIntro(true), 6000);
  }

  // 保证在外部 JS 中也能等 DOM 准备好
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', run, { once: true });
  } else {
    run();
  }
})();