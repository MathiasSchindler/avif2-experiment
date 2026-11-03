import re
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RETIRED = {
    "av2h_luma_reconstruct_without_predictor",
    "av2h_chroma_reconstruct_to_plane_ex",
    "av2h_chroma_reconstruct_to_plane_format",
    "av2h_luma_reconstruct_with_diagnostics",
    "av2h_luma_reconstruct_u16_with_diagnostics",
    "av2h_chroma_reconstruct_with_diagnostics",
    "av2h_chroma_reconstruct_u16_with_diagnostics",
    "av2h_luma_reconstruct_to_plane_with_diagnostics",
    "av2h_luma_reconstruct_to_plane_u16_with_diagnostics",
    "av2h_chroma_reconstruct_to_plane_format_with_diagnostics",
    "av2h_chroma_reconstruct_to_plane_u16_with_diagnostics",
    "av2h_reconstruct_transform_generic_with_diagnostics",
    "av2h_reconstruct_transform_generic_u16_with_diagnostics",
    "av2h_cdef_filter_frame_with_sequence",
    "av2h_cdef_filter_frame_u16_with_sequence",
    "av2h_ccso_filter_frame_with_sequence",
    "av2h_ccso_filter_frame_u16_with_sequence",
    "av2h_gdf_filter_frame_with_sequence",
    "av2h_gdf_filter_frame_u16_with_sequence",
    "av2h_cdef_filter_frame_with_controls",
    "av2h_cdef_filter_frame_u16_with_controls",
    "av2h_gdf_filter_frame_with_controls",
    "av2h_gdf_filter_frame_with_baseline",
    "av2h_gdf_filter_frame_u16_with_controls",
    "av2h_chroma_mode_read_with_delta",
    "av2h_chroma_mode_read_with_delta_for_sdp",
    "av2h_luma_mode_read_with_state_and_gdf",
    "av2h_luma_mode_read_with_state_and_delta_q",
    "av2h_coeff_cdfs_init_for_q",
    "av2h_coeff_block_read_default_context",
    "av2h_chroma_coeff_prefix_read_default_context",
    "av2h_chroma_reconstruct_dc_4x4",
    "av2h_chroma_transform_type_read_dc",
    "av2h_first_leaf_mode_read",
    "av2h_first_coeff_prefix_read",
    "av2h_copy_visible_planar_with_execution_private",
    "av2h_copy_visible_planar_u16_with_execution_private",
    "av2h_decode_frame_planar_with_execution_private",
    "av2h_decode_frame_group_planar_with_execution_private",
    "av2h_planar_output_size_bytes",
    "av2h_frame_decode_workspace_query_bytes",
    "av2h_frame_decode_workspace_init_bytes",
    "av2h_filter_workspace_query_bytes",
    "av2h_filter_workspace_init_bytes",
    "av2h_frame_header_closed_key_parse_diagnostic",
    "av2h_tile_decoder_init_at",
    "av2h_partition_node_read_for_structure",
}
TYPED_ADAPTERS = {
    "av2h_chroma_plane_coded_sample": "av2h_reference_load_u8",
    "av2h_chroma_plane_coded_sample_u16": "av2h_reference_load_u16",
    "av2h_binary_cdf_init": "av2h_cdf_init",
    "av2h_intrabc_integer_position": "av2h_sample_floor_shift",
    "av2h_luma_predict_u16": "av2h_luma_predict_u16_adjusted",
    "av2h_chroma_predict_u16": "av2h_luma_predict_u16_adjusted",
}

def kernel_specializations(path: Path):
    for width in ("u8", "u16"):
        source = (
            f'#include "decoder/internal/sample_kernel_{width}.inc"\n'
            f'#include "{path.relative_to(ROOT / "src")}"\n'
            '#include "decoder/internal/sample_kernel_end.inc"\n')
        yield subprocess.run(
            ["clang", "-E", "-P", "-x", "c", "-Isrc", "-Iinclude", "-"],
            input=source, cwd=ROOT, text=True, capture_output=True,
            check=True).stdout


class DecoderEntrypointTests(unittest.TestCase):
    def test_no_retired_entrypoints_in_code(self) -> None:
        pattern = re.compile(r"\b(?:" + "|".join(sorted(RETIRED)) + r")\b")
        for directory in ("src", "tests", "tools", "experimental"):
            for path in (ROOT / directory).rglob("*"):
                if path.suffix not in {".c", ".h", ".inc"}:
                    continue
                with self.subTest(path=path.relative_to(ROOT)):
                    self.assertIsNone(pattern.search(path.read_text()))

    def test_no_decoder_function_aliases(self) -> None:
        for path in (ROOT / "src/decoder").rglob("*.c"):
            text = path.read_text().replace("\\\n", " ")
            for match in re.finditer(
                r"^\s*#define\s+([A-Za-z_]\w*)(?:\([^)]*\))?\s+([A-Za-z_]\w*)",
                text, re.MULTILINE,
            ):
                with self.subTest(path=path.relative_to(ROOT), alias=match[1]):
                    self.assertFalse(match[2].startswith("av2h_"))

    def test_no_default_only_forwarding_functions(self) -> None:
        pattern = re.compile(
            r"^(?:static\s+)?(?:AV2H_NOINLINE\s+)?[A-Za-z_]\w*\s+"
            r"(av2h_\w+)\s*\([^;{}]*\)"
            r"\s*\{\s*(?:if\s*\(!av2h_sample_domain_valid\(domain\)\)"
            r"\s*return AV2H_INVALID_ARGUMENT;\s*)?"
            r"return\s+(av2h_\w+)\s*\([^;{}]*\);\s*\}",
            re.MULTILINE,
        )
        found = {}
        paths = [*(ROOT / "src/decoder").rglob("*.c"),
                 *(ROOT / "src/decoder/internal").glob("*_kernel.inc")]
        for path in paths:
            sources = (kernel_specializations(path) if path.suffix == ".inc"
                       else [path.read_text()])
            for source in sources:
                for match in pattern.finditer(source):
                    with self.subTest(path=path.relative_to(ROOT), function=match[1]):
                        self.assertIn(match[1], TYPED_ADAPTERS)
                        found[match[1]] = match[2]
        self.assertEqual(found, TYPED_ADAPTERS)


if __name__ == "__main__":
    unittest.main()
