import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCES = sorted(path for path in (ROOT / "src").rglob("*")
                 if path.suffix in {".c", ".h"})
TOKENS = re.compile(
    r"(?P<comment>/\*.*?\*/|//[^\n]*)"
    r"|(?P<directive>^[ \t]*\#(?:\\\n|[^\n])*)"
    r"|(?P<string>\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*')"
    r"|(?P<word>[A-Za-z_]\w*)|(?P<symbol>[^\s])",
    re.M | re.S,
)
PREDICATES = {
    "av2h_coeff_is_low_frequency", "av2h_intrabc_bank_origin_valid",
    "is_directional_joint_mode", "block_allows_intrabc",
    "av2h_leaf_block_allows_palette", "plane_sample_reconstructed_cells",
    "plane_sample_inside_active_tile_cells", "av2h_plane_sample_reconstructed",
    "plane_sample_inside_active_tile", "plane_sample_reconstructed_u16",
    "plane_sample_inside_active_tile_u16", "tile_luma_lossless_unit_complete",
    "tile_chroma_lossless_unit_complete", "tile_cfl_allowed_for_sdp",
}


def layout_errors(text: str) -> list[str]:
    errors: list[str] = []
    declaration_seen = False
    function_seen = False
    depth = 0
    previous = ""
    for token in TOKENS.finditer(text):
        if token.lastgroup == "comment":
            continue
        value = token.group()
        if token.lastgroup == "directive":
            if re.match(r"\s*#\s*include\b", value) and declaration_seen:
                errors.append("include after declaration")
            if re.match(r"\s*#\s*define\b", value) and declaration_seen and depth == 0:
                errors.append("file-scope macro after declaration")
            continue
        if value == "typedef" and function_seen and depth == 0:
            errors.append("type definition after function")
        if value == "{":
            if depth == 0 and previous == ")":
                function_seen = True
            depth += 1
        elif value == "}":
            depth -= 1
        declaration_seen = True
        previous = value
    return errors


class SourceConsistencyTests(unittest.TestCase):
    def test_layout_guard_detects_late_directives_and_types(self) -> None:
        self.assertEqual(layout_errors(
            '#include "first.h"\n#define SIZE 4\n'
            'typedef int Value;\nint f(void) { return 0; }\n'), [])
        self.assertIn("include after declaration", layout_errors(
            'int f(void) { return 0; }\n#include "late.h"\n'))
        self.assertIn("file-scope macro after declaration", layout_errors(
            'typedef int Value;\n#define LATE 1\n'))
        self.assertIn("type definition after function", layout_errors(
            'int f(void) { return 0; }\ntypedef int Late;\n'))
        self.assertEqual(layout_errors(
            '/* #include "ignored.h" */\nint f(void) {\n'
            '#define LOCAL 1\nreturn LOCAL;\n#undef LOCAL\n}\n'), [])
        self.assertEqual(layout_errors(
            '#define WRAP(v) \\\n do { (v) = 1; } while (0)\n'
            '#include "after_macro.h"\nint value;\n'), [])

    def test_include_blocks_are_ordered(self) -> None:
        for path in SOURCES:
            for block in re.finditer(
                    r"^#include[^\n]*\n(?:[ \t]*\n|#include[^\n]*\n)*",
                    path.read_text(), re.M):
                lines = [line for line in block.group().splitlines() if line.strip()]
                names = [re.search(r'["<]([^">]+)[">]', line)[1] for line in lines]
                primary = None
                if path.suffix == ".c":
                    candidates = [name for name in names if name.endswith(".h") and
                                  Path(name).stem == path.stem]
                    if not candidates:
                        candidates = [name for name in names if name.endswith(".h") and
                                      path.stem.startswith(Path(name).stem + "_")]
                    if candidates:
                        primary = max(candidates, key=lambda name: len(Path(name).stem))
                    elif lines and '"' in lines[0] and names[0].endswith(".h"):
                        primary = names[0]
                keys = [
                    (0 if name == primary else 3 if name.endswith(".inc") else
                     2 if "<" in line else 1, name)
                    for line, name in zip(lines, names)]
                with self.subTest(path=path.relative_to(ROOT)):
                    self.assertEqual(keys, sorted(keys))

    def test_includes_macros_and_types_precede_code(self) -> None:
        for path in SOURCES:
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertEqual(layout_errors(path.read_text()), [])

    def test_null_users_include_stddef(self) -> None:
        for path in SOURCES:
            if path == ROOT / "src/shared/stddef.h":
                continue
            text = path.read_text()
            if re.search(r"\bNULL\b", text):
                with self.subTest(path=path.relative_to(ROOT)):
                    self.assertIn("#include <stddef.h>", text)

    def test_common_pointer_checks_use_null(self) -> None:
        pointers = r"(?:diagnostics|cdfs|writer|reader|decoder|pool|execution)"
        legacy = re.compile(r"\b" + pointers + r"\s*(?:==|!=)\s*0[Uu]?\b")
        for path in SOURCES:
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertIsNone(legacy.search(path.read_text()))

    def test_pointer_nulls_in_macro_bodies_and_arguments(self) -> None:
        pointers_by_file = {
            "src/decoder/frame_output.c": (
                "diagnostic_leaf", "result", "reconstruction_diagnostics"),
            "src/decoder/frame_workspace.c": ("workspace",),
        }
        for name, pointers in pointers_by_file.items():
            pattern = r"\b(?:" + "|".join(pointers) + r")\s*(?:==|!=)\s*0[Uu]?\b"
            with self.subTest(path=name):
                self.assertIsNone(re.search(pattern, (ROOT / name).read_text()))

    def test_predicates_return_int(self) -> None:
        names = "|".join(sorted(PREDICATES))
        declarations = re.compile(r"\b(int|uint8_t)\s+(" + names + r")\s*\(")
        found: set[str] = set()
        for path in SOURCES:
            for match in declarations.finditer(path.read_text()):
                with self.subTest(path=path.relative_to(ROOT), predicate=match[2]):
                    self.assertEqual(match[1], "int")
                found.add(match[2])
        self.assertEqual(found, PREDICATES)

    def test_boolean_storage_is_byte_sized(self) -> None:
        base = (ROOT / "src/base.h").read_text()
        self.assertEqual(base.count("uint8_t sizing_only;"), 3)
        pool = (ROOT / "src/runtime/concurrency.h").read_text()
        self.assertIn("volatile uint8_t stop;", pool)


if __name__ == "__main__":
    unittest.main()
