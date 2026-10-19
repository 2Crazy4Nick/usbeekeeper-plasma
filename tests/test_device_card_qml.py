"""Check the actual DeviceCard layout when the Qt/Plasma QML runtime is available."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class DeviceCardQmlTests(unittest.TestCase):
    def test_expanded_information_at_wide_and_narrow_tree_widths(self):
        qtpaths = shutil.which("qtpaths6")
        if not qtpaths:
            self.skipTest("Qt 6 runtime tools are unavailable")

        def qt_directory(key):
            return Path(subprocess.check_output([qtpaths, "--query", key], text=True).strip())

        runner = qt_directory("QT_INSTALL_BINS") / "qml"
        imports = qt_directory("QT_INSTALL_QML")
        if not runner.is_file() or not all((imports / name).is_file() for name in (
                "org/kde/kirigami/qmldir", "org/kde/plasma/components/qmldir")):
            self.skipTest("The Qt/Plasma QML runtime is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                               QML_DISABLE_DISK_CACHE="1", QT_FORCE_STDERR_LOGGING="1",
                               QT_LOGGING_TO_CONSOLE="1", QT_LOGGING_RULES="",
                               XDG_CONFIG_HOME=temporary, XDG_CACHE_HOME=temporary)
            probe = Path(__file__).parent / "qml/device_information_probe.qml"
            result = subprocess.run([str(runner), str(probe)], env=environment,
                                    capture_output=True, text=True, timeout=30)
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("PASS layout phase 1", output)
        self.assertIn("PASS layout phase 2", output)
        self.assertIn("PASS collapsed height", output)
        for error in ("TypeError:", "ReferenceError:", "SyntaxError:",
                      "Detected recursive rearrange", "Binding loop"):
            self.assertNotIn(error, output)


if __name__ == "__main__":
    unittest.main()
