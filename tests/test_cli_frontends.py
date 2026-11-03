import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FRONTENDS = [ROOT / "src/platform" / platform / "decoder.c"
             for platform in ("posix", "nolibc", "windows")]


class CliFrontendTests(unittest.TestCase):
    def test_frontends_only_call_shared_cli_and_platform_services(self) -> None:
        for path in FRONTENDS:
            text = path.read_text()
            calls = set(re.findall(r"\b(av2h_\w+)\s*\(", text))
            allowed = {"av2h_memory_copy", "av2h_task_pool_init",
                       "av2h_task_pool_execution", "av2h_task_pool_destroy"}
            with self.subTest(path=path.relative_to(ROOT)):
                self.assertEqual({name for name in calls
                                  if not name.startswith("av2h_cli_") and
                                  not re.fullmatch(
                                      r"av2h_(?:abi_open|"
                                      r"(?:abi|macos|linux)_syscall[1-6])",
                                      name)} - allowed,
                                 set())
                self.assertIn('#include "shared/cli_common.h"', text)
                self.assertNotRegex(
                    text, r'#include "(?:decoder/|preview\.h|shared/png_writer\.h)')
                self.assertNotRegex(text, r"\b(?:write_bmp|write_png|find_obu|"
                                    r"decode_obu|decode_stage_name|"
                                    r"coefficient_kind_name)\s*\(")
                self.assertNotIn("error:", text)
                self.assertNotIn('"{\\"status', text)
                self.assertNotRegex(text, r"header\s*\[\s*[01][Uu]?\s*\]")

    def test_cli_services_are_separate_from_core_archive(self) -> None:
        makefile = (ROOT / "Makefile").read_text()
        core = makefile.split("CORE_SOURCES :=", 1)[1].split("CLI_SOURCES :=", 1)[0]
        self.assertNotIn("cli_common.c", core)
        self.assertNotIn("cli_diagnostics.c", core)
        for platform in ("NATIVE", "NOLIBC", "HOST", "WINDOWS"):
            self.assertIn(f"$({platform}_CLI_OBJECTS)", makefile)
        self.assertIn("tests/cli_unit.c $(HOST_CLI_OBJECTS) $(HOST_LIBRARY)",
                      makefile)


if __name__ == "__main__":
    unittest.main()
