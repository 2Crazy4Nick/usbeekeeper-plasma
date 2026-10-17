# USBeeKeeper — Agent Guidelines

## Project overview

USBeeKeeper is a KDE Plasma 6 plasmoid backed by `usbeehived`. The repository
also contains the separate legacy WhatCable-Linux sysfs-based CLI. It is
derived from
[WhatCable](https://github.com/darrylmorley/whatcable) by Darryl Morley.

Do not assume the plasmoid and CLI share a hardware backend. The current
plasmoid must use usbeehive exclusively.

## Current architecture

### Plasma widget

```text
QML
  -> DeviceModel (QAbstractListModel)
  -> UsbeehiveClient (Qt 6 D-Bus)
  -> org.usbeehive.Devices5
  -> usbeehived
```

- `src/plasmoid/plugin/UsbeehiveClient.*` watches the session-bus owner,
  requests `SnapshotJson()`, calls `Refresh()`, and reacts to device signals.
- `src/plasmoid/plugin/DeviceModel.*` parses snapshots, preserves raw USB
  identity, groups verified companion hubs, resolves logical topology, applies
  physical profiles/aliases, and exposes presentation roles to QML.
- `src/plasmoid/plugin/PhysicalDeviceProfile.*` loads and matches declarative
  profiles from `src/plasmoid/profiles/`.
- `src/plasmoid/contents/ui/` contains the Plasma UI.

The plasmoid does not read sysfs, use libudev, or poll. Do not reintroduce a
direct hardware backend into it. `usbeehived` is required at runtime.

### Legacy core and CLI

- `src/core/` is the original Linux Qt Core library. It reads sysfs, decodes
  USB Power Delivery data, and uses libudev for hotplug monitoring.
- `src/cli/` builds `whatcable-linux` on that legacy core.

The legacy code remains buildable but is independent of the plasmoid and is
not the primary validated path. Changes to it should not be inferred from
plasmoid work.

## Model principles

Keep these layers distinct:

1. Raw USB identity and logical paths from the daemon snapshot.
2. Presentation identity, companion grouping, filtering, and topology order.
3. Optional physical profiles, relationships, instance keys, and user aliases.

Physical metadata augments topology; it must not overwrite raw identity or
hide real enumerated USB devices. `integrated_function` is evidence-based.
Absence of an Integrated match never means External.

Companion hubs may be grouped only by the existing verified criteria. Do not
infer physical products from display names, VID/PID alone, list order, or a
child's mere presence beneath a hub.

## Code conventions

- C++20 and Qt 6 style; use `QStringLiteral()` for C++ string literals.
- Preserve the attribution header on source derived from the original Swift
  project:
  `// Derived from WhatCable by Darryl Morley (https://github.com/darrylmorley/whatcable)`
- Missing daemon fields and optional JSON values must fail safely.
- D-Bus failures and malformed snapshots must not be interpreted as a valid
  empty device list.
- Do not add polling; use daemon owner/device signals.
- Keep user aliases separate from logical display identity and use only
  conservative instance keys.
- Preserve unrelated user changes in a dirty worktree.

For legacy core changes only, sysfs access goes through `SysfsReader`; never
read `/sys/` with ad hoc file I/O.

## Physical profiles

Profiles live in `src/plasmoid/profiles/`, one profile per JSON file.
`schema.json` is documentation/schema data, not a hardware profile.

Adding supported hardware should normally require only a JSON file. Do not add
profile IDs, VID/PIDs, or profile-specific branches to C++. A profile may mark
a relationship Integrated only when its complete required signature matches.

Validate profiles with:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/validate_profiles.py
```

See `CONTRIBUTING_PROFILES.md` for contributor rules.

## Build and test

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/validate_profiles.py
env CCACHE_DISABLE=1 cmake -B build
env CCACHE_DISABLE=1 cmake --build build -j2
ctest --test-dir build --output-on-failure
git diff --check
```

The plasmoid is built only when its Qt 6, KDE Frameworks 6, and Plasma
development dependencies are available. The complete repository also requires
libudev for the legacy core/CLI.

Development output:

- `build/pkg/io.github._2crazy4nick.usbeekeeper/` — applet package.
- `build/qml/io/github/_2crazy4nick/usbeekeeper/` — required QML plugin/module.

With `usbeehived` running:

```bash
QML_IMPORT_PATH="$PWD/build/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}" \
plasmoidviewer -a "$PWD/build/pkg/io.github._2crazy4nick.usbeekeeper"
```

Do not use plasmoidviewer restart behavior to judge configuration persistence;
its applet instance is transient.

## Installation and packaging

The applet package and C++ QML module are both required. CMake installs through
KDE/Qt install variables. Release plugin archives are rooted at
`io/github/_2crazy4nick/usbeekeeper/` relative to the QML import root reported
by:

```bash
qtpaths6 --query QT_INSTALL_QML
```

For user-local applet testing:

```bash
kpackagetool6 -t Plasma/Applet -i io.github._2crazy4nick.usbeekeeper
```

A user-local QML module directory is not discovered automatically; it must be
provided to the loading Plasma process through `QML_IMPORT_PATH`.

## Key files

| File | Purpose |
|---|---|
| `src/plasmoid/plugin/UsbeehiveClient.h/.cpp` | D-Bus lifecycle, snapshots, and refresh |
| `src/plasmoid/plugin/DeviceModel.h/.cpp` | Snapshot parsing and QML-facing model |
| `src/plasmoid/plugin/PhysicalDeviceProfile.h/.cpp` | Generic profile loader and matcher |
| `src/plasmoid/profiles/` | Declarative physical profiles and schema |
| `src/plasmoid/contents/ui/main.qml` | Plasmoid entry and configuration bindings |
| `src/plasmoid/contents/ui/DeviceCard.qml` | Device card and topology guides |
| `tools/validate_profiles.py` | Contributor profile validator |
| `tests/` | Validator, matcher, and model regression tests |
| `src/core/` | Legacy sysfs/libudev core |
| `src/cli/` | Legacy command-line frontend |
