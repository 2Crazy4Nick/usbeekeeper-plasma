#!/usr/bin/env python3
"""Validate declarative WhatCable physical-device profiles."""

import argparse
import json
import re
import sys
from pathlib import Path

HEX_ID = re.compile(r"^[0-9a-f]{4}$")
PROFILE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
RELATIONSHIPS = {"integrated_function"}
RELATIONSHIP_SOURCES = {"none", "hardware", "profile", "user"}
IDENTITY_SOURCES = {"none", "profile", "user", "deterministic"}
CONFIDENCES = {"none", "possible", "probable", "exact"}


def fail(path, message):
    raise ValueError(f"{path}: {message}")


def object_only(value, allowed, path):
    if not isinstance(value, dict):
        fail(path, "must be an object")
    unknown = set(value) - set(allowed)
    if unknown:
        fail(path, f"unknown field(s): {', '.join(sorted(unknown))}")


def vid_pid(value, path):
    object_only(value, {"vid", "pid"}, path)
    for field in ("vid", "pid"):
        if not isinstance(value.get(field), str) or not HEX_ID.fullmatch(value[field]):
            fail(path, f"{field} must be four lowercase hexadecimal digits")


def validate(path):
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        fail(path, f"invalid JSON: {exc}")
    object_only(data, {"schemaVersion", "id", "description", "physicalType", "physicalName",
                       "physicalVendor", "physicalIdentitySource", "physicalConfidence",
                       "relationshipSource", "root", "members"}, path)
    if data.get("schemaVersion") != 1:
        fail(path, "schemaVersion must be 1")
    if not isinstance(data.get("id"), str) or not PROFILE_ID.fullmatch(data["id"]):
        fail(path, "id is invalid")
    if data.get("physicalIdentitySource", "deterministic") not in IDENTITY_SOURCES:
        fail(path, "invalid physicalIdentitySource")
    if data.get("physicalConfidence", "none") not in CONFIDENCES:
        fail(path, "invalid physicalConfidence")
    if data.get("relationshipSource", "profile") not in RELATIONSHIP_SOURCES:
        fail(path, "invalid relationshipSource")

    root = data.get("root")
    object_only(root, {"requireCompanionGroup", "requireContainerId", "exactCompanionFunctions",
                       "companionFunctions", "parentFunctions", "manufacturer", "productDb",
                       "interfaceClass"}, f"{path}:root")
    for field in ("requireCompanionGroup", "requireContainerId", "exactCompanionFunctions"):
        if field in root and not isinstance(root[field], bool):
            fail(path, f"root.{field} must be boolean")
    for field in ("companionFunctions", "parentFunctions"):
        if field in root:
            if not isinstance(root[field], list):
                fail(path, f"root.{field} must be an array")
            for i, item in enumerate(root[field]):
                vid_pid(item, f"{path}:root.{field}[{i}]")
        else:
            root[field] = []
    if root.get("exactCompanionFunctions") and len(root["companionFunctions"]) != 2:
        fail(path, "exactCompanionFunctions requires exactly two companionFunctions")
    if len({(x["vid"], x["pid"]) for x in root["companionFunctions"]}) != len(root["companionFunctions"]):
        fail(path, "duplicate companion function")
    if root.get("requireCompanionGroup") is not True and not root["companionFunctions"] \
            and not root["parentFunctions"] and not root.get("requireContainerId") \
            and not any(root.get(x) for x in ("manufacturer", "productDb", "interfaceClass")):
        fail(path, "root matcher is empty")

    members = data.get("members")
    if not isinstance(members, list):
        fail(path, "members must be an array")
    seen = set()
    for i, member in enumerate(members):
        member_path = f"{path}:members[{i}]"
        object_only(member, {"match", "relationship", "required"}, member_path)
        if member.get("required") is not True:
            fail(member_path, "required must be true")
        if member.get("relationship") not in RELATIONSHIPS:
            fail(member_path, "unknown relationship")
        match = member.get("match")
        object_only(match, {"vid", "pid", "logicalPort", "interfaceClass"}, member_path + ".match")
        vid_pid({"vid": match.get("vid"), "pid": match.get("pid")}, member_path + ".match")
        port = match.get("logicalPort")
        if port is not None and (not isinstance(port, str) or not re.fullmatch(r"[1-9][0-9]*", port)):
            fail(member_path, "logicalPort must be a positive decimal string")
        key = (match["vid"], match["pid"], port or "")
        if key in seen:
            fail(member_path, "duplicate member definition")
        seen.add(key)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--directory", type=Path,
                        default=Path(__file__).resolve().parents[1] / "src/plasmoid/profiles")
    args = parser.parse_args()
    try:
        count = validate_directory(args.directory)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"Validated {count} profile(s).")
    return 0


def validate_directory(directory):
    files = sorted(p for p in directory.glob("*.json") if p.name != "schema.json")
    ids = set()
    for path in files:
        validate(path)
        data = json.loads(path.read_text())
        if data["id"] in ids:
            fail(path, f"duplicate profile id {data['id']}")
        ids.add(data["id"])
    return len(files)


if __name__ == "__main__":
    raise SystemExit(main())
