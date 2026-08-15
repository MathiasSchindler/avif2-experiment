#!/usr/bin/env python3

import os
from pathlib import Path
from unittest import TestCase


UNIT_ONLY_ENV = "ENCODER_TEST_UNIT_ONLY"


def configured_path(name: str, default: Path) -> Path:
    return Path(os.environ.get(name, default))


def require_executable(test: TestCase, path: Path, label: str) -> Path:
    if path.is_file() and os.access(path, os.X_OK):
        return path
    message = f"{label} is not executable: {path}"
    test.fail(message)
    raise AssertionError("unreachable")


def require_validator(test: TestCase, path: Path, label: str) -> Path:
    if os.environ.get(UNIT_ONLY_ENV) == "1":
        test.skipTest(f"{label} disabled by explicit unit-only run")
    return require_executable(test, path, label)
