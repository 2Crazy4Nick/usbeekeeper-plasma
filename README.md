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

**v0.1.0 is the first regular release.** It promotes the functionality tested
in the installed Fedora COPR `0.1.0-dev.3` packages, including device details,
detail-level settings, renaming controls, hotplug and package updates. See the
[v0.1.0 release notes](releases/v0.1.0.md).

USBeeKeeper remains a small, actively developed USB inspector. Available data
depends on usbeehive and the hardware; a regular release does not imply full
USB/Type-C diagnostics or comprehensive physical-device identification.

## Install on Fedora KDE — recommended

Install and start `usbeehived` separately. Then enable the
[USBeeKeeper COPR](https://copr.fedorainfracloud.org/coprs/2crazy4nick/usbeekeeper/)
and install the package:

```bash
sudo dnf copr enable 2crazy4nick/usbeekeeper
sudo dnf install usbeekeeper
usbeekeeper-check-installation
```

The RPM installs both the applet and native QML plugin together; no manual
extraction or QML import-path setup is needed. Run the conflict checker without
sudo, then add USBeeKeeper through Plasma's **Add Widgets** menu.

Update through your normal Fedora package updater or:

```bash
sudo dnf upgrade usbeekeeper
```

Log out and back in after upgrading a loaded native plugin. The regular
`0.1.0` RPM sorts newer than `0.1.0~dev.3`; upgrades preserve widget settings
and physical aliases. The separate daemon must provide
`org.usbeehive.Devices5` on the session D-Bus. USBeeKeeper does not bundle,
start, build or modify it.

See [Fedora installation and RPM builds](packaging/fedora/INSTALL.md) for local
copy conflicts, uninstall behavior, standalone RPMs and maintainer COPR setup.
Other installation methods are [advanced/manual](#advancedmanual-installation).

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

- USB identity, descriptor, kernel driver, declared current and negotiated link
  information. Declared USB current in mA is **not live power consumption**.
- Standard/Extended/Debug detail levels, with wrapped technical information
  and no repeated USB version/link-speed rows already present in the header.
- Data-rate capability summaries and warnings supplied by usbeehive.
- Verified USB 2/USB 3 companion-hub grouping.
- Optional depth-first USB topology tree, with logical port information in
  Debug details.
- Declarative physical-device profiles for evidence-based compositions.
- Integrated-function annotations without hiding enumerated USB devices.
- Persistent user-defined names for physical devices when a usable instance
  identity is available.
- Per-widget Plasma settings for hubs, tree view, detail levels, empty USB-C
  ports where available, and optional physical-device renaming controls.
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
sudo dnf install git gcc-c++ cmake extra-cmake-modules \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    kf6-kirigami-devel kf6-ki18n-devel kf6-kcoreaddons-devel \
    kf6-kpackage-devel kf6-kcmutils libplasma-devel plasma-workspace-devel \
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

## Advanced/manual installation

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

## Plasma settings

- **Show USB hubs** defaults to off. Enable it to include hubs.
- **Show as topology tree** is available when hubs are shown; otherwise the
  list remains flat.
- **Device information detail level** defaults to **Standard**: VID:PID,
  kernel driver and declared max USB current. **Extended** adds available
  manufacturer/product, class/subclass, serial, power source, distinct capable
  speed and Alternate Mode information. **Debug** adds database, logical-port,
  companion, hub and other low-level details.
- **Show physical device renaming controls** defaults to off. Enable it to
  name, rename or clear a physical alias. Hiding the controls preserves saved
  aliases and their display; it does not hide Integrated USB device cards.
- **Show empty USB-C ports** only exposes entries available in the snapshot.

Settings are per widget. Applying detail/renaming settings updates expanded
cards without restarting the daemon. USB version remains accessible in
Extended/Debug when it is not in the header, including companion-function
versions. Negotiated link speeds are not repeated in the expanded tables.

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
- Installed Fedora COPR operation and updates have been tested. Systematic
  alias-persistence testing across installed Plasma session restarts remains
  limited; `plasmoidviewer` configuration is transient.
- Tests cover profile validation/matching, model and request sequencing,
  General settings, and real-plugin QML delegates/layouts. There is no complete
  fake D-Bus service integration harness for daemon owner transitions.
- Physical profiles describe known structures conservatively and do not imply
  a retail product identity unless the available evidence supports one.
- The legacy CLI and Type-C/sysfs implementation are separate from the
  usbeehive plasmoid and are not presented as fully validated current
  functionality.
- Review Debug details and screenshots before sharing: they can contain
  serial numbers or other unique hardware identifiers.

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
