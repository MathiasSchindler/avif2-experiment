#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental/encoder"
BUILD = ENCODER / "build"
ENCODER_BIN = BUILD / "av2-encoder-experiment"
AVMDEC = ROOT / "build/reference/avm/avmdec"
PROJECT_DECODER = ROOT / "build/host-cmake/av2heif-decoder"
SCARLET = ROOT / "images/prepared/le896/scarlet-flycatcher/source-896x598-gray8.i400"
PASTEL = ROOT / "images/prepared/le896/pastel-chalks/source-896x598-gray8.i400"
FRONTIER_FIXTURE = ENCODER / "fixtures/blocky-m1-corpus-frontier"


def run(command: list[str]) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def tokens(stderr: bytes) -> dict[str, str]:
    result: dict[str, str] = {}
    for item in stderr.decode("utf-8", errors="replace").split():
        if "=" in item:
            key, value = item.split("=", 1)
            result[key] = value
    return result


def crop_i400(
    source: bytes, source_width: int,
    x: int, y: int, width: int, height: int,
) -> bytes:
    return b"".join(
        source[(y + row) * source_width + x:
               (y + row) * source_width + x + width]
        for row in range(height)
    )


class M1CandidateOracleValidationTests(unittest.TestCase):
    def require_tools(self) -> None:
        if os.environ.get("ENCODER_TEST_UNIT_ONLY") == "1":
            self.skipTest("external validator integration disabled")
        for path in (ENCODER_BIN, AVMDEC, PROJECT_DECODER):
            if not path.is_file():
                self.skipTest(f"required executable is unavailable: {path}")

    def validate_decode(
        self, work: Path, source: bytes, rd_path: Path, expected_sse: int,
    ) -> None:
        avm_path = work / "decoded.avm.i400"
        project_path = work / "decoded.project.i400"
        for command in (
            [
                str(AVMDEC), "--codec=av2", "--rawvideo", "--limit=1",
                "--output-bit-depth=8", "-o", str(avm_path), str(rd_path),
            ],
            [str(PROJECT_DECODER), str(rd_path), "-o", str(project_path)],
        ):
            completed = run(command)
            self.assertEqual(
                completed.returncode, 0,
                completed.stderr.decode("utf-8", errors="replace"),
            )
        avm = avm_path.read_bytes()
        self.assertEqual(avm, project_path.read_bytes())
        self.assertEqual(len(avm), len(source))
        self.assertEqual(
            sum((a - b) ** 2 for a, b in zip(source, avm)), expected_sse)

    def encode(
        self, work: Path, source: bytes, width: int, height: int,
        qindex: int, search: str,
    ) -> tuple[Path, dict[str, str]]:
        source_path = work / "source.i400"
        output = work / f"{search}.obu"
        source_path.write_bytes(source)
        completed = run([
            str(ENCODER_BIN), str(source_path),
            "--width", str(width), "--height", str(height),
            "--qindex", str(qindex), "--search", search,
            "--output", str(output),
        ])
        self.assertEqual(
            completed.returncode, 0,
            completed.stderr.decode("utf-8", errors="replace"),
        )
        return output, tokens(completed.stderr)

    def test_non_dc_ac_candidate_is_validator_exact_pareto_win(self) -> None:
        self.require_tools()
        work = BUILD / "m1-candidate-oracle-validator"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        try:
            source = bytes(
                min(255, 32 + x * 4 + y * 11)
                for y in range(16) for x in range(16)
            )
            source_path = work / "source.i400"
            source_path.write_bytes(source)
            results: dict[str, tuple[Path, dict[str, str]]] = {}
            for search in ("bounded", "rd"):
                output = work / f"{search}.obu"
                completed = run([
                    str(ENCODER_BIN), str(source_path),
                    "--width", "16", "--height", "16",
                    "--qindex", "100", "--search", search,
                    "--output", str(output),
                ])
                self.assertEqual(
                    completed.returncode, 0,
                    completed.stderr.decode("utf-8", errors="replace"),
                )
                results[search] = output, tokens(completed.stderr)
            bounded_path, bounded = results["bounded"]
            rd_path, rd = results["rd"]
            self.assertEqual((bounded_path.stat().st_size, bounded["sse_y"]),
                             (45, "1624"))
            self.assertEqual((rd_path.stat().st_size, rd["sse_y"]), (43, "1574"))
            self.assertEqual(rd["rd_mode_smooth_v"], "1")
            self.assertEqual(rd["rd_residual_ac"], "1")
            self.assertEqual(rd["rd_trials"], "21")
            self.validate_decode(work, source, rd_path, 1574)
        finally:
            shutil.rmtree(work, ignore_errors=True)

    def test_paeth_ac_omits_forbidden_secondary_transform_symbol(self) -> None:
        self.require_tools()
        work = BUILD / "m1-candidate-oracle-paeth"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        try:
            source = bytes(
                min(255, 16 + x * 12 + y * 3)
                for y in range(16) for x in range(16)
            )
            source_path = work / "source.i400"
            rd_path = work / "rd.obu"
            source_path.write_bytes(source)
            completed = run([
                str(ENCODER_BIN), str(source_path),
                "--width", "16", "--height", "16",
                "--qindex", "40", "--search", "rd",
                "--output", str(rd_path),
            ])
            self.assertEqual(
                completed.returncode, 0,
                completed.stderr.decode("utf-8", errors="replace"),
            )
            rd = tokens(completed.stderr)
            self.assertEqual((rd_path.stat().st_size, rd["sse_y"]),
                             (61, "175153"))
            self.assertEqual(rd["rd_mode_paeth"], "1")
            self.assertEqual(rd["rd_residual_ac"], "1")
            self.validate_decode(work, source, rd_path, 175153)
        finally:
            shutil.rmtree(work, ignore_errors=True)

    def test_prepared_crop_oracle_wins_remain_validator_exact(self) -> None:
        self.require_tools()
        manifest = json.loads(
            (FRONTIER_FIXTURE / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["id"],
                         "prepared-scarlet-flycatcher-le896-i400-crop-512x336-q60")
        self.assertEqual(manifest["searches"]["bounded"]["bytes"], 806)
        self.assertEqual(manifest["searches"]["rd"]["bytes"], 800)
        cases = (
            (
                "scarlet", (FRONTIER_FIXTURE / "source.i400").read_bytes(),
                60, (806, 1653563), (800, 967184),
            ),
            (
                "pastel", crop_i400(
                    PASTEL.read_bytes(), 896, 432, 240, 64, 64),
                141, (185, 136118), (181, 131763),
            ),
        )
        root = BUILD / "m1-candidate-oracle-prepared"
        shutil.rmtree(root, ignore_errors=True)
        try:
            for name, source, qindex, bounded_expected, rd_expected in cases:
                with self.subTest(name=name):
                    work = root / name
                    work.mkdir(parents=True)
                    bounded_path, bounded = self.encode(
                        work, source, 64, 64, qindex, "bounded")
                    rd_path, rd = self.encode(
                        work, source, 64, 64, qindex, "rd")
                    self.assertEqual(
                        (bounded_path.stat().st_size, int(bounded["sse_y"])),
                        bounded_expected,
                    )
                    self.assertEqual(
                        (rd_path.stat().st_size, int(rd["sse_y"])),
                        rd_expected,
                    )
                    self.assertLess(rd_path.stat().st_size,
                                    bounded_path.stat().st_size)
                    self.assertLess(int(rd["sse_y"]), int(bounded["sse_y"]))
                    self.assertGreater(int(rd["rd_trials"]), 0)
                    self.validate_decode(
                        work, source, rd_path, rd_expected[1])
        finally:
            shutil.rmtree(root, ignore_errors=True)

    def test_partial_boundary_full_search_is_reachable_but_not_admitted(self) -> None:
        self.require_tools()
        source = crop_i400(
            SCARLET.read_bytes(), 896, 448, 320, 128, 70)
        work = BUILD / "m1-candidate-oracle-partial-boundary"
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        try:
            bounded_path, bounded = self.encode(
                work, source, 128, 70, 60, "bounded")
            rd_path, rd = self.encode(work, source, 128, 70, 60, "rd")
            self.assertEqual(rd_path.read_bytes(), bounded_path.read_bytes())
            self.assertEqual(rd["sse_y"], bounded["sse_y"])
            self.assertGreater(int(rd["rd_trials"]), 0)
            self.validate_decode(work, source, rd_path, int(rd["sse_y"]))
        finally:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
