import importlib.util
import json
import sys
from pathlib import Path
from unittest import TestCase


TOOLS = Path(__file__).parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
TOOL = TOOLS / "m3_odd_model.py"
SPEC = importlib.util.spec_from_file_location("m3_odd_model", TOOL)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class M3OddModelTests(TestCase):
    def test_locked_vectors_match_model(self) -> None:
        vectors = json.loads(
            (Path(__file__).parent / "m3_odd_model_vectors.json").read_text()
        )
        self.assertEqual(vectors["schema"], "av2-m3-odd-model-v1")
        self.assertEqual(
            vectors["cases"],
            [MODULE.model(width, height) for width, height in MODULE.CASES],
        )

    def test_coded_dimensions_and_partition_shapes(self) -> None:
        expected = {
            (1, 1): "8x8",
            (7, 9): "8x16",
            (9, 7): "16x8",
            (15, 15): "16x16",
            (1, 16): "8x16",
            (16, 1): "16x8",
        }
        for dimensions, shape in expected.items():
            vector = MODULE.model(*dimensions)
            self.assertEqual(vector["prediction_block"], shape)
            self.assertEqual(vector["transform_size"], "8x8")

    def test_source_extension_repeats_last_active_row_and_column(self) -> None:
        source = bytes(range(1, 64))
        padded = MODULE.edge_extend(source, 7, 9)
        self.assertEqual(len(padded), 8 * 16)
        self.assertEqual(padded[6], padded[7])
        self.assertEqual(padded[8 * 8:8 * 9], padded[8 * 9:8 * 10])
        self.assertEqual(padded[-1], source[-1])

    def test_forced_path_and_output_crop(self) -> None:
        small = MODULE.model(1, 1)
        self.assertEqual(
            [step["coded_symbol"] for step in small["partition_path"]],
            [False, False, False, False, False, False, True],
        )
        self.assertEqual(small["blocks"][0]["active_width"], 1)
        self.assertEqual(small["blocks"][0]["active_height"], 1)
        tall = MODULE.model(7, 9)
        self.assertEqual(tall["partition_path"][-2]["action"], "implied_VERT")
        self.assertEqual(len(tall["blocks"]), 2)
        self.assertEqual(tall["blocks"][1]["active_height"], 1)

    def test_contexts_follow_raster_reconstructed_neighbors(self) -> None:
        square = MODULE.model(15, 15)
        blocks = square["blocks"]
        self.assertEqual(blocks[0]["all_zero_context"], 1)
        self.assertEqual(blocks[0]["dc_sign_context"], 0)
        for index, neighbors in ((1, (0,)), (2, (0,)), (3, (1, 2))):
            expected_skip = (
                sum(min(4, abs(blocks[n]["level"])) for n in neighbors) + 3
            ) >> 1
            score = sum(
                -1 if blocks[n]["level"] < 0 else
                1 if blocks[n]["level"] > 0 else 0
                for n in neighbors
            )
            self.assertEqual(blocks[index]["all_zero_context"], expected_skip)
            self.assertEqual(
                blocks[index]["dc_sign_context"],
                1 if score < 0 else 2 if score > 0 else 0,
            )

    def test_dc_uses_full_transform_not_visible_sample_count(self) -> None:
        one = MODULE.model(1, 1)
        block = one["blocks"][0]
        self.assertEqual(block["padded_sum"], 64 * 64)
        self.assertEqual(one["dc_both_edge_divisor"], 16)
