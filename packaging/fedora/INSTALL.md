# Fedora RPM

The `usbeekeeper` RPM contains both the Plasma applet and its native QML
plugin. No archive extraction or QML_IMPORT_PATH setting is needed.

Install and start **usbeehived separately** first. It must expose
`org.usbeehive.Devices5` on the session D-Bus. The RPM does not provide, build,
start, or modify the daemon. There is deliberately no RPM dependency on an
unverified usbeehive package name.

## Install, update, remove

Download an RPM built for your Fedora release and architecture, then run:

```bash
sudo dnf install ./usbeekeeper-*.rpm
usbeekeeper-check-installation
```

Run the checker as your desktop user, without sudo. Add **USBeeKeeper** from
Plasma's Add Widgets menu. Log out and back in after upgrading a loaded native
plugin so Plasma loads the new library.

For a newer downloaded RPM, use the same `sudo dnf install ./<new-rpm>.rpm`
command. Once an actual COPR repository is available and enabled, normal
`sudo dnf upgrade usbeekeeper` will update the package. No COPR repository is
created or assumed by this packaging.

Remove it with `sudo dnf remove usbeekeeper`. The RPM owns no files in home
directories and has no install/uninstall scripts that edit user settings.
Plasma settings and physical aliases are retained.

## Previous manual installations

A user-local applet with the same ID can shadow the system copy. A native
module in the Plasma session's QML_IMPORT_PATH can also override the RPM's
plugin. The checker reports the current user's applet directory, current
QML_IMPORT_PATH/QML2_IMPORT_PATH, and the previously documented local module
locations. It also reports separate legacy WhatCable copies. It cannot inspect
every custom installation path or another process's environment.

If a same-ID user applet is reported, move that **applet directory** to a backup
outside `plasma/plasmoids`, or use kpackagetool6 to remove only that local
package. If a manual module is reported, back up the obsolete module outside
all QML import roots and remove the obsolete import-path setting from the
Plasma session environment. Review each path yourself; the checker never
deletes anything. Do not delete Plasma's configuration files or alias data.
For a previous manual system install at the RPM's paths, compare file ownership
with `rpm -qf <path>` and back up unowned copies before installation.

## Build and COPR preparation

The root `usbeekeeper.spec` is an ordinary Fedora spec. It uses Fedora's CMake
and Qt 6 RPM macros; no fixed `/usr/lib64` path is assumed. On x86_64 it installs:

```text
/usr/share/plasma/plasmoids/io.github._2crazy4nick.usbeekeeper/
/usr/lib64/qt6/qml/io/github/_2crazy4nick/usbeekeeper/
/usr/bin/usbeekeeper-check-installation
```

The legacy core/CLI is disabled with `BUILD_LEGACY_CLI=OFF`. Other builds keep
their existing default and the existing release archives are unaffected.
The profile validator and CTest suite run during the RPM build.

For the initial development release, the spec maps Git tag `v0.1.0-dev.1` to
RPM Version `0.1.0~dev.1`, Release `1%{?dist}`. The tilde makes the development
version older than stable `0.1.0`. Increment Release for packaging-only changes;
update Version, upstream_version and Release for new upstream releases.

With Fedora build tools/dependencies installed, a committed checkout can
produce a source RPM without fetching an unpublished tag:

```bash
rpm_root="$PWD/build/rpm"
mkdir -p "$rpm_root/SOURCES" "$rpm_root/SPECS"
git archive --format=tar --prefix=usbeekeeper-plasma-0.1.0-dev.1/ HEAD \
    | gzip -n > "$rpm_root/SOURCES/usbeekeeper-0.1.0-dev.1.tar.gz"
rpmbuild -bs --define "_topdir $rpm_root" usbeekeeper.spec
```

`git archive HEAD` includes committed files only. For a published tag, Source0
also supplies the corresponding GitHub archive URL. The tag must exist before
using that URL; the spec does not create it.

Use Mock to rebuild the SRPM in the desired Fedora release's clean buildroot,
or submit the SRPM to COPR later. Each buildroot installs the spec's declared
BuildRequires and produces a distribution-specific RPM. No daemon or live USB
hardware is needed for the build/tests. An SRPM contains the complete source
required for offline rebuilds; do not submit the native binary from a different
Fedora release as a substitute. Debug RPMs may be generated separately by
Fedora's normal tooling; they are not needed to run USBeeKeeper.
