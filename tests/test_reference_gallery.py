import hashlib
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
REFERENCE = DOCS / "reference"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class ReferenceGalleryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads(
            (REFERENCE / "manifest.json").read_text(encoding="utf-8")
        )

    def test_gallery_page_uses_shared_wasm_decoder(self) -> None:
        html = (DOCS / "gallery.html").read_text(encoding="utf-8")
        stylesheet = (DOCS / "gallery.css").read_text(encoding="utf-8")
        javascript = (DOCS / "gallery.js").read_text(encoding="utf-8")
        self.assertIn('src="gallery.js?', html)
        self.assertIn('fetch("decoder.wasm?', javascript)
        self.assertIn('fetch("reference/manifest.json"', javascript)
        self.assertIn("IntersectionObserver", javascript)
        self.assertIn("detailZoom", javascript)
        self.assertIn("[hidden] { display: none !important; }", stylesheet)

    def test_manifest_covers_published_corpus(self) -> None:
        manifest = self.manifest
        self.assertEqual(manifest["schema"], "av2h-reference-gallery-v1")
        self.assertEqual(manifest["source_count"], 14)
        self.assertEqual(manifest["variant_count"], 88)
        self.assertEqual(len(manifest["sources"]), 14)
        self.assertEqual(
            sum(len(source["variants"]) for source in manifest["sources"]),
            88,
        )
        self.assertEqual(
            len({source["id"] for source in manifest["sources"]}), 14
        )

    def test_streams_and_reviewed_metadata_are_exact(self) -> None:
        variant_ids = set()
        for source in self.manifest["sources"]:
            metadata_path = REFERENCE / source["metadata"]
            metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
            self.assertEqual(metadata["id"], source["id"])
            self.assertEqual(metadata["creator"], source["creator"])
            self.assertEqual(metadata["license"]["spdx"], source["license"]["spdx"])
            self.assertTrue((REFERENCE / source["license"]["local_file"]).is_file())
            for variant in source["variants"]:
                self.assertNotIn(variant["id"], variant_ids)
                variant_ids.add(variant["id"])
                stream = REFERENCE / variant["file"]
                self.assertEqual(stream.stat().st_size, variant["bytes"])
                self.assertEqual(sha256(stream), variant["sha256"])
                self.assertGreater(variant["decoded_bytes"], 0)
                self.assertEqual(len(variant["decoded_sha256"]), 64)
        self.assertEqual(len(variant_ids), 88)
        self.assertEqual(len(list((REFERENCE / "streams").glob("*.obu"))), 88)


if __name__ == "__main__":
    unittest.main()