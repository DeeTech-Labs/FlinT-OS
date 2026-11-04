# PC Monitor example

Compile `project.json` with `platform/tools/compile_ignite.py`, then install the resulting binary in Ignite Hub. No firmware source changes or flashing are needed.

The screen declares `companion.pc.metrics` scoped to `cpu,ram`. Grant that permission in Ignite Hub, launch the component, and enable Flare's independent CPU/RAM policy. Denial, revocation, disconnect and expired samples render the unavailable binding instead of exposing cached data.

The example is not a privileged factory package.
