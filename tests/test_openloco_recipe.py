"""Regression checks for the OpenLoco Amiga release recipe."""
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PORT = ROOT / "ports/openloco"


class OpenLocoRecipeTests(unittest.TestCase):
    def test_stack_and_post_link_contract_are_required(self):
        spec = json.loads((PORT / "source.json").read_text())
        port = spec["ports"]["openloco"]
        self.assertEqual(port["stackBytes"], 4 * 1024 * 1024)
        self.assertTrue(port["postLink68040"])
        runtime = next(x for x in port["cmake"] if x.startswith("-DOPENLOCO_AMIGA_RUNTIME_COMPAT_SOURCE="))
        self.assertIn("Stack.Amiga.cpp", runtime)
        self.assertIn("__stack = 4UL * 1024UL * 1024UL", (PORT / "compat/Stack.Amiga.cpp").read_text())

    def test_compiler_workaround_is_limited_to_known_failures(self):
        patch = (PORT / "patches/0009-scope-open3-compiler-ice-workaround.patch").read_text()
        self.assertNotIn("GLOB_RECURSE", patch)
        for name in ("IndustryList.cpp", "TerraForm.cpp", "TownList.cpp"):
            self.assertIn(name, patch)
        self.assertEqual(patch.count("src/Ui/Windows/"), 3)

    def test_openal_is_a_mandatory_receipted_input(self):
        spec = json.loads((PORT / "source.json").read_text())
        self.assertEqual(
            spec["externalInputEnvironment"],
            ["OPENAL_CONFIG", "OPENAL_LIBRARY", "OPENAL_MODULE"],
        )
        script = (PORT / "build-amiga.sh").read_text()
        self.assertIn("STOVE=$STAGED/prefix", script)
        self.assertIn("export OPENAL_CONFIG=", script)
        self.assertIn("openrtg-patches/*.patch", script)
        self.assertIn("scripts/build_sdl3_port.py", script)
        self.assertIn("$SDL3_SDK", script)

        openrtg_patch = (PORT / "openrtg-patches/0001-openal-use-exact-amiga-hook-result-type.patch").read_text()
        self.assertIn("(ULONG (*)())al_vhook_stub", openrtg_patch)
        self.assertIn("(ULONG (*)())fn", openrtg_patch)

    def test_package_entry_point_includes_native_installer(self):
        script = (PORT / "build-amiga.sh").read_text()
        installer = (PORT / "Install-OpenLoco").read_text()
        self.assertIn('cp "$HERE/Install-OpenLoco" "$OUT/Install-OpenLoco"', script)
        self.assertIn("@installer-version", installer)
        self.assertIn("LIBS:OpenGPU/OpenAL.module", installer)
        self.assertIn("(askbool", installer)


if __name__ == "__main__":
    unittest.main()
