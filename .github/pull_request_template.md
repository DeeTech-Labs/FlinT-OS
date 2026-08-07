## Summary

<!-- What changed and why (1–3 bullets). Use FlinT OS / FlinT Spark / DeeTech Labs naming. -->

-

## Test plan

- [ ] `idf.py set-target esp32c3` + build succeeds (FlinT Spark / `flint_spark` unless noted)
- [ ] Flash + serial monitor smoke check (face boots, time/Wi‑Fi as relevant)
- [ ] Exercised changed UI paths (serial keys: `s` / `p`/`n` / `+`/`-` / Enter / `q`)
- [ ] If strings changed: i18n / `.po` updated as needed
- [ ] If flash layout changed: `config/partitions.csv` reviewed (dual OTA + `storage` + `cfg`; full flash after partition edits)
- [ ] No `secrets.h` or credentials in the diff

## Notes

<!-- Breaking API, NVS/cfg migration, board-specific caveats — or "none". -->
