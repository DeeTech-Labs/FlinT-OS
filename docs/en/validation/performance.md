[Русский оригинал](../../ru/validation/performance.md)

# Diagnostics and performance validation

Host tests do not establish electrical behavior, power consumption or device latency. Associate measurements with OS/SDK revisions, board, configuration and scenario.

- [Tests](../development/testing.md).
- [2026-11-10](2026-11-10-optimization.md).

USB install scenario: `python tools/check_install_responsiveness.py --port <port> --output <new.json>`. Changes device test packages; close Flare and save work first.

Name new reports `YYYY-MM-DD-topic.md` and include conditions, commands, results and unverified areas. Historical maxima are not guarantees for current firmware.
