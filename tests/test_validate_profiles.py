#!/usr/bin/env python3
import json
import tempfile
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import validate_profiles


class ProfileValidatorTests(unittest.TestCase):
    def write(self, directory, name, data):
        path = directory / name
        path.write_text(json.dumps(data))
        return path

    def valid(self):
        return {
            "schemaVersion": 1,
            "id": "generic.test",
            "root": {"requireCompanionGroup": True, "requireContainerId": True},
            "members": [{"match": {"vid": "0bda", "pid": "8153", "logicalPort": "2"},
                          "relationship": "integrated_function", "required": True}],
        }

    def test_bundled_profiles_load(self):
        self.assertEqual(validate_profiles.validate_directory(ROOT / "src/plasmoid/profiles"), 3)

    def test_bad_cases(self):
        cases = []
        bad = self.valid(); bad["root"]["companionFunctions"] = [{"vid": "xyz", "pid": "0817"}]
        cases.append(bad)
        bad = self.valid(); bad["schemaVersion"] = 2
        cases.append(bad)
        bad = self.valid(); bad["members"][0]["relationship"] = "external"
        cases.append(bad)
        bad = self.valid(); bad["root"]["typo"] = True
        cases.append(bad)
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            for i, case in enumerate(cases):
                path = self.write(directory, f"bad{i}.json", case)
                with self.assertRaises(ValueError):
                    validate_profiles.validate(path)

    def test_duplicate_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            self.write(directory, "a.json", self.valid())
            self.write(directory, "b.json", self.valid())
            with self.assertRaises(ValueError):
                validate_profiles.validate_directory(directory)


if __name__ == "__main__":
    unittest.main()
