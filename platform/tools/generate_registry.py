#!/usr/bin/env python3
"""Generate the API and permission registries from the v1 specifications."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    data = json.loads((ROOT / "spec/permissions.json").read_text(encoding="utf-8"))
    rows = []
    seen = set()
    for access, permissions in data.items():
        if access == "version":
            continue
        for name in permissions:
            if name in seen:
                raise ValueError(name)
            seen.add(name)
            cap = 8 if name == "companion.pc.metrics" else 0
            rows.append(f'    Permission{{"{name}", Access::{access}, {cap}}},')
    registry = "\n".join(
        [
            "#pragma once",
            "// Generated from spec/permissions.json; do not edit.",
            '#include "permissions.h"',
            "",
            "namespace flint {",
            "inline constexpr Permission PermissionRegistry[]{",
            *rows,
            "};",
            "",
            "inline const Permission *permission(std::string_view id) {",
            "    for (auto &entry : PermissionRegistry) {",
            "        if (entry.id == id) {",
            "            return &entry;",
            "        }",
            "    }",
            "    return nullptr;",
            "}",
            "} // namespace flint",
            "",
        ]
    )
    (ROOT / "include/flint/registry.h").write_text(registry)
    services = json.loads((ROOT / "spec/services.json").read_text(encoding="utf-8"))[
        "services"
    ]
    lines = [
        "#pragma once",
        "// Generated from spec/services.json; do not edit.",
        '#include "protocol.h"',
        "",
        "namespace flint {",
        "struct ServiceDescriptor {",
        "    Method method;",
        "    std::string_view name;",
        "    bool mutates;",
        "};",
        "",
        "inline constexpr ServiceDescriptor ServiceRegistry[]{",
    ]
    for service in services:
        name = service["name"]
        mutates = str(service["mutates"]).lower()
        lines.append(f'    ServiceDescriptor{{Method::{name}, "{name}", {mutates}}},')
    lines.extend(["};", ""])
    for service in services:
        lines.append(
            f'static_assert(uint16_t(Method::{service["name"]}) == {service["id"]});'
        )
    lines.extend(["} // namespace flint", ""])
    (ROOT / "include/flint/services.h").write_text("\n".join(lines))


if __name__ == "__main__":
    main()
