# Security Policy

## Supported versions

FlinT OS is in **alpha**. Security fixes target the latest `main` branch.

## Reporting a vulnerability

**Do not** open a public issue for security-sensitive reports.

1. **GitHub private vulnerability reporting** — **Security → Report a vulnerability** on this repository.
2. If that is unavailable, contact maintainers via the [DeeTech Labs](https://github.com/DeeTech-Labs) org on GitHub.

Include: FlinT OS commit/version, ESP-IDF version, board profile (e.g. FlinT Spark / `flint_spark`), impact, and reproduction steps. Share a minimal proof-of-concept only if needed.

## Scope notes

- Never commit `components/flint/config/secrets.h` or real credentials.
- Pure physical-access issues without a software bug may be out of scope; ask privately if unsure.
