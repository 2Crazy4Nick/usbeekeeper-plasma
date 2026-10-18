#!/usr/bin/env python3
"""Prepare USBeeKeeper release metadata; never commit or publish anything."""

import argparse
from dataclasses import dataclass
from datetime import date
import os
from pathlib import Path
import re
import shlex
import stat
import subprocess
import sys
import tempfile
from typing import Optional


ROOT = Path(__file__).resolve().parents[1]
NUMBER = r"(0|[1-9][0-9]*)"
VERSION = re.compile(rf"{NUMBER}\.{NUMBER}\.{NUMBER}(?:-dev\.{NUMBER})?")
CMAKE_VERSION = r"(^project\(usbeekeeper-plasma[ \t]+VERSION[ \t]+)([0-9.]+)(?=[ \t)])"


class ReleaseError(ValueError):
    pass


@dataclass(frozen=True)
class Version:
    base: tuple
    development: Optional[int] = None

    @classmethod
    def parse(cls, text):
        match = VERSION.fullmatch(text)
        if not match:
            raise ReleaseError("Use X.Y.Z or X.Y.Z-dev.N with no leading zeroes or 'v' prefix.")
        return cls(tuple(int(match[i]) for i in (1, 2, 3)),
                   int(match[4]) if match[4] is not None else None)

    @property
    def numeric(self):
        return ".".join(map(str, self.base))

    @property
    def upstream(self):
        suffix = "" if self.development is None else f"-dev.{self.development}"
        return self.numeric + suffix

    @property
    def rpm(self):
        return self.upstream.replace("-dev.", "~dev.")

    @property
    def order_key(self):
        # For this bounded grammar, this agrees with RPM's numeric/~ ordering.
        return (*self.base, self.development is None,
                self.development if self.development is not None else 0)


def git(root, *arguments):
    environment = dict(os.environ, GIT_OPTIONAL_LOCKS="0")
    for name in ("GIT_DIR", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_INDEX_FILE"):
        environment.pop(name, None)
    try:
        return subprocess.run(["git", "-C", str(root), *arguments],
                              text=True, capture_output=True, check=False, env=environment)
    except FileNotFoundError as error:
        raise ReleaseError("Git is required; run this tool from a Git checkout.") from error


def require_clean(root, paths):
    top = git(root, "rev-parse", "--show-toplevel")
    if top.returncode or Path(top.stdout.strip()).resolve() != root:
        raise ReleaseError("The release tool must belong to the root of this Git checkout.")
    names = [str(path.relative_to(root)) for path in paths]
    tracked = git(root, "ls-files", "--error-unmatch", "--", *names)
    if tracked.returncode:
        raise ReleaseError("Release target files must already be tracked in this Git checkout.")
    status = git(root, "status", "--porcelain", "--untracked-files=all", "--", *names)
    if status.returncode:
        raise ReleaseError(status.stderr.strip() or "Unable to check Git status.")
    if status.stdout:
        raise ReleaseError("Commit or stash changes to the target files first: " + ", ".join(names))


def read_text(path):
    if path.is_symlink() or not path.is_file():
        raise ReleaseError(f"Expected a regular, non-symlink file: {path.name}")
    text = path.read_bytes().decode("utf-8")
    if "\r" in text:
        raise ReleaseError(f"Expected LF line endings in {path.name}.")
    return text


def field(text, pattern, label):
    matches = list(re.finditer(pattern, text, re.MULTILINE))
    if len(matches) != 1:
        raise ReleaseError(f"Expected exactly one {label}; no files were changed.")
    return matches[0]


def replace_value(text, pattern, replacement, label):
    match = field(text, pattern, label)
    return text[:match.start(2)] + replacement + text[match.end(2):]


UPSTREAM_FIELD = r"^(%global[ \t]+upstream_version[ \t]+)(\S+)[ \t]*$"
RPM_FIELD = r"^(Version:[ \t]+)(\S+)[ \t]*$"
RELEASE_FIELD = r"^(Release:[ \t]+)(\S+)[ \t]*$"


def check_packaging_contract(root, spec):
    # Deliberately check the existing integration contract, not arbitrary YAML/Make syntax.
    workflow = read_text(root / ".github/workflows/release.yml")
    if not re.search(r"(?m)^on:\n[ \t]+push:\n[ \t]+tags:\n[ \t]+- ['\"]v\*['\"][ \t]*$", workflow):
        raise ReleaseError("Review the GitHub release workflow: expected the v* tag trigger.")
    if "prerelease: ${{ contains(github.ref_name, '-') }}" not in workflow:
        raise ReleaseError("Review the GitHub release workflow's prerelease handling.")
    expected = (
        "/archive/refs/tags/v%{upstream_version}.tar.gz#/%{name}-%{upstream_version}.tar.gz",
        "%autosetup -n usbeekeeper-plasma-%{upstream_version}",
    )
    if not all(item in spec for item in expected):
        raise ReleaseError("Review the spec's versioned source URL and archive directory.")
    copr = read_text(root / ".copr/Makefile")
    if not all(item in copr for item in (
        "'%{SOURCE}'", 'archive_root="usbeekeeper-plasma-$$upstream_version"',
        'archive --format=tar --prefix="$$archive_root/" HEAD',
    )):
        raise ReleaseError("Review the COPR source filename/archive-directory contract.")


def add_changelog(spec, rpm_version, release, message, today):
    marker = field(spec, r"^%changelog[ \t]*\n", "%changelog section")
    body = spec[marker.end():]
    evr = f"{rpm_version}-{release}"
    if re.search(r"^\* .* - " + re.escape(evr) + r"[ \t]*$", body, re.MULTILINE):
        raise ReleaseError(f"A changelog entry for {evr} already exists.")
    weekdays = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")
    months = ("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec")
    stamp = f"{weekdays[today.weekday()]} {months[today.month - 1]} {today.day:02d} {today.year}"
    entry = f"* {stamp} USBeeKeeper contributors - {evr}\n- {message}\n"
    return spec[:marker.end()] + entry + ("\n" if body else "") + body


def write_changes(originals, changes, verify_clean):
    """Stage complete files first; retain backups until every replacement succeeds."""
    staged = {}
    backups = {}
    applied = []
    recovery = set()

    def stage(path, content):
        descriptor, filename = tempfile.mkstemp(prefix=f".{path.name}.release-", dir=path.parent)
        temporary = Path(filename)
        try:
            with os.fdopen(descriptor, "wb") as output:
                output.write(content.encode("utf-8"))
            temporary.chmod(stat.S_IMODE(path.stat().st_mode))
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
        return temporary

    try:
        for path, content in changes.items():
            staged[path] = stage(path, content)
            backups[path] = stage(path, originals[path])
        verify_clean()
        if any(read_text(path) != originals[path] for path in changes):
            raise ReleaseError("A target file changed while preparing the release; retry after review.")
        try:
            for path, temporary in staged.items():
                os.replace(temporary, path)
                applied.append(path)
        except OSError as error:
            for path in reversed(applied):
                # Never roll back over somebody else's concurrent edit.
                try:
                    if read_text(path) != changes[path]:
                        raise ReleaseError("Concurrent edit")
                    os.replace(backups[path], path)
                except (OSError, ValueError):
                    recovery.add(backups[path])
            if recovery:
                raise ReleaseError("Partial update; original backups retained for manual recovery: "
                                   + ", ".join(map(str, recovery))) from error
            raise
    finally:
        for temporary in (*staged.values(), *backups.values()):
            if temporary not in recovery:
                temporary.unlink(missing_ok=True)


def run(root, command, requested=None, today=None):
    root = root.resolve()
    spec_path = root / "usbeekeeper.spec"
    cmake_path = root / "CMakeLists.txt"
    targets = [spec_path, cmake_path] if command == "prepare" else [spec_path]
    require_clean(root, targets)
    branch = git(root, "symbolic-ref", "--quiet", "--short", "HEAD")
    if branch.returncode:
        raise ReleaseError("Check out a branch before preparing a release.")

    spec = read_text(spec_path)
    current = Version.parse(field(spec, UPSTREAM_FIELD, "upstream_version")[2])
    rpm_version = field(spec, RPM_FIELD, "RPM Version")[2]
    if rpm_version != current.rpm:
        raise ReleaseError("RPM Version does not match upstream_version; resolve the inconsistency first.")
    release_text = field(spec, RELEASE_FIELD, "RPM Release")[2]
    match = re.fullmatch(r"([1-9][0-9]*)%\{\?dist\}", release_text)
    if not match:
        raise ReleaseError("Expected a positive integer RPM Release followed by %{?dist}.")
    old_release = int(match[1])
    check_packaging_contract(root, spec)
    originals = {spec_path: spec}
    changes = {}
    if command == "prepare":
        version = Version.parse(requested)
        if version.order_key <= current.order_key:
            raise ReleaseError(f"Requested version must be greater than {current.upstream}.")
        tag = "v" + version.upstream
        tag_check = git(root, "show-ref", "--verify", "--quiet", "refs/tags/" + tag)
        if tag_check.returncode == 0:
            raise ReleaseError(f"Local tag {tag} already exists; no files were changed.")
        if tag_check.returncode != 1:
            raise ReleaseError("Unable to check existing local tags.")
        cmake = read_text(cmake_path)
        if field(cmake, CMAKE_VERSION, "CMake project VERSION")[2] != current.numeric:
            raise ReleaseError("CMake project VERSION does not match the current numeric base.")
        originals[cmake_path] = cmake
        updated_cmake = replace_value(cmake, CMAKE_VERSION, version.numeric, "CMake project VERSION")
        if updated_cmake != cmake:
            changes[cmake_path] = updated_cmake
        spec = replace_value(spec, UPSTREAM_FIELD, version.upstream, "upstream_version")
        spec = replace_value(spec, RPM_FIELD, version.rpm, "RPM Version")
        release = 1
        kind = "development release" if version.development is not None else "release"
        message = f"Prepare {kind} {version.upstream}."
        commit_message = f"Prepare USBeeKeeper {version.upstream}"
    else:
        version = current
        release = old_release + 1
        message = "Packaging-only update; application version unchanged."
        commit_message = f"Bump USBeeKeeper RPM release to {release}"
    spec = replace_value(spec, RELEASE_FIELD, f"{release}%{{?dist}}", "RPM Release")
    changes[spec_path] = add_changelog(spec, version.rpm, release, message, today or date.today())
    write_changes(originals, changes, lambda: require_clean(root, targets))

    names = [str(path.relative_to(root)) for path in changes]
    print(f"USBeeKeeper {version.upstream}; RPM {version.rpm}-{release}%{{?dist}}")
    print("Updated: " + ", ".join(names))
    print("GitHub v* tags/prerelease handling and COPR source naming checked locally.")
    print("Review, then run these commands yourself:")
    commands = [
        ["cd", str(root)],
        ["git", "diff", "--check"], ["git", "diff", "--", *names],
        ["git", "add", "--", *names],
        ["git", "commit", "--only", "-m", commit_message, "--", *names],
    ]
    if command == "prepare":
        commands.append(["git", "tag", "-a", tag, "-m", f"USBeeKeeper {version.upstream}"])
    commands.append(["git", "push", "origin", f"HEAD:refs/heads/{branch.stdout.strip()}"])
    if command == "prepare":
        commands.append(["git", "push", "origin", f"refs/tags/{tag}"])
    for arguments in commands:
        print("  " + shlex.join(arguments))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare", help="Prepare a newer upstream release").add_argument("version")
    commands.add_parser("rpm-bump", help="Increment the packaging-only RPM Release")
    arguments = parser.parse_args(argv)
    try:
        run(ROOT, arguments.command, getattr(arguments, "version", None))
    except (ValueError, OSError) as error:
        print(f"release: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
