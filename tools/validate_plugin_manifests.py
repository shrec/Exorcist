#!/usr/bin/env python3
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PLUGINS = ROOT / "plugins"

CANONICAL_LAYERS = {"cpp-sdk", "c-abi", "lua-jit", "dsl"}
CANONICAL_VIEW_LOCATIONS = {"sidebar-left", "sidebar-right", "bottom", "top"}


def iter_manifests():
    for path in sorted(PLUGINS.rglob("plugin.json")):
        yield path


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        return f"invalid json: {exc}"


def validate_manifest(path: Path, data):
    rel = path.relative_to(ROOT)
    errors = []
    warnings = []

    if isinstance(data, str):
        errors.append(data)
        return errors, warnings

    for key in ("id", "name", "version"):
        if not data.get(key):
            errors.append(f"missing required field '{key}'")

    plugin_kind = path.parts[1] if len(path.parts) > 1 else ""
    if plugin_kind not in {"lua", "javascript"}:
        if not data.get("author"):
            warnings.append("missing recommended field 'author'")
        if not data.get("license"):
            warnings.append("missing recommended field 'license'")
        layer = data.get("layer")
        if not layer:
            warnings.append("missing recommended field 'layer'")
        elif layer not in CANONICAL_LAYERS:
            warnings.append(
                f"non-canonical layer '{layer}' (expected one of {sorted(CANONICAL_LAYERS)})"
            )

    activation_events = data.get("activationEvents")
    if activation_events is not None and not isinstance(activation_events, list):
        errors.append("'activationEvents' must be an array")

    requested_permissions = data.get("requestedPermissions")
    if requested_permissions is not None and not isinstance(requested_permissions, list):
        errors.append("'requestedPermissions' must be an array")

    permissions = data.get("permissions")
    if permissions is not None and not isinstance(permissions, list):
        errors.append("'permissions' must be an array")

    contributions = data.get("contributions", {})
    if contributions and not isinstance(contributions, dict):
        errors.append("'contributions' must be an object")
        return errors, warnings

    for view in contributions.get("views", []):
        view_id = view.get("id", "<unknown>")
        location = view.get("location")
        if location and location not in CANONICAL_VIEW_LOCATIONS:
            warnings.append(
                f"view '{view_id}' uses non-canonical location '{location}' "
                f"(expected one of {sorted(CANONICAL_VIEW_LOCATIONS)})"
            )

    for command in contributions.get("commands", []):
        command_id = command.get("id", "<unknown>")
        if not command.get("title"):
            errors.append(f"command '{command_id}' missing 'title'")
        if plugin_kind not in {"lua", "javascript"} and not command.get("category"):
            warnings.append(f"command '{command_id}' missing recommended 'category'")

    return errors, warnings


def main():
    total_errors = 0
    total_warnings = 0

    for manifest in iter_manifests():
        errors, warnings = validate_manifest(manifest, load_json(manifest))
        if not errors and not warnings:
            continue

        print(manifest.relative_to(ROOT))
        for error in errors:
            print(f"  ERROR: {error}")
        for warning in warnings:
            print(f"  WARN: {warning}")
        total_errors += len(errors)
        total_warnings += len(warnings)

    if total_errors == 0 and total_warnings == 0:
        print("All plugin manifests passed validation.")
        return 0

    print(
        f"Validation finished with {total_errors} error(s) and {total_warnings} warning(s)."
    )
    return 1 if total_errors else 0


if __name__ == "__main__":
    sys.exit(main())
