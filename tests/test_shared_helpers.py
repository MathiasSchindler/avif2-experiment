import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANONICAL_OWNERS = {
    "av2h_ceil_log2_u32": "src/shared/bit_math.h",
    "av2h_floor_log2_u32": "src/shared/bit_math.h",
    "av2h_ceil_log2_ratio_u32": "src/shared/bit_math.h",
    "av2h_load_u16le": "src/base.h",
    "av2h_load_u32le": "src/base.h",
    "av2h_store_u16le": "src/base.h",
    "av2h_store_u32le": "src/base.h",
    "av2h_store_u16be": "src/base.h",
    "av2h_store_u32be": "src/base.h",
    "av2h_sample_clamp": "src/decoder/internal/sample_math.h",
    "av2h_sample_abs": "src/decoder/internal/sample_math.h",
    "av2h_sample_block_log2": "src/decoder/internal/sample_math.h",
    "av2h_sample_floor_shift": "src/decoder/internal/sample_math.h",
    "av2h_sample_round_pow2": "src/decoder/internal/sample_math.h",
    "av2h_sample_round_pow2_signed": "src/decoder/internal/sample_math.h",
}
LEGACY_HELPER = re.compile(
    r"^\s*static\s+(?:inline\s+)?[\w\s*]+?\b("
    r"clamp|ccso_clamp|clamp_coordinate|absolute_value|"
    r"round_shift\w*|floor_shift\w*|lossless_floor_shift|"
    r"av2h_intra_round_shift|intrabc_round_shift|"
    r"\w*ceil_log2\w*|\w*floor_log2\w*|tile_log2_u32|"
    r"reciprocal_msb|mhccp_log2|coeff_dim_log2|recon_size_log2|"
    r"block_log2|av2h_intra_block_log2|store_u(?:16|32|64)(?:le|be)"
    r")\s*\(",
    re.MULTILINE,
)


class SharedHelperTests(unittest.TestCase):
    def test_no_legacy_helper_definitions(self) -> None:
        violations = []
        for path in sorted((ROOT / "src").rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            for match in LEGACY_HELPER.finditer(path.read_text()):
                if CANONICAL_OWNERS.get(match.group(1)) == str(path.relative_to(ROOT)):
                    continue
                violations.append(f"{path.relative_to(ROOT)}: {match.group(1)}")
        self.assertEqual(violations, [], "\n".join(violations))

    def test_canonical_helpers_have_one_owner(self) -> None:
        definitions = re.compile(
            r"^\s*static\s+inline\s+[\w\s*]+?\b("
            + "|".join(CANONICAL_OWNERS)
            + r")\s*\(",
            re.MULTILINE,
        )
        owners = {name: [] for name in CANONICAL_OWNERS}
        for path in sorted((ROOT / "src").rglob("*")):
            if path.suffix not in (".c", ".h"):
                continue
            for match in definitions.finditer(path.read_text()):
                owners[match.group(1)].append(str(path.relative_to(ROOT)))
        for name, expected in CANONICAL_OWNERS.items():
            with self.subTest(helper=name):
                self.assertEqual(owners[name], [expected])


if __name__ == "__main__":
    unittest.main()
