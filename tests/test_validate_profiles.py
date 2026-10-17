#!/usr/bin/env python3
import json
import re
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
        directory = ROOT / "src/plasmoid/profiles"
        expected = len([path for path in directory.glob("*.json") if path.name != "schema.json"])
        self.assertEqual(validate_profiles.validate_directory(directory), expected)

    def test_schema_contains_structural_profile_constraints(self):
        schema = json.loads((ROOT / "src/plasmoid/profiles/schema.json").read_text())
        root = schema["$defs"]["root"]
        self.assertTrue(root["properties"]["companionFunctions"]["uniqueItems"])
        self.assertTrue(root["properties"]["parentFunctions"]["uniqueItems"])
        self.assertTrue(root["allOf"])
        self.assertTrue(root["anyOf"])

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

    def test_wrong_optional_types_are_rejected(self):
        malformed = [
            ("logicalPort", None),
            ("logicalPort", 4),
            ("interfaceClass", []),
        ]
        for field, value in malformed:
            with self.subTest(field=field, value=value):
                profile = self.valid()
                profile["members"][0]["match"][field] = value
                with tempfile.TemporaryDirectory() as temp:
                    with self.assertRaises(ValueError):
                        validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

    def test_logical_port_canonical_range(self):
        for port in ("1", "2", "255"):
            with self.subTest(port=port):
                profile = self.valid()
                profile["members"][0]["match"]["logicalPort"] = port
                with tempfile.TemporaryDirectory() as temp:
                    validate_profiles.validate(self.write(Path(temp), "valid.json", profile))

        for port in ("", "0", "01", "+1", "-1", "256", None, 1):
            with self.subTest(port=port):
                profile = self.valid()
                profile["members"][0]["match"]["logicalPort"] = port
                with tempfile.TemporaryDirectory() as temp:
                    with self.assertRaises(ValueError):
                        validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

    def test_identifier_suffixes_are_rejected(self):
        for field in ("id", "vid", "pid", "logicalPort"):
            for suffix in ("\n", "\r\n", " ", "\t", "!"):
                with self.subTest(field=field, suffix=repr(suffix)):
                    profile = self.valid()
                    target = profile if field == "id" else profile["members"][0]["match"]
                    target[field] += suffix
                    with tempfile.TemporaryDirectory() as temp:
                        with self.assertRaises(ValueError):
                            validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

    def test_schema_patterns_require_complete_strings(self):
        schema = json.loads((ROOT / "src/plasmoid/profiles/schema.json").read_text())
        member = schema["$defs"]["member"]["properties"]["match"]["properties"]
        ids = schema["$defs"]["vidPid"]["properties"]
        cases = [(schema["properties"]["id"]["pattern"], "Generic.audit_2-name")]
        cases += [(fields[field]["pattern"], "0bda")
                  for fields in (ids, member) for field in ("vid", "pid")]
        cases += [(member["logicalPort"]["pattern"], port) for port in ("1", "2", "255")]
        for pattern, value in cases:
            with self.subTest(pattern=pattern, value=value):
                # JSON Schema patterns search for a match; do not use fullmatch
                # here, which would hide an insufficiently strict pattern.
                self.assertIsNotNone(re.search(pattern, value))
                for suffix in ("\n", "\r\n", " ", "\t", "!"):
                    self.assertIsNone(re.search(pattern, value + suffix))
        for port in ("", "0", "01", "+1", "-1", "256"):
            self.assertIsNone(re.search(member["logicalPort"]["pattern"], port))

    def test_duplicate_parent_functions_are_rejected(self):
        profile = self.valid()
        function = {"vid": "2109", "pid": "2817"}
        profile["root"]["parentFunctions"] = [function, function]
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

    def test_non_string_relationship_is_rejected_cleanly(self):
        profile = self.valid()
        profile["members"][0]["relationship"] = []
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

        for field, value in (("manufacturer", []), ("physicalName", {}),
                             ("schemaVersion", True)):
            with self.subTest(field=field, value=value):
                profile = self.valid()
                if field == "schemaVersion":
                    profile[field] = value
                elif field == "physicalName":
                    profile[field] = value
                else:
                    profile["root"][field] = value
                with tempfile.TemporaryDirectory() as temp:
                    with self.assertRaises(ValueError):
                        validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

    def test_unknown_optional_enum_is_rejected(self):
        profile = self.valid()
        profile["physicalConfidence"] = "certain"
        with tempfile.TemporaryDirectory() as temp:
            with self.assertRaises(ValueError):
                validate_profiles.validate(self.write(Path(temp), "bad.json", profile))

    def test_duplicate_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            self.write(directory, "a.json", self.valid())
            self.write(directory, "b.json", self.valid())
            with self.assertRaises(ValueError):
                validate_profiles.validate_directory(directory)


if __name__ == "__main__":
    unittest.main()
