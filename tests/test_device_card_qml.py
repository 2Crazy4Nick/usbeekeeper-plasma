"""Check the actual DeviceCard layout when the Qt/Plasma QML runtime is available."""

import os
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET


class DeviceCardQmlTests(unittest.TestCase):
    def test_detail_settings_use_per_applet_configuration(self):
        root = Path(__file__).resolve().parents[1]
        config = ET.parse(root / "src/plasmoid/contents/config/main.xml")
        ns = {"k": "http://www.kde.org/standards/kcfg/1.0"}
        entries = {entry.attrib["name"]: entry for entry in config.findall(".//k:entry", ns)}
        level = entries["deviceInformationDetailLevel"]
        self.assertEqual(level.attrib["type"], "String")
        self.assertEqual(level.find("k:default", ns).text, "standard")
        controls = entries["showPhysicalDeviceRenamingControls"]
        self.assertEqual(controls.attrib["type"], "Bool")
        self.assertEqual(controls.find("k:default", ns).text, "false")
        main = (root / "src/plasmoid/contents/ui/main.qml").read_text()
        self.assertIn('informationDetailLevel: Plasmoid.configuration.deviceInformationDetailLevel || "standard"', main)
        self.assertIn('showPhysicalDeviceRenamingControls: Plasmoid.configuration.showPhysicalDeviceRenamingControls || false', main)

    def test_applet_metadata_has_full_upstream_version(self):
        root = Path(__file__).resolve().parents[1]
        upstream = re.search(r"(?m)^%global upstream_version (\S+)$",
                             (root / "usbeekeeper.spec").read_text())[1]
        metadata = json.loads((root / "src/plasmoid/metadata.json").read_text())
        self.assertEqual(metadata["KPlugin"]["Version"], upstream)

    @unittest.skipUnless(shutil.which("rpmspec"), "RPM tooling is unavailable")
    def test_rpm_preserves_fresh_qml_timestamps(self):
        root = Path(__file__).resolve().parents[1]
        # Query the actual parsed spec policy, not a duplicated Python rule.
        # Qt otherwise reuses old bytecode for different releases built with
        # the same clamped changelog timestamp (the installed-widget failure).
        with tempfile.TemporaryDirectory() as temporary:
            spec = Path(temporary) / "usbeekeeper.spec"
            spec.write_text((root / "usbeekeeper.spec").read_text()
                            + "\n%{echo:QML_MTIME_CLAMP=%{clamp_mtime_to_source_date_epoch}}\n")
            result = subprocess.run(["rpmspec", "-P", str(spec)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("QML_MTIME_CLAMP=0", result.stdout + result.stderr)

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
