import re
import unittest
from pathlib import Path

from test_decoder_entrypoints import kernel_specializations


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
    "luma_top_superblock_boundary", "gdf_frame_arguments_valid", "gdf_frame_arguments_valid_u16",
}

KERNEL_BLOCK = re.compile(
    r'^/\* Instantiate typed kernels after the shared helpers\. \*/\n'
    r'#include "(?P<prefix>(?:internal/)?)sample_kernel_u8\.inc"\n'
    r'#include "(?P=prefix)(?P<kernel>\w+_kernel\.inc)"\n'
    r'#include "(?P=prefix)sample_kernel_end\.inc"\n'
    r'#include "(?P=prefix)sample_kernel_u16\.inc"\n'
    r'#include "(?P=prefix)(?P=kernel)"\n'
    r'#include "(?P=prefix)sample_kernel_end\.inc"\n', re.M)


def without_kernel_blocks(text: str, owner: str | None = None) -> str:
    blocks = list(KERNEL_BLOCK.finditer(text))
    if len(blocks) != 1 or owner is None:
        return text
    block = blocks[0]
    if block["kernel"] != owner + "_kernel.inc":
        return text
    tokens = list(TOKENS.finditer(text))
    section = [token for token in tokens
               if token.start() >= block.start() and token.end() <= block.end()]
    if [token.lastgroup for token in section] != ["comment"] + ["directive"] * 6:
        return text
    depth = 0
    conditional_depth = 0
    for token in tokens:
        if token.start() >= block.start():
            break
        if token.lastgroup == "symbol":
            depth += (token.group() == "{") - (token.group() == "}")
        elif token.lastgroup == "directive":
            if re.match(r"\s*#\s*(?:if|ifdef|ifndef)\b", token.group()):
                conditional_depth += 1
            elif re.match(r"\s*#\s*endif\b", token.group()):
                conditional_depth -= 1
    header = block["prefix"] == ""
    if depth != 0 or conditional_depth != int(header):
        return text
    following = [token for token in tokens
                 if token.start() >= block.end() and token.lastgroup != "comment"]
    if header:
        if (len(following) != 1 or following[0].lastgroup != "directive" or
                not re.match(r"\s*#\s*endif\b", following[0].group())):
            return text
    elif following:
        return text
    return text[:block.start()] + "\n" + text[block.end():]


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


def kernel_branch_errors(text: str) -> list[str]:
    errors: list[str] = []
    frames: list[tuple[int, list[str], list[str]]] = []
    closing = {")": "(", "]": "[", "}": "{"}

    def finish_branch(line: int, brackets: list[str], tokens: list[str]) -> None:
        if brackets:
            errors.append(f"line {line}: unclosed branch delimiters {''.join(brackets)}")
        if (any(token in {"if", "else", "for", "while", "switch", "do"} for token in tokens)
                and tokens[-1] not in {";", "}"}):
            errors.append(f"line {line}: split control-flow statement")

    for token in TOKENS.finditer(text):
        if token.lastgroup == "comment":
            continue
        value = token.group()
        if token.lastgroup == "directive":
            directive = re.match(r"\s*#\s*(\w+)", value)
            if directive is None:
                continue
            kind = directive[1]
            line = text.count("\n", 0, token.start()) + 1
            if kind in {"if", "ifdef", "ifndef"}:
                frames.append((line, [], []))
            elif kind in {"else", "elif", "endif"}:
                if not frames:
                    errors.append(f"line {line}: unmatched #{kind}")
                    continue
                finish_branch(*frames.pop())
                if kind != "endif":
                    frames.append((line, [], []))
            continue
        for line, brackets, tokens in frames:
            tokens.append(value)
            if token.lastgroup != "symbol":
                continue
            if value in {"(", "[", "{"}:
                brackets.append(value)
            elif value in closing:
                if not brackets or brackets[-1] != closing[value]:
                    errors.append(f"line {line}: unmatched branch delimiter {value}")
                else:
                    brackets.pop()
    for line, brackets, tokens in frames:
        finish_branch(line, brackets, tokens)
        errors.append(f"line {line}: unterminated conditional")
    return errors


class SourceConsistencyTests(unittest.TestCase):
    def test_coefficient_admission_uses_complete_context_banks(self) -> None:
        text = (ROOT / "src/decoder/coeff_level.c").read_text()
        self.assertNotRegex(text, r"\bq_context\s*==\s*[123]U\b")
        self.assertNotRegex(text, r"\bav2h_coeff_level_(?:base|eob|br)_row\s*\(")

    def test_kernel_branch_guard_detects_split_control_flow(self) -> None:
        for invalid in (
                "#if AV2H_KERNEL_WIDE\nif (wide) {\n#else\nif (low) {\n#endif\n}\n",
                "#if AV2H_KERNEL_WIDE\n} if (value) {\n#endif\n",
                "#if AV2H_KERNEL_WIDE\nif (wide)\n#else\nif (low)\n#endif\nreturn 0;\n",
                "#if AV2H_KERNEL_WIDE\nif (\n#else\nif (low)\n#endif\nvalue) return 0;\n",
                "#if AV2H_KERNEL_WIDE\nif (wide) {} else\n#endif\nreturn 0;\n",
                "#if AV2H_KERNEL_WIDE\n#if ENABLED\n{\n#endif\n}\n#endif\n"):
            with self.subTest(source=invalid):
                self.assertTrue(kernel_branch_errors(invalid))
        for valid in (
                "#if AV2H_KERNEL_WIDE\nif (wide) { return 0; }\n#else\nreturn 1;\n#endif\n",
                "if (value) {\n#if AV2H_KERNEL_WIDE\nvalue = load_wide();\n#endif\n}\n",
                "f(first,\n#if AV2H_KERNEL_WIDE\n&domain,\n#endif\nlast);\n",
                '#if AV2H_KERNEL_WIDE\nconst char *s = "} if (";\n/* {\n#else\n} */\n#endif\n',
                "#if AV2H_KERNEL_WIDE\nif (value)\n#if ENABLED\nreturn 1;\n#else\nreturn 0;\n#endif\n#endif\n"):
            with self.subTest(source=valid):
                self.assertEqual(kernel_branch_errors(valid), [])

    def test_kernel_branches_are_balanced(self) -> None:
        for template in (ROOT / "src/decoder/internal").glob("*_kernel.inc"):
            with self.subTest(template=template.name):
                self.assertEqual(kernel_branch_errors(template.read_text()), [])

    def test_divergent_admission_stays_outside_numerical_kernels(self) -> None:
        families = {
            "reconstruct": ("av2h_luma_reconstruct_to_plane", "av2h_chroma_reconstruct_to_plane"),
            "reconstruct_block": ("av2h_luma_reconstruct", "av2h_chroma_reconstruct"),
            "inverse_transform": ("av2h_reconstruct_transform_generic",),
            "chroma_prediction_cfl": ("av2h_cfl_derived_predict",),
            "chroma_prediction_mhccp": ("av2h_mhccp_predict_observed",),
            "intra_prediction_basic": ("av2h_predict_smooth", "av2h_predict_paeth"),
            "intra_prediction_directional": (),
        }
        for family, entrypoints in families.items():
            owner = ROOT / "src/decoder" / f"{family}.c"
            template = ROOT / "src/decoder/internal" / f"{family}_kernel.inc"
            with self.subTest(family=family):
                self.assertNotIn("AV2H_KERNEL_WIDE", template.read_text())
                for specialized in kernel_specializations(template):
                    definitions = re.findall(
                        r"^((?:static\s+)?\w+\s+\w+\s*\([^;{}]*\))\s*\{",
                        specialized, re.M)
                    self.assertTrue(definitions)
                    self.assertTrue(all(header.startswith("static ") for header in definitions))
                for entrypoint in entrypoints:
                    for suffix in ("", "_u16"):
                        self.assertRegex(
                            owner.read_text(), r"\bAv2hStatus\s+" + entrypoint + suffix + r"\s*\(")

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
                    without_kernel_blocks(path.read_text(), path.stem), re.M):
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
                self.assertEqual(layout_errors(
                    without_kernel_blocks(path.read_text(), path.stem)), [])

    def test_kernel_exception_requires_complete_owned_scopes(self) -> None:
        block = (
            '/* Instantiate typed kernels after the shared helpers. */\n'
            '#include "internal/sample_kernel_u8.inc"\n'
            '#include "internal/example_kernel.inc"\n'
            '#include "internal/sample_kernel_end.inc"\n'
            '#include "internal/sample_kernel_u16.inc"\n'
            '#include "internal/example_kernel.inc"\n'
            '#include "internal/sample_kernel_end.inc"\n')
        self.assertEqual(without_kernel_blocks(block, "example"), "\n")
        self.assertEqual(without_kernel_blocks(block, "other"), block)
        for invalid in (
                block.replace('/* Instantiate typed kernels after the shared helpers. */\n', ''),
                block.replace("sample_kernel_u16", "sample_kernel_u8"),
                block.replace('example_kernel.inc"\n', 'other_kernel.inc"\n', 1),
                block.rsplit("#include", 1)[0],
                block.replace("sample_kernel_end", "unrelated", 1)):
            with self.subTest(block=invalid):
                self.assertEqual(without_kernel_blocks(invalid, "example"), invalid)

    def test_kernel_exception_requires_a_trailing_file_scope_section(self) -> None:
        block = (
            '/* Instantiate typed kernels after the shared helpers. */\n'
            '#include "internal/sample_kernel_u8.inc"\n'
            '#include "internal/example_kernel.inc"\n'
            '#include "internal/sample_kernel_end.inc"\n'
            '#include "internal/sample_kernel_u16.inc"\n'
            '#include "internal/example_kernel.inc"\n'
            '#include "internal/sample_kernel_end.inc"\n')
        helper = 'static int helper(void) { return 0; }\n\n'
        self.assertEqual(without_kernel_blocks(helper + block, "example"), helper + "\n")
        for invalid in (
                'int f(void) {\n' + block + 'return 0;\n}\n',
                '#if ENABLED\n' + block + '#endif\n',
                '  #if ENABLED\n' + block + '  #endif\n',
                helper + block + 'int later(void) { return 0; }\n',
                helper + block + '#define LATE 1\n',
                helper + block + '#include "late.h"\n',
                '/*\n' + block + '*/\n',
                '#define EXPAND \\\n' + block,
                block + block,
                block.replace('internal/', '')):
            with self.subTest(source=invalid):
                self.assertEqual(without_kernel_blocks(invalid, "example"), invalid)
        header_block = block.replace('internal/', '')
        header = '#ifndef EXAMPLE_H\n#define EXAMPLE_H\n\n'
        self.assertEqual(
            without_kernel_blocks(header + helper + header_block + '\n#endif\n', "example"),
            header + helper + '\n\n#endif\n')
        for invalid in (
                header + helper + header_block,
                header + helper + header_block + '\nint later;\n#endif\n',
                header + helper + '#if ENABLED\n' + header_block + '#endif\n#endif\n'):
            with self.subTest(source=invalid):
                self.assertEqual(without_kernel_blocks(invalid, "example"), invalid)

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
        paths = [*SOURCES, *(ROOT / "src/decoder/internal").glob("*_kernel.inc")]
        for path in paths:
            sources = (kernel_specializations(path) if path.suffix == ".inc"
                       else [path.read_text()])
            for source in sources:
                for match in declarations.finditer(source):
                    with self.subTest(path=path.relative_to(ROOT), predicate=match[2]):
                        self.assertEqual(match[1], "int")
                    found.add(match[2])
        self.assertEqual(found, PREDICATES)

    def test_boolean_storage_is_byte_sized(self) -> None:
        base = (ROOT / "src/base.h").read_text()
        self.assertEqual(base.count("uint8_t sizing_only;"), 3)
        pool = (ROOT / "src/runtime/concurrency.h").read_text()
        self.assertIn("volatile uint8_t stop;", pool)

    def test_typed_kernels_have_one_scoped_owner(self) -> None:
        internal = ROOT / "src/decoder/internal"
        templates = sorted(internal.glob("*_kernel.inc"))
        self.assertEqual(len(templates), 17)
        for template in templates:
            stem = template.stem.removesuffix("_kernel")
            owners = [ROOT / "src/decoder" / (stem + ".c"), internal / (stem + ".h")]
            owners = [path for path in owners if path.exists() and
                      any(block["kernel"] == template.name
                          for block in KERNEL_BLOCK.finditer(path.read_text()))]
            with self.subTest(template=template.name):
                self.assertEqual(len(owners), 1)
                blocks = list(KERNEL_BLOCK.finditer(owners[0].read_text()))
                self.assertEqual(len(blocks), 1)
                self.assertEqual(blocks[0]["kernel"], template.name)
                self.assertEqual(blocks[0]["prefix"],
                                 "internal/" if owners[0].suffix == ".c" else "")
                self.assertNotEqual(
                    without_kernel_blocks(owners[0].read_text(), stem), owners[0].read_text())
                self.assertIn("AV2H_KERNEL_NAME(", template.read_text())
                for token in TOKENS.finditer(template.read_text()):
                    if token.lastgroup == "directive":
                        self.assertIsNone(re.match(
                            r"\s*#\s*(?:include|define|undef)\b", token.group()))
        for width in ("u8", "u16"):
            configuration = (internal / f"sample_kernel_{width}.inc").read_text()
            sample = "low" if width == "u8" else "high"
            self.assertEqual(configuration, (
                f"#define AV2H_KERNEL_WIDE {int(width == 'u16')}\n"
                f"#define AV2H_KERNEL_TYPE(low, high) {sample}\n"
                f"#define AV2H_KERNEL_NAME(low, high) {sample}\n"))
        self.assertEqual((internal / "sample_kernel_end.inc").read_text(), (
            "#undef AV2H_KERNEL_WIDE\n"
            "#undef AV2H_KERNEL_TYPE\n"
            "#undef AV2H_KERNEL_NAME\n"))

    def test_sample_depth_arithmetic_uses_domains(self) -> None:
        legacy = re.compile(
            r"\bav2h_sample_(?:clip|midpoint|below_midpoint|above_midpoint|maximum)_10bit\b")
        for path in (ROOT / "src/decoder").rglob("*"):
            if path.suffix not in {".c", ".h", ".inc"} or path.name == "sample_math.h":
                continue
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertIsNone(legacy.search(path.read_text()))
                if path.suffix == ".c":
                    self.assertIsNone(re.search(
                        r"bit_depth_idc\s*==\s*0U\s*\?\s*10U\s*:\s*8U", path.read_text()))


if __name__ == "__main__":
    unittest.main()
