#!/usr/bin/env python3

import shutil
import subprocess
from pathlib import Path
from unittest import TestCase

from .validator_support import configured_path, require_validator


ROOT = Path(__file__).resolve().parents[3]
ENCODER_ROOT = ROOT / "experimental" / "encoder"
ENCODER = ENCODER_ROOT / "build" / "av2-encoder-experiment"
CHROMA_DRIVER = ENCODER_ROOT / "build" / "m3-chroma-driver"
FIXTURES = ENCODER_ROOT / "fixtures" / "image-input"
WORK = ENCODER_ROOT / "build" / "input-integration"


def chroma_pattern(
    format_id: int, width: int, height: int, values: tuple[int, int, int]
) -> bytes:
    result = bytearray()
    for plane, value in enumerate(values):
        plane_width = width if plane == 0 or format_id == 2 else (width + 1) // 2
        plane_height = (
            height if plane == 0 or format_id == 2 else (height + 1) // 2
        )
        for index in range(plane_width * plane_height):
            result.append(
                (index * 37 + plane * 71 + (index // plane_width) * 19) & 255
                if value == 256
                else value
            )
    return bytes(result)


class EncoderInputIntegrationTests(TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not ENCODER.is_file() or not CHROMA_DRIVER.is_file():
            raise RuntimeError("encoder integration executables are missing")
        shutil.rmtree(WORK, ignore_errors=True)
        WORK.mkdir(parents=True)

    @classmethod
    def tearDownClass(cls) -> None:
        shutil.rmtree(WORK, ignore_errors=True)

    def validators(self) -> tuple[Path, Path]:
        return (
            require_validator(
                self,
                configured_path(
                    "AVMDEC", ROOT / "build/reference/avm/avmdec"
                ),
                "pinned avmdec",
            ),
            require_validator(
                self,
                configured_path(
                    "PROJECT_DECODER",
                    ROOT / "build/host-cmake/av2heif-decoder",
                ),
                "project decoder",
            ),
        )

    def encode(self, source: Path, *options: str) -> Path:
        output = source.with_suffix(".obu")
        subprocess.run(
            [str(ENCODER), str(source), *options],
            cwd=ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.assertTrue(output.is_file())
        rerun = WORK / f"{source.stem}-rerun.obu"
        subprocess.run(
            [str(ENCODER), str(source), "--output", str(rerun), *options],
            cwd=ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.assertEqual(output.read_bytes(), rerun.read_bytes())
        return output

    def assert_parity(self, obu: Path, local: bytes, extension: str) -> None:
        avmdec, project = self.validators()
        avm = WORK / f"{obu.stem}.avm"
        project_output = WORK / f"{obu.stem}.project.{extension}"
        subprocess.run(
            [
                str(avmdec), "--codec=av2", "--rawvideo",
                "--output-bit-depth=8", "--limit=1",
                "-o", str(avm), str(obu),
            ],
            cwd=ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        subprocess.run(
            [str(project), str(obu), "-o", str(project_output)],
            cwd=ROOT,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        self.assertEqual(local, avm.read_bytes())
        self.assertEqual(local, project_output.read_bytes())

    def chroma_oracle(
        self, name: str, format_id: int, width: int, height: int,
        values: tuple[int, int, int],
    ) -> tuple[bytes, bytes]:
        obu = WORK / f"{name}-oracle.obu"
        local = WORK / f"{name}-oracle.raw"
        subprocess.run(
            [
                str(CHROMA_DRIVER), str(format_id), "100",
                str(width), str(height), *(str(value) for value in values),
                str(obu), str(local),
            ],
            cwd=ROOT,
            check=True,
        )
        return obu.read_bytes(), local.read_bytes()

    def test_five_extensions_odd_dimensions_and_local_parity(self) -> None:
        i400 = WORK / "source-3x5-gray8.i400"
        i400.write_bytes(bytes([128]) * 15)
        i400_obu = self.encode(i400)
        self.assert_parity(i400_obu, bytes([128]) * 15, "i400")

        i420_values = (256, 256, 256)
        i420 = WORK / "source-3x3-yuv420p8.i420"
        i420.write_bytes(chroma_pattern(0, 3, 3, i420_values))
        expected_obu, local = self.chroma_oracle(
            "i420", 0, 3, 3, i420_values
        )
        i420_obu = self.encode(i420)
        self.assertEqual(expected_obu, i420_obu.read_bytes())
        self.assert_parity(i420_obu, local, "i420")

        red_values = (54, 100, 255)
        i444 = WORK / "source-3x5-yuv444p8.i444"
        i444.write_bytes(chroma_pattern(2, 3, 5, red_values))
        expected_obu, local = self.chroma_oracle(
            "red", 2, 3, 5, red_values
        )
        i444_obu = self.encode(i444)
        self.assertEqual(expected_obu, i444_obu.read_bytes())
        self.assert_parity(i444_obu, local, "i444")

        for extension, fixture in (
            ("png", FIXTURES / "solid-rgba.png"),
            ("bmp", FIXTURES / "solid-color.bmp"),
        ):
            source = WORK / f"solid-red.{extension}"
            shutil.copyfile(fixture, source)
            obu = self.encode(source)
            self.assertEqual(expected_obu, obu.read_bytes())
            self.assert_parity(obu, local, "i444")

        gray = WORK / "solid-gray.png"
        shutil.copyfile(FIXTURES / "solid-gray.png", gray)
        gray_obu = self.encode(gray)
        self.assertEqual(i400_obu.read_bytes(), gray_obu.read_bytes())
        self.assert_parity(gray_obu, bytes([128]) * 15, "i400")

    def test_real_prepared_png_and_raw_parity(self) -> None:
        prepared = (
            ROOT / "images" / "prepared" / "le896" /
            "hong-kong-harbour-night"
        )
        cases = (
            (prepared / "master.png", "i444"),
            (prepared / "source-896x424-yuv420p8.i420", "i420"),
        )
        avmdec, project = self.validators()
        for source, extension in cases:
            with self.subTest(source=source.name):
                self.assertTrue(source.is_file())
                output = WORK / f"prepared-{extension}.obu"
                rerun = WORK / f"prepared-{extension}-rerun.obu"
                for destination in (output, rerun):
                    subprocess.run(
                        [
                            str(ENCODER), str(source),
                            "--output", str(destination),
                        ],
                        cwd=ROOT,
                        check=True,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                self.assertEqual(output.read_bytes(), rerun.read_bytes())
                avm = WORK / f"prepared-{extension}.avm"
                project_output = WORK / f"prepared-{extension}.project.{extension}"
                subprocess.run(
                    [
                        str(avmdec), "--codec=av2", "--rawvideo",
                        "--output-bit-depth=8", "--limit=1",
                        "-o", str(avm), str(output),
                    ],
                    cwd=ROOT,
                    check=True,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                subprocess.run(
                    [str(project), str(output), "-o", str(project_output)],
                    cwd=ROOT,
                    check=True,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                self.assertEqual(avm.read_bytes(), project_output.read_bytes())


if __name__ == "__main__":
    import unittest
    unittest.main()
