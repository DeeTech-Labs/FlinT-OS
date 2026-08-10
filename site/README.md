# Product landing (GitHub Pages)

Static site for FlinT OS. Deployed by `.github/workflows/pages.yml`.

## Local preview

```bash
python3 site/sync_changelog.py   # embed root CHANGELOG.md → changelog.data.js
# open site/index.html / site/changelog.html
```

Source of truth is the repo-root `CHANGELOG.md`. Pages only uploads `site/`,
so CI (and local sync) embed it into `changelog.data.js` — no duplicate
`site/CHANGELOG.md`, works under `file://`.
