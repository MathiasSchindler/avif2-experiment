import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
LEGACY = re.compile(r"\b(?:Av2hFirst\w*|av2h_first_\w*|AV2H_FIRST_\w*)\b")
FIRST_ELEMENT_PARSERS = {
    "av2h_first_coeff_prefix_read": "src/decoder/coeff.c",
    "av2h_first_leaf_mode_read": "src/decoder/leaf.c",
}
TYPE_OWNERS = {
    "Av2hIntraMode": "leaf_mode.h",
    "Av2hLeafMode": "leaf_mode.h",
    "Av2hChromaLeafMode": "leaf_mode.h",
    "Av2hCoeffPrefix": "coeff_block.h",
    "Av2hCoeffBlock": "coeff_block.h",
    "Av2hCoeffCdfs": "coeff.h",
    "Av2hCoeffLevelCdfs": "coeff_level.h",
    "Av2hTxType": "transform_type.h",
    "Av2hTransformType": "transform_type.h",
    "Av2hTxSize": "tx_geometry.h",
    "Av2hTransformCdfs": "transform.h",
    "Av2hBlockReconstruction": "reconstruct.h",
    "Av2hBlockReconstructionU16": "reconstruct.h",
    "Av2hPartitionPrefix": "partition.h",
    "Av2hLeafPath": "partition.h",
}


class DecoderNameTests(unittest.TestCase):
    def test_no_general_purpose_first_names(self) -> None:
        for directory in ("src", "include", "tests", "tools", "experimental"):
            for path in (ROOT / directory).rglob("*"):
                if path.suffix not in {".c", ".h", ".inc", ".S"} and not (
                    directory == "tools" and path.suffix == ".py"
                ):
                    continue
                for match in LEGACY.finditer(path.read_text()):
                    with self.subTest(path=path.relative_to(ROOT), name=match[0]):
                        self.assertIn(match[0], FIRST_ELEMENT_PARSERS)

    def test_canonical_types_have_one_owner(self) -> None:
        headers = sorted((ROOT / "src/decoder").rglob("*.h"))
        for name, owner in TYPE_OWNERS.items():
            declarations = [
                path.relative_to(ROOT / "src/decoder").as_posix()
                for path in headers
                for _ in re.finditer(r"}\s+" + re.escape(name) + r"\s*;", path.read_text())
            ]
            with self.subTest(name=name):
                self.assertEqual(declarations, [owner])

    def test_first_element_exceptions_reject_non_origin_leaves(self) -> None:
        for name, source in FIRST_ELEMENT_PARSERS.items():
            text = (ROOT / source).read_text()
            definition = re.search(
                r"\bAv2hStatus\s+" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{",
                text,
            )
            self.assertIsNotNone(definition)
            assert definition is not None
            start = definition.end()
            end = start
            depth = 1
            while depth and end < len(text):
                depth += (text[end] == "{") - (text[end] == "}")
                end += 1
            self.assertEqual(depth, 0, f"Unclosed function body: {name}")
            body = text[start:end]
            with self.subTest(name=name):
                self.assertIn("leaf->row != 0U || leaf->column != 0U", body)
                self.assertIn("return AV2H_UNSUPPORTED;", body)


if __name__ == "__main__":
    unittest.main()
