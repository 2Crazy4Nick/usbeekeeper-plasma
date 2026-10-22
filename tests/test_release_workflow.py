"""Check the release-notes contract without running a publishing action."""

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ReleaseWorkflowTests(unittest.TestCase):
    def setUp(self):
        self.workflow = (ROOT / ".github/workflows/release.yml").read_text()

    def test_current_release_has_versioned_body(self):
        version = json.loads((ROOT / "src/plasmoid/metadata.json").read_text())["KPlugin"]["Version"]
        self.assertIn("body_path: releases/${{ github.ref_name }}.md", self.workflow)
        self.assertIn("prerelease: ${{ contains(github.ref_name, '-') }}", self.workflow)
        notes = ROOT / "releases" / ("v" + version + ".md")
        self.assertTrue(notes.is_file(), str(notes))
        self.assertTrue(notes.read_text().strip())
        self.assertNotIn("${{", notes.read_text())  # body_path Markdown is not interpolated.

    @unittest.skipUnless(shutil.which("bash"), "Bash is needed to exercise the workflow guard")
    def test_actual_notes_guard_supports_regular_and_development_tags(self):
        # Exercise the production shell command, not a Python reimplementation
        # of its file checks. No GitHub API, tags or releases are created.
        match = re.search(r"- name: Check versioned release notes\n\s+run: ([^\n]+)", self.workflow)
        self.assertIsNotNone(match)
        with tempfile.TemporaryDirectory() as temporary:
            notes = Path(temporary) / "releases"
            notes.mkdir()
            for tag in ("v0.1.0", "v0.1.1-dev.1"):
                with self.subTest(tag=tag):
                    environment = dict(os.environ, GITHUB_REF_NAME=tag)
                    path = notes / (tag + ".md")
                    for content, expected in ((None, False), ("", False), ("# Release notes\n", True)):
                        if content is not None:
                            path.write_text(content)
                        result = subprocess.run(["bash", "--noprofile", "--norc", "-ec", match[1]],
                                                cwd=temporary, env=environment, capture_output=True)
                        self.assertEqual(result.returncode == 0, expected, (tag, content, result.stderr))


if __name__ == "__main__":
    unittest.main()
