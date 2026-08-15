import importlib.util
import json
import sys
from pathlib import Path
from unittest import TestCase


TOOLS = Path(__file__).parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
TOOL = TOOLS / "m3_large_partition_model.py"
SPEC = importlib.util.spec_from_file_location("m3_large_partition_model", TOOL)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class M3LargePartitionModelTests(TestCase):
    def test_locked_multileaf_reconstruction_vectors(self) -> None:
        vectors = json.loads(
            (
                Path(__file__).parent
                / "m3_multileaf_reconstruction_vectors.json"
            ).read_text()
        )
        self.assertEqual(
            vectors["schema"], "av2-m3-multileaf-reconstruction-v1"
        )
        self.assertEqual(
            vectors["cases"],
            [MODULE.reconstruction_model(*case) for case in MODULE.CASES],
        )

    def test_locked_nonflat_avm_probe_evidence(self) -> None:
        report = json.loads(
            (
                Path(__file__).parent / "m3_multileaf_avm_probes.json"
            ).read_text()
        )
        self.assertEqual(
            report["avm_revision"],
            "966a7d7cd6fcf60360caf5dc413b2aeeb65e144d",
        )
        self.assertTrue(report["avm_all_pass"])
        self.assertFalse(report["project_all_pass"])
        self.assertTrue(
            all(
                case["project_decoder_returncode"] == 1
                and "integer overflow" in case["project_decoder_stderr"]
                for case in report["cases"]
            )
        )

    def test_every_dimension_17_through_64_is_covered_once(self) -> None:
        for width in range(17, 65):
            for height in range(17, 65):
                result = MODULE.model(width, height)
                self.assertTrue(
                    result["active_grid_exact_cover"], (width, height)
                )
                self.assertTrue(
                    result["above_left_dependency_safe"], (width, height)
                )
                self.assertEqual(result["square_split_symbols"], 0)
                self.assertTrue(
                    all(
                        leaf["width"] in (8, 16)
                        and leaf["height"] in (8, 16)
                        for leaf in result["leaves"]
                    )
                )

    def test_32x32_uses_rectangles_not_square_split(self) -> None:
        result = MODULE.model(32, 32)
        coded = [
            decision for decision in result["decisions"]
            if not decision["boundary_implied"]
        ]
        self.assertEqual(coded[0]["node"], "32x32")
        self.assertEqual(coded[0]["partition"], "HORZ")
        self.assertEqual(coded[0]["symbols"], [["do_split", 1],
                                               ["rect_type", 0]])
        self.assertNotIn(
            "do_square_split",
            [symbol[0] for decision in coded for symbol in decision["symbols"]],
        )
        self.assertEqual(
            [(leaf["x"], leaf["y"]) for leaf in result["leaves"]],
            [(0, 0), (16, 0), (0, 16), (16, 16)],
        )

    def test_contexts_follow_committed_leaf_sizes(self) -> None:
        result = MODULE.model(64, 64)
        leaf_none = [
            decision for decision in result["decisions"]
            if decision["partition"] == "NONE"
        ]
        self.assertEqual(len(leaf_none), 16)
        self.assertEqual(leaf_none[0]["contexts"]["split"], 4)
        internal = [
            decision for decision in result["decisions"]
            if decision["partition"] != "NONE"
            and not decision["boundary_implied"]
        ]
        self.assertTrue(
            any(
                decision["contexts"]["split"]
                > MODULE.GEOMETRY[decision["node"]][2] * 4
                for decision in internal
            )
        )
        self.assertFalse(result["global_leaf_order_is_raster"])

    def test_pixel_dimensions_collapse_to_four_pixel_classes(self) -> None:
        for width in range(17, 65):
            for height in range(17, 65):
                rounded_width = (width + 7) // 8 * 8
                rounded_height = (height + 7) // 8 * 8
                a = MODULE.model(width, height)
                b = MODULE.model(rounded_width, rounded_height)
                self.assertEqual(a["decisions"], b["decisions"])
                self.assertEqual(a["leaves"], b["leaves"])

    def test_source_extension_repeats_active_edges(self) -> None:
        source = MODULE.source_samples(17, 31)
        padded = MODULE.edge_extend(source, 17, 31)
        self.assertEqual(len(padded), 24 * 32)
        self.assertEqual(padded[16], padded[17])
        self.assertEqual(padded[16], padded[23])
        self.assertEqual(padded[30 * 24:31 * 24],
                         padded[31 * 24:32 * 24])

    def test_32x32_decoder_order_explains_legacy_sse_divergence(self) -> None:
        result = MODULE.reconstruction_model(32, 32)
        self.assertEqual(
            result["transform_order"][:8],
            [
                [0, 0], [8, 0], [0, 8], [8, 8],
                [16, 0], [24, 0], [16, 8], [24, 8],
            ],
        )
        self.assertEqual(result["first_legacy_order_difference"], 2)
        self.assertFalse(result["legacy_raster_matches_decoder_order"])
        self.assertFalse(result["legacy_local_decoder_exact"])
        self.assertNotEqual(
            result["legacy_raster_squared_error"],
            result["legacy_stream_decoder_squared_error"],
        )
        self.assertTrue(result["local_decoder_exact"])

    def test_contexts_update_after_each_transform(self) -> None:
        transforms = MODULE.reconstruction_model(32, 32)["first_transforms"]
        self.assertEqual(transforms[0]["all_zero_context"], 1)
        self.assertEqual(transforms[0]["dc_sign_context"], 0)
        self.assertNotEqual(transforms[1]["dc_sign_context"], 0)
        self.assertGreaterEqual(transforms[2]["all_zero_context"], 1)

    def test_representatives_crop_local_canvas_like_decoder(self) -> None:
        for width, height in MODULE.CASES:
            result = MODULE.reconstruction_model(width, height)
            self.assertTrue(result["local_decoder_exact"], (width, height))
            self.assertEqual(
                result["local_reconstruction_sha256"],
                result["decoder_reconstruction_sha256"],
            )
            self.assertEqual(
                result["transform_count"],
                result["coded_width"] // 8 * (result["coded_height"] // 8),
            )
