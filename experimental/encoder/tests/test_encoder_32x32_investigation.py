import importlib.util
from pathlib import Path
from unittest import TestCase


TOOL = Path(__file__).parents[1] / "tools/encoder_32x32_investigation.py"
SPEC = importlib.util.spec_from_file_location("encoder_32x32_investigation", TOOL)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class Encoder32x32InvestigationTests(TestCase):
    def test_controlled_cases(self) -> None:
        probes = MODULE.cases()
        self.assertEqual(set(probes), {"flat128", "flat96", "quadrants"})
        self.assertTrue(all(len(data) == 1024 for data in probes.values()))
        self.assertEqual(probes["quadrants"][0], 64)
        self.assertEqual(probes["quadrants"][31], 112)
        self.assertEqual(probes["quadrants"][31 * 32], 160)
        self.assertEqual(probes["quadrants"][-1], 208)

    def test_normative_32x32_selectors(self) -> None:
        facts = MODULE.normative_facts()
        self.assertEqual(facts["block_enum"]["32x32"], 9)
        self.assertEqual(facts["do_split_context_no_smaller_neighbors"]["32x32"], 8)
        self.assertEqual(facts["tx_partition_group"]["32x32"], 5)
        self.assertEqual(facts["tx_partition_type_group"]["32x32"], 6)
        self.assertEqual(facts["square_split_row"], [18000, 7, 0])
