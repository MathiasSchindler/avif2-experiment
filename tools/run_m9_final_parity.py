#!/usr/bin/env python3
"""Verify final filtered output for the four locked Milestone 9 fixtures."""

from __future__ import annotations

import argparse
import base64
import hashlib
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Fixture:
    name: str
    width: int
    height: int
    obu_base64: str | None
    obu_path: Path | None
    file_hash: str
    plane_hashes: tuple[str, str, str]


FIXTURES = (
    Fixture(
        "tiny",
        16,
        16,
        "AQgMBIIKM/8cOc3kbkIgYxDjADPQuHAA18NDhv8B/CpSqo9dY8WcNnNM6J7neHE1nCMyt6oYXIMXJQ/9tSoP21W51efIPuv1NXWy1tTBW+LGmUZHTlkUouA1I1BxW8BdJIulS8uQqXX8sLpZ8kPFc0Tu2A==",
        None,
        "5c18646460bb9f13c53baa6e65861a5cc746706a173e9c4f9b1cdeda86fd7ab5",
        (
            "140c6ee37d9ca689b9209ac66521eedf64bd6c3bc014a114f5bb96d6d83ac583",
            "c6e23fa3557ca3ed35870ab65becf341c018ab935f490a02a104e4d8a047b5ff",
            "f7d78cf2085d0c7ea77cb7585a232452054ae1585342052dac258a0e8b2d8dd9",
        ),
    ),
    Fixture(
        "odd",
        17,
        15,
        "AQgMBIIKQ4cOHObyNzJEaxDuwAz3wonCk8//wQAA/ypSZ9aA85mcWUL75lAVfW6SildKSdC3g64vHX/KLdNG9hJUpQofsY7hYEGj8Sm0W/OkfI6aLgpED1YhulH91RZ4POxL6gvP4fzuzyeSR2Ch1XBr3cULJWxyTfvJ",
        None,
        "74b176943ef8760c8971600c4e6ec25d6e3b6a4b21ffc3a55df845a41bdef9af",
        (
            "a1b908d31fc5dd4881a94c5d5bfba588a9ea6dce69f69847bbcea1897f0c7b3e",
            "aba8c6aed4df464bfd3da0432a2f07328129ab7157d60ad46ad2cc3773342bf7",
            "2af335e8378c30612953985fb4a1dede62f9c0a71209aba44c89a191b2ab71b0",
        ),
    ),
    Fixture(
        "ordinary",
        160,
        120,
        "AQgNBIIKdp/uOHubyNySILsGEO6QA+lVCidQYfcnmwDNd8VYX01Uf/Oe/JwJlbc3vRk4VIVnNwmxlom3mrm6mK7TbBQVK//43I64ZtcUCUmNr9VxZnZWiI01syhpySP0Nc5a3x0xqjgnKLYeo45oTCjkzIK/uNFimhNEOXwkIkrXZO///bg7bsP//bGGjQgzNCZANrHwBTYOGhqToDhokK4nOObAbjGYfi/o+NinESoPTVYXqdBlm3iqwaqApq/IfaPjg+wC2DuhCGwHX5Qf3aKs7d4+q87ZwYEyO2BZy3xypuolgQmEKx47vxK151bClBrLVscDQf/JH+RY+Gg5UcccdiGaR9xwdjMDX7w8TEk7gOg2wtZUOMmOn0H3KwVmeHuEOzaMqRx3x4Gyq9EsHr0kRFbFn3hgeMp2pTG+zm+In47B3s2eqbz/R+8G4a3EMvonAQFHsTPilHlUXGjRnzQDUitGHjGJYFXj9YcgLiblK5y7i69j2lccCT0UA92te0i5pGciHOA/HirP13oIxBN6hgiopFOlXsmuoqzcUszhhYtnGD0AGVtfzU5L3Dp31s5Rn85CDadKJLaIWllzbfe5CN+/JvAIC4Io6pqYIE4b43R1SA08SPyA9uMoFUETqLRJrxNbVf+1yHSiUsLseS+hC+LMMiOGhQ8sX9MrJvwmXPrq+UEmgdulHi6dpeVHEQvMkZvdUfjEKG3gDMofI+XsIl4bF60EQl+QISC8LY5871JPAhpE1PXca3TtvpimBppGhdktt4oFlgHZ2klGHN8hTL3UCQMWB6k6j9Dh3tM6xPOFlGA+AjekqAtNxV5TuA4qWvYXBIVBE15u4gcml+qqrwgB83Abp4BdW6ZQOHgun6aRc7WMNDqePHqq2I1HXmQxvfgqNpUzHrXKAyr73C8WfY1v2o65MnwPVNZjkZRXjmbccC9uXmK5bjQDRT3I1p1yBFdYuifv968we/OKg5ZvDheDdlb8KFwfwdej7R7qoGtVMhLgjOpFI686T8CxTOE5TPNeRk4oFrV+viHNIsh08bCwCRwuVJui5ig+WgeBA3IuxUeQzUT0U5sdgqMLVeLANaRpagxOxlWgIpn3QnKBORYghlezuIhBwsA=",
        None,
        "def0ced80c4bc1866d0d865fb392c80020ac0a823f310b2f62999d10eede1af5",
        (
            "8fb38e790d50f7e302fb1bd716e9830b53f99f832451a04ebe55cc83c55ca41d",
            "ab3a3252ebab6f46ba128c441fd68c51ebcace7ee235b0ed0f53a55b40b6ac8d",
            "17d3435a3bee634f90fc851832ae0c7366605f7b614b2a0df6c8fece4495f88f",
        ),
    ),
    Fixture(
        "high-detail",
        96,
        80,
        None,
        ROOT / "tests" / "fixtures" / "high-detail-96x80.obu",
        "4120930c9c9c49f73d4aa3e1d4c56cbb4b557679b42fe3b9de984bca1431745a",
        (
            "102e622eb635273997f0bc9e3ef1b865d186132909b5598a3a30ba8afefc93fd",
            "517916587d6dcc0a7f30fe1b9bc7e0cf8e0c0bd8a5eaa18e58a5d1b996f7aae7",
            "ec05707b0a64e2234fa8e268022ca3176beb027ce26c8fdd8afeda01ea2c89c1",
        ),
    ),
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fixture_bytes(fixture: Fixture) -> bytes:
    if fixture.obu_base64 is not None:
        return base64.b64decode(fixture.obu_base64, validate=True)
    assert fixture.obu_path is not None
    return fixture.obu_path.read_bytes()


def verify_fixture(decoder: Path, fixture: Fixture, directory: Path) -> None:
    source = directory / f"{fixture.name}.obu"
    output = directory / f"{fixture.name}.i420"
    source.write_bytes(fixture_bytes(fixture))
    subprocess.run(
        [str(decoder), str(source), "-o", str(output)],
        cwd=ROOT,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    samples = output.read_bytes()
    chroma_width = (fixture.width + 1) // 2
    chroma_height = (fixture.height + 1) // 2
    y_size = fixture.width * fixture.height
    uv_size = chroma_width * chroma_height
    if len(samples) != y_size + 2 * uv_size:
        raise RuntimeError(f"{fixture.name}: unexpected output size {len(samples)}")
    if digest(samples) != fixture.file_hash:
        raise RuntimeError(f"{fixture.name}: final I420 hash mismatch")
    planes = (samples[:y_size], samples[y_size : y_size + uv_size], samples[y_size + uv_size :])
    for name, plane, expected in zip("YUV", planes, fixture.plane_hashes):
        if digest(plane) != expected:
            raise RuntimeError(f"{fixture.name}: final {name} hash mismatch")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--decoder", type=Path, required=True)
    args = parser.parse_args()
    decoder = args.decoder.resolve()
    if not decoder.is_file():
        raise RuntimeError(f"missing decoder: {decoder}")
    with tempfile.TemporaryDirectory(prefix="av2h-m9-") as temporary:
        directory = Path(temporary)
        for fixture in FIXTURES:
            verify_fixture(decoder, fixture, directory)
            print(f"{fixture.name}: final Y/U/V exact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
