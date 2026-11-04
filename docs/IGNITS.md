# Ignite packages

Ignite is the universal binary package format for apps, watch faces, themes, locales, resources and widgets. Factory and user components use the same validator and Runtime.

## Build a package

```bash
python3 -m pip install Pillow
python3 platform/tools/compile_ignite.py \
    platform/examples/pc-monitor/project.json /tmp/pc-monitor.ignit
```

A project declares its ID, publisher, component sections, memory budgets and permission scopes. Its UI, resources and manifest are compiled on the computer. New packages are installed through Flare without changing firmware source.

## Installation and updates

Flare uses resumable binary blocks, a complete-file digest, staging validation and an atomic registry commit. The previous version remains available for rollback. There are no fixed installation slots. User packages cannot replace protected factory identities or inherit additional consent on update.

## Runtime and limits

Declarative screens support shared fonts, package glyph atlases, images, localized text and permitted data bindings. Lua is the application language. The bounded runtime runs in SDK/simulator builds and in the physically validated Spark firmware configuration. Background services are unsupported. The complete [platform contract](../platform/README.md) defines binary layouts, security and resource limits.
