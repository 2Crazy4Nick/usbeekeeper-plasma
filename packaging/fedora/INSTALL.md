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

## Build and COPR SCM setup

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
version older than stable `0.1.0`. Use the release preparation tool below to
maintain the version fields and RPM changelog together.

### Prepare a release or packaging update

From a Git checkout with Python 3 and Git available:

```bash
python3 tools/release.py prepare 0.1.0-dev.2
```

`prepare` accepts canonical `X.Y.Z` and `X.Y.Z-dev.N` versions, without a `v`
prefix or leading zeroes. Development counters may start at zero. Other suffixes
and build metadata are rejected. The requested version must be greater than
the current upstream version: `dev.10` follows `dev.2`, and stable `0.1.0`
follows every `0.1.0-dev.N`.

It updates `%global upstream_version`, converts the RPM Version to
`0.1.0~dev.2`, resets Release to `1%{?dist}`, and synchronizes the CMake project
VERSION with the numeric base (`0.1.0`). A dated English RPM changelog entry is
prepended while old entries are preserved. Stable releases omit the RPM tilde.

For a packaging-only update instead:

```bash
python3 tools/release.py rpm-bump
```

`rpm-bump` increments the integer Release, preserves `%{?dist}`, and adds a
packaging-only changelog entry. It leaves the upstream version, RPM Version
and CMake file untouched. It prints no tag command, so the intended next step
is a branch push for COPR rather than a new GitHub tag release.

Both commands require a branch checkout and clean tracked target files.
`prepare` checks the spec and CMake file; `rpm-bump` checks only the spec.
Unrelated staged or unstaged work is allowed and left alone. Invalid,
non-increasing and duplicate upstream versions, existing local release tags,
inconsistent version fields, and unexpected packaging layouts fail before any
target is written. A second invocation before committing the first update
refuses the now-dirty target files. Each successful run stages complete file
contents before replacing targets and retains backups until replacement
succeeds; ordinary write failures roll back completed replacements.

The tool checks the existing GitHub `v*`/prerelease and COPR archive-naming
contracts locally. It does not contact GitHub/COPR or check remote-only tags.
It prints a summary and exact review, commit, push, and (for `prepare`) tag
commands, but runs none of them. Suggested commits use `--only` to avoid
including unrelated staged changes. Review the diff and remote state before
executing those commands. No application behavior or daemon is changed, and
no external Python dependencies are needed.

Tests are discovered by the existing CTest Python suite, or run them directly:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_release.py
```

### Local source generation

With `make`, `git`, `rpm-build`, `tar` and `gzip` installed, a committed
checkout can produce a source RPM without fetching an unpublished tag:

```bash
make -f .copr/Makefile srpm outdir="$PWD/build/copr" spec=usbeekeeper.spec
```

The target accepts COPR's `outdir` and `spec` parameters. Relative spec paths
are resolved from the repository root; absolute spec paths also work. `outdir`
may be relative or absolute, and only the resulting SRPM is written there.
All intermediate files use a temporary directory that is removed on exit.

The source filename is obtained from the spec with `rpmspec`. Its contents
come from `git archive HEAD`, so only the checked-out commit's files are
included: no `.git`, untracked build output, local credentials, or uncommitted
source changes. Commit the intended changes before requesting a COPR build.
Do not run the local target with sudo. No compiler, Qt/KDE development packages,
daemon, or live USB hardware is needed just to generate the SRPM.

For SCM builds, Source0 remains the release archive URL for conventional RPM
tooling, but the target supplies an archive of the selected commit locally.
It never downloads that URL or requires its tag to exist. The source RPM is
generated without the source builder's Fedora dist suffix; each binary build
gets its own target Fedora suffix from the unchanged spec.

### Exact COPR package settings

Create a COPR project manually and enable `fedora-43-x86_64` and
`fedora-44-x86_64`. Add an SCM package with these settings:

| Setting | Value |
|---|---|
| Package name | `usbeekeeper` |
| Source type | SCM |
| SCM type | Git |
| Clone URL | `https://github.com/2Crazy4Nick/usbeekeeper-plasma.git` |
| Committish | `main` |
| Subdirectory | Leave empty (repository root) |
| Spec File | `usbeekeeper.spec` |
| SRPM build method | `make srpm` |

For a reproducible release build, replace `main` with a published tag or
commit containing `.copr/Makefile`, such as `v0.1.0-dev.1` once that tag exists
and includes this packaging. Older tags without the Makefile cannot use this
method. The repository and selected commit must be publicly readable by COPR.
After these changes have been committed and pushed by a maintainer, request a
build in the COPR UI; no manual SRPM upload is required.

COPR invokes the target as described in its
[SCM documentation](https://docs.copr.fedorainfracloud.org/user_documentation.html#scm).
Its source-generation stage runs as root in a disposable chroot. If the small
source toolset is missing, the target installs `git`, `rpm-build`, `tar` and
`gzip` with DNF there. `make` and Bash are needed to invoke the target. There
is no need to add the full Qt/KDE toolchain to the SCM source environment.

Binary-build dependencies are resolved from `usbeekeeper.spec` in each target
buildroot: `gcc-c++`, `cmake`, `cmake-rpm-macros`, `extra-cmake-modules`,
`qt6-rpm-macros`, `python3`, and the CMake capabilities `Qt6Core`, `Qt6DBus`,
`Qt6Qml`, `Qt6Quick`, `KF6I18n`, `KF6CoreAddons`, `KF6Package` and `Plasma`.
The existing runtime dependencies and external usbeehived prerequisite remain
unchanged. COPR needs no custom dependency repository for the build itself.

The source-generation target does not change Version or Release. Use
`tools/release.py rpm-bump` for packaging-only updates or
`tools/release.py prepare <version>` for upstream changes before publishing
a changed RPM. Rebuilding a newer `main` commit with the
same RPM version/release is not a normal DNF upgrade. No automatic Git snapshot
versioning policy is introduced.

COPR project creation, package configuration, build requests and any optional
webhook setup are manual. No account credentials or webhook secrets belong in
this repository, and this packaging does not create or publish a COPR project.

### Rebuild and validate

If Mock is already available, test the generated source RPM with:

```bash
mock -r fedora-44-x86_64 --rebuild \
    build/copr/usbeekeeper-0.1.0~dev.1-1.src.rpm \
    --resultdir "$PWD/build/mock-fedora44"
```

Use Mock to rebuild the SRPM in the desired Fedora release's clean buildroot,
or let COPR rebuild it from the SCM configuration. Each buildroot installs the
spec's declared BuildRequires and produces a distribution-specific RPM. No daemon or live USB
hardware is needed for the build/tests. An SRPM contains the complete source
required for offline rebuilds; do not submit the native binary from a different
Fedora release as a substitute. Debug RPMs may be generated separately by
Fedora's normal tooling; they are not needed to run USBeeKeeper.
