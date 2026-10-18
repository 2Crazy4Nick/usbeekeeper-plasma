# USBeeKeeper

[USBeeKeeper](https://github.com/2Crazy4Nick/usbeekeeper-plasma) is a KDE
Plasma 6 USB inspection and topology widget powered by usbeehive. It displays
USB devices, negotiated link speeds, hub topology, and selected
physical-device relationships through the already-running `usbeehived`
service.

The codebase originated as
[WhatCable-Linux](https://github.com/Zetaphor/whatcable-linux), a Linux port of
[WhatCable](https://github.com/darrylmorley/whatcable), the macOS application by
[Darryl Morley](https://github.com/darrylmorley). The repository still contains
that historical Linux sysfs-based core and CLI. The supported Plasma product is
now USBeeKeeper and no longer uses that backend.

## Status

This is an active development project and an initial public development
baseline, not a stable release. The usbeehive-backed plasmoid has been tested
with live USB devices and hotplug events, but interfaces, profiles, and UI
details may still change.

## Architecture

```text
QML
  -> DeviceModel (QAbstractListModel)
  -> Qt 6 D-Bus client
  -> org.usbeehive.Devices5
  -> usbeehived
```

The plasmoid calls `SnapshotJson()` for complete snapshots and listens for the
daemon's device-change signals. It does not poll and does not access sysfs or
libudev directly. `usbeehived` must be running on the session bus as
`org.usbeehive.Devices`.

The model deliberately keeps three concepts separate:

1. Raw USB identity and logical topology from usbeehive.
2. Presentation identity, companion-hub grouping, and tree metadata.
3. Optional physical-device profiles, relationships, and user aliases.

Physical metadata augments the logical USB topology; it does not replace it.
An integrated USB function remains visible as a real enumerated USB device with
its own identity and diagnostics. A device with no classified physical
relationship is unclassified—it is not automatically considered external.

## Features

- USB identity, descriptor, driver, power, and negotiated link information.
- Data-rate capability summaries and warnings supplied by usbeehive.
- Verified USB 2/USB 3 companion-hub grouping.
- Optional depth-first USB topology tree with logical port annotations.
- Declarative physical-device profiles for evidence-based compositions.
- Integrated-function annotations without hiding enumerated USB devices.
- Persistent user-defined names for physical devices when a usable instance
  identity is available.
- Manual refresh and automatic refresh through D-Bus change signals; no
  periodic polling.

## Requirements

Runtime requirements for the plasmoid are:

- KDE Plasma 6
- Qt 6 Core, DBus, Quick, and QML
- KDE Frameworks 6 and Kirigami
- A running `usbeehived` exposing `org.usbeehive.Devices5`
- The USBeeKeeper C++ QML module as well as the Plasma applet package

Building the complete repository additionally requires a C++20 compiler,
CMake, Extra CMake Modules, Plasma/KPackage development files, Python 3 for
validation, and libudev development files for the legacy core/CLI.

Example Fedora development dependencies:

```bash
sudo dnf install gcc-c++ cmake extra-cmake-modules \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
    kf6-kpackage-devel libplasma-devel plasma-workspace-devel \
    systemd-devel python3
```

Package names differ between distributions.

## Build and test

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/validate_profiles.py
env CCACHE_DISABLE=1 cmake -B build
env CCACHE_DISABLE=1 cmake --build build -j2
ctest --test-dir build --output-on-failure
git diff --check
```

The build creates:

- `build/pkg/io.github._2crazy4nick.usbeekeeper/` — a
  `kpackagetool6`-installable applet.
- `build/qml/io/github/_2crazy4nick/usbeekeeper/` — the required C++ QML
  module.
- `build/src/cli/whatcable-linux` — the separate legacy CLI.

With `usbeehived` running, launch the development package with:

```bash
QML_IMPORT_PATH="$PWD/build/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}" \
plasmoidviewer -a "$PWD/build/pkg/io.github._2crazy4nick.usbeekeeper"
```

`plasmoidviewer` creates a transient applet instance. Its configuration is not
a valid test of persistence across host restarts; alias persistence must also
be tested with an installed Plasma applet instance.

## Installation

### Fedora RPM

The `usbeekeeper` RPM installs both the applet and native QML plugin together:

```bash
sudo dnf install ./usbeekeeper-<version>.<architecture>.rpm
usbeekeeper-check-installation
```

Install and run `usbeehived` separately; it must provide
`org.usbeehive.Devices5` on the session D-Bus. Run the conflict checker without
sudo, then add USBeeKeeper through Plasma's Add Widgets menu. Install a newer
RPM with the same DNF command to upgrade; log out and back in to reload a
previously loaded plugin. No COPR repository is published yet.

See [Fedora installation and RPM builds](packaging/fedora/INSTALL.md) for local
copy conflicts, uninstall behavior, and future COPR preparation.

Both runtime pieces are required:

1. The Plasma applet package.
2. The `io.github._2crazy4nick.usbeekeeper` C++ QML module.

Installing only the applet produces a widget that cannot import `DeviceModel`.

### CMake system installation

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

On the Fedora build environment used by this project, that prefix produces:

```text
/usr/share/plasma/plasmoids/io.github._2crazy4nick.usbeekeeper
/usr/lib64/qt6/qml/io/github/_2crazy4nick/usbeekeeper
```

`/usr/lib64/qt6/qml` is not portable across distributions. Query the active
Qt installation instead:

```bash
qtpaths6 --query QT_INSTALL_QML
```

CMake uses KDE/Qt install variables and may select a different QML module root
on another distribution or with another prefix.

### Release archives

The plasmoid archive contains `io.github._2crazy4nick.usbeekeeper/`. The plugin
archive contains `io/github/_2crazy4nick/usbeekeeper/` relative to the Qt QML
import root.

```bash
qt_qml_dir="$(qtpaths6 --query QT_INSTALL_QML)"
sudo tar xzf usbeekeeper-qml-plugin-*-x86_64.tar.gz -C "$qt_qml_dir"

tar xzf usbeekeeper-plasmoid-*.tar.gz
kpackagetool6 -t Plasma/Applet -i io.github._2crazy4nick.usbeekeeper
```

Use `kpackagetool6 -t Plasma/Applet -u io.github._2crazy4nick.usbeekeeper` when
upgrading an already installed applet package.

### User-local development installation

`kpackagetool6` installs an applet for the current user unless `--global` is
specified:

```bash
kpackagetool6 -t Plasma/Applet -i io.github._2crazy4nick.usbeekeeper
```

The QML module may be extracted beneath a user-controlled import root, for
example `~/.local/lib64/qml/io/github/_2crazy4nick/usbeekeeper`, but Qt does
not automatically discover that directory. `QML_IMPORT_PATH` must be present
in the environment of the process loading the applet:

```bash
user_qml_root="$HOME/.local/lib64/qml"
mkdir -p "$user_qml_root"
tar xzf usbeekeeper-qml-plugin-*-x86_64.tar.gz -C "$user_qml_root"

QML_IMPORT_PATH="$user_qml_root${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}" \
plasmoidviewer -a io.github._2crazy4nick.usbeekeeper
```

For a persistent desktop installation, the same import path must be exported
to the Plasma session before `plasmashell` starts. Exporting it only in a later
terminal does not affect the running shell.

## Physical-device profiles

Profiles are strict, versioned JSON files in `src/plasmoid/profiles/`, with one
physical layout per file. Adding supported hardware should normally require
only one new JSON file—no C++ or per-profile CMake edit.

`schema.json` documents the format and is bundled as a resource, but it is not
a hardware profile. Profiles may classify a relationship only when explicit
hardware or structural evidence supports it. Not matching a profile does not
mean a child is external.

To contribute a profile:

1. Copy a similar hardware profile and assign a unique ID.
2. Describe the root and only relationships supported by evidence.
3. Run `python3 tools/validate_profiles.py`.
4. Configure and build the repository.
5. Run `ctest --test-dir build --output-on-failure`.
6. Submit the new JSON file in a pull request.

See [CONTRIBUTING_PROFILES.md](CONTRIBUTING_PROFILES.md) for the format and
contribution rules. Manual user relationship overrides do not exist yet.

## Known limitations

- Hardware without profile or direct hardware evidence remains relationship-
  unclassified. Unclassified does not mean external.
- Physical alias portability depends on the stable instance identity exposed
  by the device. Serial and container identities can survive relocation;
  topology fallback aliases are tied to an attachment location and are
  explicitly nonportable.
- Alias persistence has been exercised in development, but persistence across
  restarts still needs verification in a real installed Plasma session;
  `plasmoidviewer` configuration is transient.
- Tests cover model and request-sequencing behavior, but there is no complete
  fake D-Bus service integration harness for daemon owner transitions.
- Physical profiles describe known structures conservatively and do not imply
  a retail product identity unless the available evidence supports one.
- The legacy CLI and Type-C/sysfs implementation are separate from the
  usbeehive plasmoid and are not presented as fully validated current
  functionality.

## Legacy CLI

`whatcable-linux` is still built from `src/core/` and `src/cli/`. It reads
Linux sysfs directly and uses libudev for hotplug monitoring. This code path is
independent of the plasmoid's usbeehive backend and currently remains primarily
as legacy functionality.

```bash
./build/src/cli/whatcable-linux --help
```

## Roadmap

- Manual local Integrated relationship overrides with an Automatic fallback.
- More evidence-based community hardware profiles.
- Further UI and diagnostic improvements.
- Broader installed-session and D-Bus lifecycle integration testing.

## License and attribution

USBeeKeeper is licensed under the [MIT License](LICENSE).

USBeeKeeper retains the original project's attribution. This codebase is
derived from
[WhatCable](https://github.com/darrylmorley/whatcable) by
[Darryl Morley](https://github.com/darrylmorley). Source files ported from the
original Swift implementation retain attribution headers, and the original
project's approach to USB Power Delivery decoding and plain-language
diagnostics remains credited under the MIT license.
