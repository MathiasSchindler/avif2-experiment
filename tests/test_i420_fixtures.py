import hashlib
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from generate_i420_fixtures import FIXTURES, generate  # noqa: E402


EXPECTED = {
    "tiny": (384, "ab42238158a28dd09c0b8770b45b66054f30010a7990d474e2d405fd8bb0b5cf"),
    "odd": (399, "d2335215ead313209788de6df81a202e57d7f499c18c779a1c22f22736f83252"),
    "ordinary": (
        28800,
        "6a8516bc3702135f8af853572503be21f8c47013739b3a3b0e5fa9261f624252",
    ),
    "high-detail": (
        11520,
        "b92b6949cafe80bb80660249c18830d49ffaf70e7310fb9711c3b681d3fa42b2",
    ),
}


class FixtureTests(unittest.TestCase):
    def test_fixture_bytes_are_stable(self) -> None:
        for fixture in FIXTURES:
            with self.subTest(fixture=fixture.name):
                data = generate(fixture)
                expected_size, expected_hash = EXPECTED[fixture.name]
                self.assertEqual(len(data), expected_size)
                self.assertEqual(hashlib.sha256(data).hexdigest(), expected_hash)


if __name__ == "__main__":
    unittest.main()