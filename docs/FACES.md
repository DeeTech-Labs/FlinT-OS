# Watch faces and ignits

User-installable faces and themes use [Flint Ignits v1](IGNITS.md). Install
packages from the Companion Market; choose and remove them in the watch Apps menu.
Spark ships exactly one protected built-in package, Spark Base.

Native `Face` classes are firmware implementation details. The built-in registry
has one slot for Spark Base; additional installed faces are supplied by the
package registry. Adding an application does not mean appending native factories
to `faces_builtin.cpp` or growing a community board configuration.

The render layouts `digital`, `blocks` and `quiet` are shared engines for installed
package descriptions. The native Face SDK remains available for development of
the base firmware and new bounded render engines; packages do not load native code.

Also: [ARCHITECTURE.md](ARCHITECTURE.md) · SDK `components/flint/sdk/flint_face_sdk.h`.

## Dirty / paint contract

Shell paints through events → dirty bits → `onTick` or `onForceRedraw` (details: [ARCHITECTURE.md](ARCHITECTURE.md)).

| Situation | Face entry | What to do |
|-----------|------------|------------|
| Day mode / first show / `TIME_VALID` / explicit force | `onForceRedraw` | Full canvas |
| Prefs / i18n (`UI_DIRTY_SETTINGS`) | `onTick` | Soft — rebuild layout (often full paint from `onTick`; **no** paired FORCE) |
| Second / minute / motion | `onTick` | Incremental bands when possible |

Built-in reference: `face_digital.cpp` (partial present + colon pulse).

## Rules

- Include the Face SDK (and allowed helpers such as `faces/glance.h`, `display/ui_tokens.h`) only — not `usb/`, `settings/`, or `app/`
- Keep `onTick` non-blocking
- Prefer `FaceContext` prefs/palette over reading services
- Prefer `UiTok::timeY(screenH)` / mapped helpers over raw `kTimeY` constants
- Use `flintFont` / `flintDrawIcon` / `flintTr` — do not vendor fonts or bake UI chrome into bitmaps
- Localized strings: `flintTr(FlintStr::…)`, weekday / day-mode helpers as in the template
- `GLANCE_DATE` draws the shared date complication

## API locations

- `components/flint/sdk/flint_face_sdk.h` — umbrella include (`UI_DIRTY_*` included)
- `components/flint/faces/Face.h`, `FaceContext.h`, `FaceRegistry.h`
- Examples: `components/flint/faces/face_digital.cpp`
- Versions: `components/flint/config/flint_version.h`
