"""Exercise release preparation against disposable Git checkouts only."""

import contextlib
from datetime import date
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import release


SPEC = """%global upstream_version 0.1.0-dev.1
Name:           usbeekeeper
Version:        0.1.0~dev.1
Release:        2%{?dist}
Source0:        %{url}/archive/refs/tags/v%{upstream_version}.tar.gz#/%{name}-%{upstream_version}.tar.gz

%prep
%autosetup -n usbeekeeper-plasma-%{upstream_version}

%changelog
* Sun Oct 18 2026 USBeeKeeper contributors - 0.1.0~dev.1-2
- Previous packaging update.

* Sat Oct 17 2026 USBeeKeeper contributors - 0.1.0~dev.1-1
- Previous upstream release.
"""
CMAKE = "project(usbeekeeper-plasma VERSION 0.1.0 LANGUAGES CXX)\n# Unrelated content.\n"
TODAY = date(2026, 10, 18)


class VersionTests(unittest.TestCase):
    def test_rpm_ordering_and_conversion(self):
        ordered = ["0.1.0-dev.0", "0.1.0-dev.1", "0.1.0-dev.2", "0.1.0-dev.10",
                   "0.1.0", "0.1.1-dev.0", "0.2.0-dev.1", "1.0.0"]
        parsed = [release.Version.parse(value) for value in ordered]
        self.assertEqual(sorted(parsed, key=lambda version: version.order_key), parsed)
        self.assertEqual(parsed[2].rpm, "0.1.0~dev.2")
        self.assertEqual(parsed[4].rpm, "0.1.0")
        # Compare our bounded grammar with RPM itself when the system has it.
        if shutil.which("rpm"):
            for left, right in zip(parsed, parsed[1:]):
                result = subprocess.check_output(
                    ["rpm", "--eval", '%{lua:print(rpm.vercmp("' + left.rpm + '","'
                     + right.rpm + '"))}'], text=True)
                self.assertLess(int(result.strip()), 0)

    def test_invalid_versions(self):
        for value in ("", "v0.1.0", "0.1", "01.1.0", "0.01.0", "0.1.00", "0.1.0-dev",
                      "0.1.0-dev.01", "0.1.0-dev.-1", "0.1.0-rc.1", "0.1.0~dev.2",
                      "0.1.0+build.1", "0.1.0-dev.2\n", "0.1.0 ", "0.1.0;echo bad"):
            with self.subTest(value=value), self.assertRaises(release.ReleaseError):
                release.Version.parse(value)


@unittest.skipUnless(shutil.which("git"), "Git is needed for disposable-checkout tests")
class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.spec = self.root / "usbeekeeper.spec"
        self.cmake = self.root / "CMakeLists.txt"
        self.spec.write_text(SPEC)
        self.cmake.write_text(CMAKE)
        # Use the real integrations rather than replacing them with permissive test stubs.
        for name in (".github/workflows/release.yml", ".copr/Makefile"):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        self.git("init", "--quiet", "--initial-branch=main", "--template=")
        self.commit_fixture()
        self.head = self.git("rev-parse", "HEAD")

    def git(self, *arguments):
        environment = dict(os.environ)
        for name in ("GIT_DIR", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_INDEX_FILE"):
            environment.pop(name, None)
        return subprocess.check_output(
            ["git", "-c", "user.name=Release test", "-c", "user.email=test@example.invalid",
             "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgsign=false",
             "-C", str(self.root), *arguments], text=True, env=environment).strip()

    def commit_fixture(self):
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", "Disposable test fixture")

    def invoke(self, *arguments):
        output, errors = io.StringIO(), io.StringIO()
        with mock.patch.object(release, "ROOT", self.root), \
                contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            status = release.main(list(arguments))
        return status, output.getvalue(), errors.getvalue()

    def prepare(self, version):
        with contextlib.redirect_stdout(io.StringIO()):
            release.run(self.root, "prepare", version, today=TODAY)

    def test_development_transition_resets_release_and_keeps_history(self):
        self.prepare("0.1.0-dev.2")
        text = self.spec.read_text()
        self.assertIn("%global upstream_version 0.1.0-dev.2\n", text)
        self.assertIn("Version:        0.1.0~dev.2\n", text)
        self.assertIn("Release:        1%{?dist}\n", text)
        self.assertIn("* Sun Oct 18 2026 USBeeKeeper contributors - 0.1.0~dev.2-1\n", text)
        self.assertIn("- Prepare development release 0.1.0-dev.2.\n", text)
        old_history = SPEC.split("%changelog\n", 1)[1]
        self.assertTrue(text.endswith(old_history))
        self.assertEqual(self.cmake.read_text(), CMAKE)
        self.assertEqual(self.git("rev-parse", "HEAD"), self.head)
        self.assertEqual(self.git("tag", "--list"), "")

    def test_numeric_base_change_and_stable_release(self):
        self.prepare("0.2.0-dev.1")
        self.assertIn("VERSION 0.2.0 LANGUAGES", self.cmake.read_text())
        self.assertIn("# Unrelated content.", self.cmake.read_text())
        self.commit_fixture()
        self.prepare("0.2.0")
        self.assertIn("Version:        0.2.0\n", self.spec.read_text())
        self.assertIn("- Prepare release 0.2.0.", self.spec.read_text())
        self.commit_fixture()
        with self.assertRaises(release.ReleaseError):
            self.prepare("0.2.0-dev.100")

    def test_numeric_development_counter(self):
        self.prepare("0.1.0-dev.2")
        self.commit_fixture()
        self.prepare("0.1.0-dev.10")
        self.assertIn("Version:        0.1.0~dev.10\n", self.spec.read_text())

    def test_packaging_increment_and_printed_commands(self):
        status, output, error = self.invoke("rpm-bump")
        self.assertEqual((status, error), (0, ""))
        text = self.spec.read_text()
        self.assertIn("%global upstream_version 0.1.0-dev.1\n", text)
        self.assertIn("Version:        0.1.0~dev.1\n", text)
        self.assertIn("Release:        3%{?dist}\n", text)
        self.assertIn("USBeeKeeper contributors - 0.1.0~dev.1-3\n", text)
        self.assertEqual(self.cmake.read_text(), CMAKE)
        self.assertIn("git add -- usbeekeeper.spec", output)
        self.assertIn("git push origin HEAD:refs/heads/main", output)
        self.assertNotIn("git tag", output)
        self.assertEqual(self.git("rev-parse", "HEAD"), self.head)

    def test_prepare_prints_exact_tag_commands(self):
        status, output, error = self.invoke("prepare", "0.1.0-dev.2")
        self.assertEqual((status, error), (0, ""))
        self.assertIn("git tag -a v0.1.0-dev.2 -m 'USBeeKeeper 0.1.0-dev.2'", output)
        self.assertIn("git push origin refs/tags/v0.1.0-dev.2", output)

    def test_repeated_invocation_does_not_write_again(self):
        for arguments in (("rpm-bump",), ("prepare", "0.1.0-dev.2")):
            with self.subTest(arguments=arguments):
                self.assertEqual(self.invoke(*arguments)[0], 0)
                once = self.spec.read_bytes(), self.cmake.read_bytes()
                self.assertEqual(self.invoke(*arguments)[0], 1)
                self.assertEqual((self.spec.read_bytes(), self.cmake.read_bytes()), once)
                # Reset only the disposable fixture for the next independent case.
                self.spec.write_text(SPEC)
                self.cmake.write_text(CMAKE)

    def test_invalid_equal_or_lower_versions_do_not_write(self):
        for value in ("0.1.0-dev.1", "0.1.0-dev.0", "0.0.9", "v0.2.0", "0.2.0\n"):
            with self.subTest(value=value):
                status, _, error = self.invoke("prepare", value)
                self.assertEqual(status, 1)
                self.assertTrue(error)
                self.assertEqual(self.spec.read_text(), SPEC)
                self.assertEqual(self.cmake.read_text(), CMAKE)

    def test_existing_local_tag_is_rejected(self):
        self.git("tag", "v0.1.0-dev.2")
        self.assertEqual(self.invoke("prepare", "0.1.0-dev.2")[0], 1)
        self.assertEqual(self.spec.read_text(), SPEC)

    def test_dirty_and_staged_targets_are_rejected(self):
        self.spec.write_text(SPEC + "\n# User change.\n")
        self.git("add", "usbeekeeper.spec")
        for arguments in (("rpm-bump",), ("prepare", "0.1.0-dev.2")):
            self.assertEqual(self.invoke(*arguments)[0], 1)
            self.assertEqual(self.spec.read_text(), SPEC + "\n# User change.\n")

    def test_git_location_environment_cannot_bypass_target_checks(self):
        self.spec.write_text(SPEC + "\n# User change.\n")
        with mock.patch.dict(os.environ, {"GIT_DIR": str(self.root / "missing-git"),
                                          "GIT_INDEX_FILE": str(self.root / "alternate-index")}):
            status, _, error = self.invoke("rpm-bump")
        self.assertEqual(status, 1)
        self.assertIn("Commit or stash changes", error)
        self.assertEqual(self.spec.read_text(), SPEC + "\n# User change.\n")

    def test_prepare_refuses_dirty_cmake_but_rpm_bump_leaves_it_alone(self):
        self.cmake.write_text(CMAKE + "# User change.\n")
        self.assertEqual(self.invoke("prepare", "0.1.0-dev.2")[0], 1)
        self.assertEqual(self.spec.read_text(), SPEC)
        self.assertEqual(self.invoke("rpm-bump")[0], 0)
        self.assertEqual(self.cmake.read_text(), CMAKE + "# User change.\n")

    def test_unrelated_dirty_file_is_preserved(self):
        other = self.root / "notes.txt"
        other.write_text("User notes.\n")
        self.assertEqual(self.invoke("prepare", "0.1.0-dev.2")[0], 0)
        self.assertEqual(other.read_text(), "User notes.\n")

    def test_unrelated_staged_file_is_not_in_suggested_commit(self):
        other = self.root / "notes.txt"
        other.write_text("User notes.\n")
        self.git("add", "notes.txt")
        status, output, error = self.invoke("rpm-bump")
        self.assertEqual((status, error), (0, ""))
        self.assertIn("git commit --only -m 'Bump USBeeKeeper RPM release to 3' -- usbeekeeper.spec", output)
        self.assertEqual(self.git("diff", "--cached", "--name-only"), "notes.txt")

    def test_next_packaging_bump_after_commit_keeps_previous_entry(self):
        self.assertEqual(self.invoke("rpm-bump")[0], 0)
        self.commit_fixture()
        self.assertEqual(self.invoke("rpm-bump")[0], 0)
        text = self.spec.read_text()
        self.assertIn("Release:        4%{?dist}\n", text)
        self.assertIn("USBeeKeeper contributors - 0.1.0~dev.1-4\n", text)
        self.assertIn("USBeeKeeper contributors - 0.1.0~dev.1-3\n", text)

    def test_inconsistent_metadata_and_missing_changelog_fail_before_writes(self):
        malformed = (
            SPEC.replace("Version:        0.1.0~dev.1", "Version:        0.1.0~dev.9"),
            SPEC.replace("Release:        2%{?dist}", "Release:        2"),
            SPEC.replace("%changelog\n", ""),
            SPEC + "\n%changelog\n",
        )
        for content in malformed:
            with self.subTest(content=content):
                self.spec.write_text(content)
                self.commit_fixture()
                self.assertEqual(self.invoke("prepare", "0.2.0-dev.1")[0], 1)
                self.assertEqual(self.spec.read_text(), content)
                self.assertEqual(self.cmake.read_text(), CMAKE)

    def test_empty_changelog_and_duplicate_entry(self):
        self.spec.write_text(SPEC.split("%changelog\n", 1)[0] + "%changelog\n")
        self.commit_fixture()
        self.prepare("0.1.0-dev.2")
        self.assertEqual(self.spec.read_text().count("* Sun Oct 18 2026"), 1)
        self.spec.write_text(SPEC + "\n* Sun Oct 18 2026 USBeeKeeper contributors - 0.1.0~dev.2-1\n- Reserved.\n")
        self.commit_fixture()
        before = self.spec.read_bytes()
        self.assertEqual(self.invoke("prepare", "0.1.0-dev.2")[0], 1)
        self.assertEqual(self.spec.read_bytes(), before)

    def test_changed_integration_contract_rejected(self):
        copr = self.root / ".copr/Makefile"
        copr.write_text("# A different source generator.\n")
        self.assertEqual(self.invoke("prepare", "0.1.0-dev.2")[0], 1)
        self.assertEqual(self.spec.read_text(), SPEC)

    def test_second_replace_failure_restores_both_targets(self):
        original_replace = release.os.replace
        calls = 0

        def fail_second(source, destination):
            nonlocal calls
            calls += 1
            if calls == 2:
                raise OSError("Injected write failure")
            return original_replace(source, destination)

        with mock.patch.object(release.os, "replace", side_effect=fail_second):
            self.assertEqual(self.invoke("prepare", "0.2.0-dev.1")[0], 1)
        self.assertEqual(self.spec.read_text(), SPEC)
        self.assertEqual(self.cmake.read_text(), CMAKE)
        self.assertEqual(list(self.root.glob(".*.release-*")), [])

    def test_edit_during_preparation_is_preserved(self):
        original_check = release.require_clean
        calls = 0

        def edit_before_second_check(root, targets):
            nonlocal calls
            calls += 1
            if calls == 2:
                self.spec.write_text(SPEC + "\n# Concurrent user edit.\n")
            return original_check(root, targets)

        with mock.patch.object(release, "require_clean", side_effect=edit_before_second_check):
            self.assertEqual(self.invoke("prepare", "0.2.0-dev.1")[0], 1)
        self.assertEqual(self.spec.read_text(), SPEC + "\n# Concurrent user edit.\n")
        self.assertEqual(self.cmake.read_text(), CMAKE)
        self.assertEqual(list(self.root.glob(".*.release-*")), [])

    def test_rollback_failure_retains_original_backup(self):
        original_replace = release.os.replace
        calls = 0

        def fail_after_first(source, destination):
            nonlocal calls
            calls += 1
            if calls >= 2:
                raise OSError("Injected persistent write failure")
            return original_replace(source, destination)

        with mock.patch.object(release.os, "replace", side_effect=fail_after_first):
            status, _, error = self.invoke("prepare", "0.2.0-dev.1")
        self.assertEqual(status, 1)
        self.assertIn("manual recovery", error)
        backups = list(self.root.glob(".*.release-*"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_text(), CMAKE)
        self.assertEqual(self.spec.read_text(), SPEC)


if __name__ == "__main__":
    unittest.main()
