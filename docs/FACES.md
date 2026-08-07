# Add a face

Pluggable faces live under `components/flint/faces/`. Do not edit core services to ship a theme.

Also: [ARCHITECTURE.md](ARCHITECTURE.md) · Face SDK `components/flint/sdk/flint_face_sdk.h` · template `community/face_template.cpp.example`.

## Steps

1. Copy `community/face_template.cpp.example` → `components/flint/faces/face_yourname.cpp`
2. Implement `Face`, export `Face* createYourFace()`, set `FaceInfo` (`id`, name, author, versions, `minApi` ≤ `FLINT_OS_API_VERSION`)
3. Register in `components/flint/faces/faces_builtin.cpp` (`FaceRegistry::add(createYourFace)`)
4. Build & flash; pick via serial face picker (`Enter` from the face), or rely on `ACTIVE_FACE_ID` when `cfg` is empty

## Rules

- Include the Face SDK (and allowed helpers such as `faces/glance.h`, `display/ui_tokens.h`) only — not `net/`, `settings/`, `app/`, or `secrets.h`
- Keep `onTick` non-blocking
- Prefer `FaceContext` prefs/palette over reading services
- Use `flintFont` / `flintDrawIcon` / `flintTr` — do not vendor fonts or bake UI chrome into bitmaps
- Localized strings: `flintTr(FlintStr::…)`, weekday / day-mode helpers as in the template

## API locations

- `components/flint/sdk/flint_face_sdk.h` — umbrella include
- `components/flint/faces/Face.h`, `FaceContext.h`, `FaceRegistry.h`
- Example: `components/flint/faces/face_digital.cpp`
- Versions: `components/flint/config/flint_version.h`
