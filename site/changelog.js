(() => {
  const statusEl = document.getElementById("changelog-status");
  const rootEl = document.getElementById("changelog-root");
  if (!statusEl || !rootEl) return;

  const esc = (s) =>
    s
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;");

  const inline = (text) => {
    let t = esc(text);
    t = t.replace(
      /`([^`]+)`/g,
      '<code class="cl-code">$1</code>'
    );
    t = t.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
    t = t.replace(
      /\[([^\]]+)\]\((https?:\/\/[^)]+)\)/g,
      '<a href="$2" rel="noopener noreferrer">$1</a>'
    );
    return t;
  };

  const parseChangelog = (md) => {
    const lines = md.replace(/\r\n/g, "\n").split("\n");
    const releases = [];
    const links = {};
    let release = null;
    let section = null;
    let intro = [];
    let inIntro = true;
    let para = [];

    const flushPara = () => {
      if (!para.length || !release || section) {
        para = [];
        return;
      }
      release.notes.push(para.join(" "));
      para = [];
    };

    for (const raw of lines) {
      const line = raw.trimEnd();
      const linkDef = line.match(/^\[([^\]]+)\]:\s+(\S+)\s*$/);
      if (linkDef) {
        links[linkDef[1]] = linkDef[2];
        continue;
      }

      const h2 = line.match(/^##\s+\[([^\]]+)\](?:\s+-\s+(.+))?\s*$/);
      if (h2) {
        flushPara();
        inIntro = false;
        release = {
          id: h2[1],
          date: (h2[2] || "").trim(),
          notes: [],
          sections: [],
        };
        section = null;
        releases.push(release);
        continue;
      }

      if (inIntro) {
        if (line.startsWith("# ")) continue;
        if (!line.trim()) continue;
        intro.push(line.trim());
        continue;
      }

      if (!release) continue;

      const h3 = line.match(/^###\s+(.+)\s*$/);
      if (h3) {
        flushPara();
        section = { title: h3[1].trim(), items: [] };
        release.sections.push(section);
        continue;
      }

      const bullet = line.match(/^-\s+(.+)$/);
      if (bullet) {
        flushPara();
        if (!section) {
          section = { title: "Notes", items: [] };
          release.sections.push(section);
        }
        section.items.push(bullet[1].trim());
        continue;
      }

      const cont = line.match(/^\s{2,}(.+)$/);
      if (cont && section && section.items.length) {
        section.items[section.items.length - 1] += " " + cont[1].trim();
        continue;
      }

      if (!line.trim()) {
        flushPara();
        continue;
      }

      if (!section) para.push(line.trim());
    }
    flushPara();

    for (const r of releases) {
      r.href = links[r.id] || null;
    }
    return { intro, releases };
  };

  const render = (data) => {
    const parts = [];
    if (data.intro.length) {
      parts.push(
        `<p class="changelog__intro">${data.intro.map(inline).join(" ")}</p>`
      );
    }

    parts.push('<ol class="cl-timeline">');
    data.releases.forEach((rel, idx) => {
      const ver = rel.href
        ? `<a class="cl-ver" href="${esc(rel.href)}" rel="noopener noreferrer">${esc(rel.id)}</a>`
        : `<span class="cl-ver">${esc(rel.id)}</span>`;
      const date = rel.date
        ? `<time class="cl-date" datetime="${esc(rel.date)}">${esc(rel.date)}</time>`
        : "";

      parts.push(
        `<li class="cl-release" data-reveal style="--i:${idx}">` +
          `<div class="cl-release__head">${ver}${date}</div>`
      );

      for (const note of rel.notes) {
        parts.push(`<p class="cl-note">${inline(note)}</p>`);
      }

      for (const sec of rel.sections) {
        const kind = sec.title.toLowerCase().replace(/[^a-z]+/g, "-");
        parts.push(
          `<section class="cl-section cl-section--${esc(kind)}">` +
            `<h3 class="cl-section__title">${esc(sec.title)}</h3><ul class="cl-list">`
        );
        for (const item of sec.items) {
          parts.push(`<li>${inline(item)}</li>`);
        }
        parts.push("</ul></section>");
      }
      parts.push("</li>");
    });
    parts.push("</ol>");

    rootEl.innerHTML = parts.join("");
    rootEl.hidden = false;
    statusEl.hidden = true;

    // Re-run reveal for injected nodes
    document.dispatchEvent(new CustomEvent("flint:content"));
  };

  const fail = (msg) => {
    statusEl.textContent = msg;
    statusEl.classList.add("is-error");
  };

  const md = window.__FLINT_CHANGELOG_MD__;
  if (typeof md === "string" && md.trim()) {
    render(parseChangelog(md));
  } else {
    fail(
      "Changelog data missing. Run: python3 site/sync_changelog.py"
    );
  }
})();
