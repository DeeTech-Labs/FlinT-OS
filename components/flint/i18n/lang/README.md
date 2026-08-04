# FlinT OS localization

Authoring lives here, next to firmware:

| File | Role |
|------|------|
| `flint.pot` | Template of English `msgid` keys |
| `en_US/flint.po` / `ru_RU/flint.po` | Translations (`msgstr`) |
| `*/lang_map.json` | Locale descriptor (points at `.po`) |
| `lang_index.json` | List of locales ↔ settings `lang` id |

Runtime: `flintTr(FlintStr::…)` in `i18n/i18n.h` (also via Face SDK).

After editing `.po` / `.pot`:

```bash
python3 tools/gen_i18n_catalog.py
```

This regenerates `i18n_catalog.inc` (compiled into the firmware).
