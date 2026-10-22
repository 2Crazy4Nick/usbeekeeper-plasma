# Fedora KDE installation and RPM packaging

The `usbeekeeper` RPM contains both the Plasma applet and its native QML
plugin. No archive extraction or QML_IMPORT_PATH setting is needed.

Install and start **usbeehived separately** first. It must expose
`org.usbeehive.Devices5` on the session D-Bus. The RPM does not provide, build,
start, or modify the daemon. There is deliberately no RPM dependency on an
unverified usbeehive package name.

## Install, update, remove

The recommended method is the
[USBeeKeeper COPR](https://copr.fedorainfracloud.org/coprs/2crazy4nick/usbeekeeper/):

```bash
sudo dnf copr enable 2crazy4nick/usbeekeeper
sudo dnf install usbeekeeper
usbeekeeper-check-installation
```

Run the checker as your desktop user, without sudo. Add **USBeeKeeper** from
Plasma's Add Widgets menu. Log out and back in after upgrading a loaded native
plugin so Plasma loads the new library.

Update using your normal Fedora package updater, or run:

```bash
sudo dnf upgrade usbeekeeper
```

The regular `0.1.0` version sorts newer than `0.1.0~dev.3`. COPR-delivered
development-package installation and updates have been tested on Fedora KDE.
Build availability depends on the COPR's enabled Fedora release/architecture.

Remove it with `sudo dnf remove usbeekeeper`. The RPM owns no files in home
directories and has no install/uninstall scripts that edit user settings.
Plasma settings and physical aliases are retained.

### Standalone RPM — secondary method

If not using COPR, download an RPM built for your Fedora release and
architecture, then run `sudo dnf install ./<downloaded-usbeekeeper-rpm>.rpm`.
Use the same command with a newer RPM to upgrade. Do not install debug RPMs
unless investigating a crash. Manual archives and source installation are
also available as [advanced methods](../../README.md#advancedmanual-installation).

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

For regular release `v0.1.0`, the spec uses RPM Version `0.1.0`, Release
`1%{?dist}`. Development versions use a tilde, for example `0.1.1-dev.1`
becomes `0.1.1~dev.1`, so they sort before the corresponding regular release.
Use the release preparation tool below to maintain version fields and the
RPM changelog together.

### Prepare a release or packaging update

From a Git checkout with Python 3 and Git available:

```bash
python3 tools/release.py prepare 0.1.1-dev.1
```

`prepare` accepts canonical `X.Y.Z` and `X.Y.Z-dev.N` versions, without a `v`
prefix or leading zeroes. Development counters may start at zero. Other suffixes
and build metadata are rejected. The requested version must be greater than
the current upstream version: `dev.10` follows `dev.2`, and stable `0.1.0`
follows every `0.1.0-dev.N`.

It updates `%global upstream_version`, converts the example RPM Version to
`0.1.1~dev.1`, resets Release to `1%{?dist}`, and synchronizes the CMake project
VERSION with the numeric base (`0.1.1`). A dated English RPM changelog entry is
prepended while old entries are preserved. Stable releases omit the RPM tilde.
The applet's `KPlugin.Version` also receives the full upstream version, including
the development suffix, so Plasma settings identify the installed release.

For a packaging-only update instead:

```bash
python3 tools/release.py rpm-bump
```

`rpm-bump` increments the integer Release, preserves `%{?dist}`, and adds a
packaging-only changelog entry. It leaves the upstream version, RPM Version
and CMake file untouched. It prints no tag command, so the intended next step
is a branch push for COPR rather than a new GitHub tag release.

Both commands require a branch checkout and clean tracked target files.
`prepare` checks the spec, CMake file and applet metadata; `rpm-bump` checks only
the spec.
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

For **every** regular or development GitHub tag, add a matching release-notes
file, for example `releases/v0.1.0.md` or `releases/v0.1.1-dev.1.md`. The tag
workflow checks that `releases/<tag>.md` is non-empty before building, then
passes that file to the GitHub Release action. The Markdown is used verbatim:
write actual asset filenames and links, not GitHub Actions expressions inside
the notes. Tags containing `-` remain prereleases; `v0.1.0` is a regular release.
Review and commit the metadata, notes and any workflow/documentation changes
together before manually creating a tag. The tool's printed commands only
cover the version files; they do not automatically include release notes.
Preparing metadata does not create a tag, GitHub Release or COPR build.

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

The published project is `2crazy4nick/usbeekeeper`. Maintainers configuring or
reproducing the SCM setup can enable `fedora-43-x86_64` and
`fedora-44-x86_64` and use these package settings:

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
commit containing `.copr/Makefile`, such as `v0.1.0` after publication.
Older tags without the Makefile cannot use this
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
`qt6-rpm-macros`, `kf6-kcmutils` (needed by QML settings tests), `python3`,
and the CMake capabilities `Qt6Core`, `Qt6DBus`,
`Qt6Qml`, `Qt6Quick`, `KF6I18n`, `KF6CoreAddons`, `KF6Package` and `Plasma`.
The existing runtime dependencies and external usbeehived prerequisite remain
unchanged. COPR needs no custom dependency repository for the build itself.

The source-generation target does not change Version or Release. Use
`tools/release.py rpm-bump` for packaging-only updates or
`tools/release.py prepare <version>` for upstream changes before publishing
a changed RPM. Rebuilding a newer `main` commit with the
same RPM version/release is not a normal DNF upgrade. No automatic Git snapshot
versioning policy is introduced.

COPR project/package administration and GitHub webhook configuration remain
maintainer tasks outside this repository. No account credentials or webhook
secrets belong here. Source/build scripts do not create a project, publish a
package or configure webhooks; the existing project delivers updates through
the enabled Fedora repository.

### Rebuild and validate

If Mock is already available, test the generated source RPM with:

```bash
mock -r fedora-44-x86_64 --rebuild \
    build/copr/usbeekeeper-0.1.0-1.src.rpm \
    --resultdir "$PWD/build/mock-fedora44"
```

Use Mock to rebuild the SRPM in the desired Fedora release's clean buildroot,
or let COPR rebuild it from the SCM configuration. Each buildroot installs the
spec's declared BuildRequires and produces a distribution-specific RPM. No daemon or live USB
hardware is needed for the build/tests. An SRPM contains the complete source
required for offline rebuilds; do not submit the native binary from a different
Fedora release as a substitute. Debug RPMs may be generated separately by
Fedora's normal tooling; they are not needed to run USBeeKeeper.
