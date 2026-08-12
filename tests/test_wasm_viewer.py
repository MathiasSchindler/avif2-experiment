import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PUBLIC_HEADER = ROOT / "include" / "av2heif.h"
FRAME_OUTPUT_HEADER = ROOT / "src" / "decoder" / "frame_output.h"
VIEWER_HTML = ROOT / "docs" / "index.html"
VIEWER_JS = ROOT / "docs" / "viewer.js"


def enum_members(source: str, enum_name: str) -> list[str]:
    match = re.search(
        rf"typedef enum \{{(?P<body>.*?)\}} {re.escape(enum_name)};",
        source,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"missing {enum_name} declaration")
    return [
        member.split("=", 1)[0].strip()
        for member in match.group("body").split(",")
        if member.strip()
    ]


def string_array(source: str, name: str) -> list[str]:
    match = re.search(
        rf"const {re.escape(name)} = \[(?P<body>.*?)\];",
        source,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"missing {name} array")
    return re.findall(r'"([^"]*)"', match.group("body"))


class WasmViewerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.public_header = PUBLIC_HEADER.read_text(encoding="ascii")
        self.frame_output_header = FRAME_OUTPUT_HEADER.read_text(encoding="ascii")
        self.html = VIEWER_HTML.read_text(encoding="utf-8")
        self.javascript = VIEWER_JS.read_text(encoding="ascii")

    def test_diagnostic_names_follow_decoder_enums(self) -> None:
        status_labels = {
            "AV2H_OK": "OK",
            "AV2H_INVALID_ARGUMENT": "invalid argument",
            "AV2H_TRUNCATED": "truncated input",
            "AV2H_INVALID_DATA": "invalid data",
            "AV2H_OVERFLOW": "integer overflow",
            "AV2H_LIMIT_EXCEEDED": "limit exceeded",
            "AV2H_OUT_OF_MEMORY": "out of memory",
            "AV2H_UNSUPPORTED": "unsupported feature",
        }
        stage_labels = {
            "AV2H_FRAME_DECODE_STAGE_NONE": "none",
            "AV2H_FRAME_DECODE_STAGE_INITIALIZE": "initialization",
            "AV2H_FRAME_DECODE_STAGE_LUMA_PARSE": "luma parsing",
            "AV2H_FRAME_DECODE_STAGE_LUMA_RECONSTRUCT": "luma reconstruction",
            "AV2H_FRAME_DECODE_STAGE_LUMA_DEBLOCK": "luma deblock",
            "AV2H_FRAME_DECODE_STAGE_LUMA_COMMIT": "luma commit",
            "AV2H_FRAME_DECODE_STAGE_CHROMA_PARSE": "chroma parsing",
            "AV2H_FRAME_DECODE_STAGE_CHROMA_CROSS_TRANSFORM": "chroma cross-transform",
            "AV2H_FRAME_DECODE_STAGE_CHROMA_RECONSTRUCT": "chroma reconstruction",
            "AV2H_FRAME_DECODE_STAGE_CHROMA_DEBLOCK": "chroma deblock",
            "AV2H_FRAME_DECODE_STAGE_CHROMA_COMMIT": "chroma commit",
            "AV2H_FRAME_DECODE_STAGE_COMPLETENESS": "completeness check",
            "AV2H_FRAME_DECODE_STAGE_FILTER": "filtering",
            "AV2H_FRAME_DECODE_STAGE_OUTPUT": "output",
        }

        status_members = enum_members(self.public_header, "Av2hStatus")
        stage_members = enum_members(
            self.frame_output_header, "Av2hFrameDecodeStage"
        )
        self.assertEqual(
            string_array(self.javascript, "statusNames"),
            [status_labels[member] for member in status_members],
        )
        self.assertEqual(
            string_array(self.javascript, "decodeStageNames"),
            [stage_labels[member] for member in stage_members],
        )

    def test_drop_and_cache_regression_guards(self) -> None:
        self.assertIn('id="drop-target"', self.html)
        for event_name in ("dragenter", "dragover", "dragleave", "drop"):
            self.assertIn(
                f'window.addEventListener("{event_name}"', self.javascript
            )
        self.assertRegex(
            self.javascript,
            r'fetch\("decoder\.wasm\?v=[^"]+", \{ cache: "no-cache" \}\)',
        )
        self.assertRegex(
            self.html,
            r'<script type="module" src="viewer\.js\?v=[^"]+">',
        )

    def test_local_file_uses_single_image_mode(self) -> None:
        self.assertIn('class="local-only">Local OBU</span>', self.html)
        self.assertIn('classList.toggle("local-mode", localMode)', self.javascript)
        self.assertIn('elements.comparison.hidden = localMode', self.javascript)
        self.assertIn('if (!localSource) copyCanvas(', self.javascript)


if __name__ == "__main__":
    unittest.main()