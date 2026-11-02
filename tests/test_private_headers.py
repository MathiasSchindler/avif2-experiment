import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.check_private_headers import (  # noqa: E402
    check_cycles,
    check_kernel_dependencies,
    include_graph,
    parse_dependencies,
)


class PrivateHeaderTests(unittest.TestCase):
    def test_guarded_cycle_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            first = root / "first.h"
            second = root / "second.h"
            first.write_text(
                '#ifndef FIRST_H\n#define FIRST_H\n#include "second.h"\n#endif\n')
            second.write_text(
                '#ifndef SECOND_H\n#define SECOND_H\n#include "first.h"\n#endif\n')
            with self.assertRaisesRegex(RuntimeError, "include cycle"):
                check_cycles(include_graph([first], [root]))

    def test_shared_dependency_is_not_a_cycle(self) -> None:
        first, second, common = map(Path, ("first.h", "second.h", "common.h"))
        check_cycles({first: {second, common}, second: {common}, common: set()})

    def test_relative_include_and_missing_include(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            first = root / "first.h"
            second = root / "second.h"
            first.write_text('#include "second.h"\n')
            second.write_text("")
            graph = include_graph([first], [])
            self.assertEqual(graph[first], {second.resolve()})
            second.unlink()
            with self.assertRaisesRegex(RuntimeError, "missing quoted include"):
                include_graph([first], [])

    def test_compiler_dependency_escaping(self) -> None:
        root = ROOT / "project"
        self.assertEqual(
            parse_dependencies("private-header: src/value.h \\\n src/a\\ b.h\n", root),
            {root / "src/value.h", root / "src/a b.h"})
        with self.assertRaisesRegex(RuntimeError, "no dependency rule"):
            parse_dependencies("", root)

    def test_transitive_parser_dependency_is_rejected(self) -> None:
        source, value, parser, table = map(
            Path, ("kernel.c", "value.h", "frame.h", "numeric.inc"))
        check_kernel_dependencies(source, {value, table}, {value})
        with self.assertRaisesRegex(RuntimeError, "frame.h"):
            check_kernel_dependencies(source, {value, parser}, {value})


if __name__ == "__main__":
    unittest.main()
