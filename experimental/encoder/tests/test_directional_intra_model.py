import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path
from unittest import TestCase


TOOLS = Path(__file__).parents[1] / "tools"
SPEC = importlib.util.spec_from_file_location(
    "directional_intra_model", TOOLS / "directional_intra_model.py"
)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
VECTORS = Path(__file__).parent / "directional_intra_vectors.json"


class DirectionalIntraModelTests(TestCase):
    def test_locked_vectors_match(self) -> None:
        self.assertEqual(json.loads(VECTORS.read_text()), MODULE.vectors())

    def test_exact_cardinal_kernels_and_neutral_fallbacks(self) -> None:
        self.assertEqual(
            MODULE.cardinal_predict("vertical", 3, 2, [7, 8, 9], [20, 21]),
            [7, 8, 9, 7, 8, 9])
        self.assertEqual(
            MODULE.cardinal_predict("horizontal", 3, 2, [7, 8, 9], [20, 21]),
            [20, 20, 20, 21, 21, 21])
        self.assertEqual(
            set(MODULE.cardinal_predict("vertical", 4, 4, None, None)), {127})
        self.assertEqual(
            set(MODULE.cardinal_predict("horizontal", 4, 4, None, None)), {129})

    def test_short_edges_extend_last_reconstructed_sample(self) -> None:
        vertical = MODULE.cardinal_predict("vertical", 4, 2, [1, 2], [9])
        horizontal = MODULE.cardinal_predict("horizontal", 2, 4, [9], [1, 2])
        self.assertEqual(vertical[:4], [1, 2, 2, 2])
        self.assertEqual(horizontal[::2], [1, 2, 2, 2])

    def test_luma_and_chroma_syntax_surface(self) -> None:
        self.assertEqual(
            [MODULE.luma_mode_index(mode, 8, 8, None, None)
             for mode in MODULE.MODES], [0, 5, 6])
        self.assertEqual(MODULE.chroma_mode_symbol("vertical", "vertical"),
                         (1, 0))
        self.assertEqual(MODULE.chroma_mode_symbol("horizontal", "dc"),
                         (1, 1))
        state = MODULE.SyntaxState()
        vertical = MODULE.write_luma_mode(state, "vertical", 16, 16)
        self.assertEqual([event["name"] for event in vertical],
                         ["y_mode_set", "y_mode_index", "fsc_mode", "mrl_index"])

    def test_candidate_cost_is_stable_and_prefers_matching_direction(self) -> None:
        width, height = 8, 8
        horizontal = MODULE.generated_pattern("horizontal-gradient", width, height)
        vertical = MODULE.generated_pattern("vertical-gradient", width, height)
        h_result = MODULE.candidate_cost(
            horizontal, width, height, horizontal[:width],
            horizontal[::width][:height])
        v_result = MODULE.candidate_cost(
            vertical, width, height, vertical[:width],
            vertical[::width][:height])
        self.assertEqual(h_result[0]["mode"], "vertical")
        self.assertEqual(v_result[0]["mode"], "horizontal")
        flat = MODULE.candidate_cost(
            [128] * 64, 8, 8, [128] * 8, [128] * 8)
        self.assertEqual(flat[0]["mode"], "dc")

    def test_reconstruction_availability_crosses_leaf_and_superblock_edges(self) -> None:
        trace = MODULE.availability_trace()
        self.assertEqual(len(trace), 256)
        self.assertTrue(next(item for item in trace
                             if item["x"] == 64 and item["y"] == 0)["left"])
        self.assertTrue(next(item for item in trace
                             if item["x"] == 0 and item["y"] == 64)["above"])
        self.assertFalse(next(item for item in trace
                              if item["x"] == 0 and item["y"] == 64)["left"])

    def test_all_requested_formats_and_sizes_are_frozen(self) -> None:
        vectors = json.loads(VECTORS.read_text())
        keys = {(case["format"], tuple(case["luma_reference_size"]))
                for case in vectors["cases"]}
        self.assertEqual(
            keys, {(fmt, size) for fmt in MODULE.FORMATS for size in MODULE.SIZES})
        planes = {(case["format"], case["plane"]) for case in vectors["cases"]}
        self.assertEqual(
            planes,
            {("i400", "Y"), ("i420", "Y"), ("i420", "U"), ("i420", "V"),
             ("i444", "Y"), ("i444", "U"), ("i444", "V")})

    def test_project_decoder_cardinal_kernel_parity(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("project decoder parity disabled by unit-only run")
        root = Path(__file__).parents[3]
        decoder = Path(os.environ.get(
            "PROJECT_DECODER", root / "build/host-cmake/av2heif-decoder"))
        library = decoder.parent / "libav2heif-core.a"
        self.assertTrue(library.is_file(), f"project decoder library missing: {library}")
        output = Path(__file__).parents[1] / "build/directional-intra-project-driver"
        completed = subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
            "-Wpedantic", "-Werror", "-I", str(root / "include"),
            "-I", str(root / "src"),
            "-I", str(root / "experimental/encoder/include"),
            str(Path(__file__).parent / "directional_intra_project_driver.c"),
            str(root / "experimental/encoder/src/directional.c"),
            str(library), "-o", str(output),
        ], check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0,
                         completed.stderr.decode("utf-8", "replace"))
        executed = subprocess.run(
            [str(output)], check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        self.assertEqual(executed.returncode, 0,
                         executed.stderr.decode("utf-8", "replace"))

    def test_pinned_avm_cardinal_kernel_parity(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("pinned AVM parity disabled by unit-only run")
        root = Path(__file__).parents[3]
        avmdec = Path(os.environ.get(
            "AVMDEC", root / "build/reference/avm/avmdec"))
        library = avmdec.parent / "libavm.a"
        self.assertTrue(library.is_file(), f"pinned AVM library missing: {library}")
        output = Path(__file__).parents[1] / "build/directional-intra-avm-driver"
        completed = subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
            "-Wpedantic", "-Werror",
            str(Path(__file__).parent / "directional_intra_avm_driver.c"),
            str(library), "-lm", "-lpthread", "-o", str(output),
        ], check=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assertEqual(completed.returncode, 0,
                         completed.stderr.decode("utf-8", "replace"))
        executed = subprocess.run(
            [str(output)], check=False, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        self.assertEqual(executed.returncode, 0,
                         executed.stderr.decode("utf-8", "replace"))
