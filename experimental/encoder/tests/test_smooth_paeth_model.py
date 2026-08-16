import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path
from unittest import TestCase


ROOT = Path(__file__).parents[3]
ENCODER = Path(__file__).parents[1]
SPEC = importlib.util.spec_from_file_location(
    "smooth_paeth_model", ENCODER / "tools/smooth_paeth_model.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
VECTORS = Path(__file__).parent / "smooth_paeth_vectors.json"


class SmoothPaethModelTests(TestCase):
    def test_locked_vectors_match(self) -> None:
        self.assertEqual(json.loads(VECTORS.read_text()), MODULE.vectors())

    def test_mode_syntax_and_interactions(self) -> None:
        self.assertEqual(
            [MODULE.luma_mode_index(mode, 8, 8) for mode in MODULE.MODES],
            [0, 5, 6, 1, 2, 3, 4])
        for mode in MODULE.MODES[3:]:
            events = MODULE.write_luma_mode(
                MODULE.cardinal.SyntaxState(), mode, 8, 8)
            self.assertEqual(
                [event["name"] for event in events],
                ["y_mode_set", "y_mode_index", "fsc_mode"])
        self.assertEqual(MODULE.chroma_mode_symbol("dc", "smooth"), (0, 1))
        self.assertEqual(MODULE.chroma_mode_symbol("vertical", "paeth"), (1, 5))

    def test_neutral_and_short_boundaries(self) -> None:
        above, left, corner = MODULE.form_references(8, 8, None, None)
        self.assertEqual(set(above), {127})
        self.assertEqual(set(left), {129})
        self.assertEqual(corner, 128)
        above, left, corner = MODULE.form_references(
            4, 4, [1, 2], [3], 9)
        self.assertEqual(above, [1, 2, 2, 2, 2])
        self.assertEqual(left, [3, 3, 3, 3, 3])
        self.assertEqual(corner, 9)

    def test_paeth_tie_order(self) -> None:
        self.assertEqual(MODULE.paeth_predict(1, 1, [10], [20], 10), [20])
        self.assertEqual(MODULE.paeth_predict(1, 1, [10], [20], 20), [10])

    def test_requested_matrix_and_incremental_gate(self) -> None:
        data = json.loads(VECTORS.read_text())
        summary = MODULE.evaluate(data)
        self.assertTrue(summary["gate"]["passed"])
        self.assertGreater(summary["incremental_wins"], 0)
        self.assertLess(summary["expanded"]["sse"], summary["baseline"]["sse"])
        self.assertGreater(summary["locked_subset"]["wins"], 0)
        keys = {(case["format"], tuple(case["luma_reference_size"]))
                for case in data["cases"]}
        self.assertEqual(
            keys,
            {(fmt, size) for fmt in MODULE.FORMATS for size in MODULE.SIZES})

    def test_pinned_avm_kernel_parity(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("pinned AVM parity disabled by unit-only run")
        library = Path(os.environ.get(
            "AVMDEC", ROOT / "build/reference/avm/avmdec")).parent / "libavm.a"
        output = ENCODER / "build/smooth-paeth-avm-driver"
        completed = subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
            "-Wpedantic", "-Werror",
            str(Path(__file__).parent / "smooth_paeth_avm_driver.c"),
            str(library), "-lm", "-lpthread", "-o", str(output),
        ], check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        executed = subprocess.run(
            [str(output)], check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        self.assertEqual(executed.returncode, 0, executed.stderr.decode())

    def test_project_decoder_kernel_parity(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("project decoder parity disabled by unit-only run")
        library = Path(os.environ.get(
            "PROJECT_DECODER",
            ROOT / "build/host-cmake/av2heif-decoder")).parent / \
            "libav2heif-core.a"
        output = ENCODER / "build/smooth-paeth-project-driver"
        completed = subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
            "-Wpedantic", "-Werror", "-I", str(ROOT / "include"),
            "-I", str(ROOT / "src"),
            str(Path(__file__).parent / "smooth_paeth_project_driver.c"),
            str(library), "-o", str(output),
        ], check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0, completed.stderr.decode())
        executed = subprocess.run(
            [str(output)], check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        self.assertEqual(executed.returncode, 0, executed.stderr.decode())
