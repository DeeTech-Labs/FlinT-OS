(() => {
  const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  const reveal = (el) => el.classList.add("is-in");

  const observe = (selector, onEnter) => {
    const nodes = [...document.querySelectorAll(selector)];
    if (!nodes.length) return;

    if (reduce || !("IntersectionObserver" in window)) {
      nodes.forEach(onEnter);
      return;
    }

    const pending = new Set(nodes);

    const tryReveal = (el) => {
      if (!pending.has(el)) return;
      pending.delete(el);
      onEnter(el);
      io.unobserve(el);
    };

    const flushVisible = () => {
      for (const el of [...pending]) {
        const rect = el.getBoundingClientRect();
        if (rect.top < window.innerHeight * 0.95) tryReveal(el);
      }
    };

    const io = new IntersectionObserver(
      (entries) => {
        for (const entry of entries) {
          if (!entry.isIntersecting) continue;
          tryReveal(entry.target);
        }
      },
      { threshold: 0.05, rootMargin: "40px 0px -2% 0px" }
    );

    nodes.forEach((el) => io.observe(el));
    flushVisible();
    window.addEventListener("scroll", flushVisible, { passive: true });
    window.addEventListener("resize", flushVisible, { passive: true });
    document.addEventListener("flint:content", () => {
      document.querySelectorAll(selector).forEach((el) => {
        if (el.classList.contains("is-in")) return;
        pending.add(el);
        io.observe(el);
      });
      flushVisible();
    });
  };

  observe("[data-animate], [data-reveal]", reveal);

  const startCycle = (root) => {
    if (root.dataset.cycling === "1") return;
    root.dataset.cycling = "1";
    const kind = root.getAttribute("data-cycle");
    const items =
      kind === "step"
        ? root.querySelectorAll(".flow__step")
        : root.querySelectorAll(":scope > li");
    if (items.length < 2) return;

    let i = 0;
    items[0].classList.add("is-active");
    window.setInterval(() => {
      items[i].classList.remove("is-active");
      i = (i + 1) % items.length;
      items[i].classList.add("is-active");
    }, 1400);
  };

  if (!reduce) {
    observe("[data-cycle]", (el) => {
      reveal(el.closest(".viz") || el);
      startCycle(el);
    });
  }

  // Bash syntax highlight
  const highlightBash = (codeEl) => {
    const esc = (s) =>
      s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");

    const paint = (token) => {
      if (/^\\$/.test(token)) return `<span class="tok-punct">\\</span>`;
      if (/^--?[a-zA-Z0-9-]+$/.test(token))
        return `<span class="tok-flag">${esc(token)}</span>`;
      if (
        /^(?:cp|idf\.py|set-target|build|flash|monitor|esp32c3)$/.test(token)
      )
        return `<span class="tok-cmd">${esc(token)}</span>`;
      if (/^\/dev\//.test(token))
        return `<span class="tok-path">${esc(token)}</span>`;
      if (/\.(?:h|c|cpp|py|md|json)$/.test(token))
        return `<span class="tok-file">${esc(token)}</span>`;
      return esc(token);
    };

    codeEl.innerHTML = codeEl.textContent
      .split("\n")
      .map((line) => {
        if (!line) return "";
        const lead = line.match(/^\s*/)?.[0] || "";
        const body = line.slice(lead.length);
        const bits = body.match(/\\$|\/dev\/\S+|\S+/g) || [];
        return esc(lead) + bits.map(paint).join(" ");
      })
      .join("\n");
  };

  document
    .querySelectorAll("code.language-bash")
    .forEach((el) => highlightBash(el));

  // Scroll progress
  const progress = document.querySelector(".scroll-progress > span");
  if (progress) {
    const onScroll = () => {
      const max = document.documentElement.scrollHeight - window.innerHeight;
      const p = max > 0 ? window.scrollY / max : 0;
      progress.style.transform = `scaleX(${Math.min(1, Math.max(0, p))})`;
      document.body.classList.toggle("is-scrolled", window.scrollY > 24);
    };
    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();
  }

  // Cursor glow (fine pointer only)
  const glow = document.querySelector(".cursor-glow");
  if (glow && !reduce && window.matchMedia("(pointer: fine)").matches) {
    let x = 0;
    let y = 0;
    let gx = 0;
    let gy = 0;
    window.addEventListener(
      "pointermove",
      (e) => {
        x = e.clientX;
        y = e.clientY;
        glow.classList.add("is-on");
      },
      { passive: true }
    );
    const tickGlow = () => {
      gx += (x - gx) * 0.12;
      gy += (y - gy) * 0.12;
      glow.style.transform = `translate(${gx}px, ${gy}px)`;
      requestAnimationFrame(tickGlow);
    };
    requestAnimationFrame(tickGlow);
  }

  // Soft pointer parallax on hero glow
  const spark = document.querySelector(".atmosphere__spark");
  if (spark && !reduce && window.matchMedia("(pointer: fine)").matches) {
    let mx = 0;
    let my = 0;
    let sx = 0;
    let sy = 0;

    window.addEventListener(
      "pointermove",
      (e) => {
        mx = (e.clientX / window.innerWidth - 0.5) * 28;
        my = (e.clientY / window.innerHeight - 0.5) * 18;
      },
      { passive: true }
    );

    const tickParallax = () => {
      sx += (mx - sx) * 0.06;
      sy += (my - sy) * 0.06;
      spark.style.transform = `translate(${sx}px, ${sy}px)`;
      requestAnimationFrame(tickParallax);
    };
    requestAnimationFrame(tickParallax);
  }

  // Magnetic buttons
  if (!reduce && window.matchMedia("(pointer: fine)").matches) {
    document.querySelectorAll(".btn").forEach((btn) => {
      btn.addEventListener("pointermove", (e) => {
        const r = btn.getBoundingClientRect();
        const dx = e.clientX - (r.left + r.width / 2);
        const dy = e.clientY - (r.top + r.height / 2);
        btn.style.transform = `translate(${dx * 0.12}px, ${dy * 0.16}px)`;
      });
      btn.addEventListener("pointerleave", () => {
        btn.style.transform = "";
      });
    });
  }

  // Tick scope schematic (not live device data)
  const scope = document.getElementById("tick-scope");
  if (scope && !reduce) {
    const ctx = scope.getContext("2d");
    const DPR = Math.min(window.devicePixelRatio || 1, 2);
    let w = 0;
    let h = 0;
    let t0 = performance.now();
    let running = true;

    const resizeScope = () => {
      const cssW = scope.clientWidth || 640;
      const cssH = 180;
      w = cssW;
      h = cssH;
      scope.width = Math.floor(cssW * DPR);
      scope.height = Math.floor(cssH * DPR);
      scope.style.height = `${cssH}px`;
      ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
    };

    const drawScope = (now) => {
      if (!running) return;
      const t = (now - t0) / 1000;
      ctx.clearRect(0, 0, w, h);

      ctx.strokeStyle = "rgba(232,230,227,0.08)";
      ctx.lineWidth = 1;
      for (let y = 20; y < h; y += 28) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(w, y);
        ctx.stroke();
      }

      const mid = h * 0.55;
      const drawPulseTrain = (period, color, amp, phase) => {
        ctx.beginPath();
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        let started = false;
        for (let x = 0; x <= w; x++) {
          const u = (x / w) * 8 + t / period + phase;
          const beat = Math.abs(((u % 1) + 1) % 1);
          const spike = beat < 0.08 ? Math.sin((beat / 0.08) * Math.PI) : 0;
          const y = mid - spike * amp;
          if (!started) {
            ctx.moveTo(x, y);
            started = true;
          } else ctx.lineTo(x, y);
        }
        ctx.stroke();
      };

      drawPulseTrain(1.0, "rgba(255,106,0,0.85)", h * 0.32, 0);
      drawPulseTrain(6.0, "rgba(158,203,255,0.7)", h * 0.18, 0.15);

      // Playhead
      const px = ((t * 0.12) % 1) * w;
      ctx.strokeStyle = "rgba(255,255,255,0.2)";
      ctx.beginPath();
      ctx.moveTo(px, 12);
      ctx.lineTo(px, h - 12);
      ctx.stroke();

      requestAnimationFrame(drawScope);
    };

    document.addEventListener("visibilitychange", () => {
      running = document.visibilityState === "visible";
      if (running) requestAnimationFrame(drawScope);
    });

    window.addEventListener("resize", resizeScope, { passive: true });
    resizeScope();
    requestAnimationFrame(drawScope);
  } else if (scope && reduce) {
    const ctx = scope.getContext("2d");
    if (ctx) {
      scope.width = 640;
      scope.height = 180;
      ctx.fillStyle = "rgba(255,106,0,0.15)";
      ctx.fillRect(0, 80, 640, 2);
    }
  }

  // Signal field
  const canvas = document.getElementById("signal-field");
  if (!canvas || reduce) return;

  const ctx = canvas.getContext("2d", { alpha: true });
  if (!ctx) return;

  const DPR = Math.min(window.devicePixelRatio || 1, 2);
  let fw = 0;
  let fh = 0;
  let nodes = [];
  let running = true;

  const resize = () => {
    fw = window.innerWidth;
    fh = window.innerHeight;
    canvas.width = Math.floor(fw * DPR);
    canvas.height = Math.floor(fh * DPR);
    canvas.style.width = `${fw}px`;
    canvas.style.height = `${fh}px`;
    ctx.setTransform(DPR, 0, 0, DPR, 0, 0);

    const count = Math.round(Math.min(40, Math.max(16, (fw * fh) / 48000)));
    nodes = Array.from({ length: count }, () => ({
      x: Math.random() * fw,
      y: Math.random() * fh,
      vx: (Math.random() - 0.5) * 0.22,
      vy: (Math.random() - 0.5) * 0.22,
      r: 1 + Math.random() * 1.3,
    }));
  };

  const draw = () => {
    if (!running) return;
    ctx.clearRect(0, 0, fw, fh);

    for (const n of nodes) {
      n.x += n.vx;
      n.y += n.vy;
      if (n.x < -20) n.x = fw + 20;
      if (n.x > fw + 20) n.x = -20;
      if (n.y < -20) n.y = fh + 20;
      if (n.y > fh + 20) n.y = -20;
    }

    const linkDist = Math.min(150, fw * 0.11);
    for (let i = 0; i < nodes.length; i++) {
      for (let j = i + 1; j < nodes.length; j++) {
        const a = nodes[i];
        const b = nodes[j];
        const d = Math.hypot(a.x - b.x, a.y - b.y);
        if (d > linkDist) continue;
        const alpha = (1 - d / linkDist) * 0.16;
        ctx.strokeStyle = `rgba(255, 106, 0, ${alpha})`;
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(a.x, a.y);
        ctx.lineTo(b.x, b.y);
        ctx.stroke();
      }
    }

    for (const n of nodes) {
      ctx.fillStyle = "rgba(255, 106, 0, 0.4)";
      ctx.beginPath();
      ctx.arc(n.x, n.y, n.r, 0, Math.PI * 2);
      ctx.fill();
    }

    requestAnimationFrame(draw);
  };

  document.addEventListener("visibilitychange", () => {
    running = document.visibilityState === "visible";
    if (running) requestAnimationFrame(draw);
  });

  window.addEventListener("resize", resize, { passive: true });
  resize();
  requestAnimationFrame(draw);
})();
