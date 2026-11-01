"""Regression tests for Neverball package provenance and integrity."""
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import hashlib
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = spec_from_file_location(
    "verify_neverball_release", ROOT / "scripts" / "verify_neverball_release.py"
)
assert SPEC and SPEC.loader
VERIFY = module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class NeverballReleaseTests(unittest.TestCase):
    def make_package(self, root: Path) -> Path:
        package = root / "Neverball"
        (package / "data").mkdir(parents=True)
        (package / "Neverball").write_bytes(b"hunk executable")
        (package / "data" / "asset.dat").write_bytes(b"asset")
        lines = [
            f"{digest(package / 'Neverball')}  ./Neverball",
            f"{digest(package / 'data' / 'asset.dat')}  ./data/asset.dat",
        ]
        (package / "SHA256SUMS").write_text("\n".join(lines) + "\n")
        return package

    def test_complete_inventory_passes(self):
        with tempfile.TemporaryDirectory() as temp:
            package = self.make_package(Path(temp))
            self.assertEqual(VERIFY.verify_inventory(package), [])

    def test_modified_file_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            package = self.make_package(Path(temp))
            (package / "Neverball").write_bytes(b"changed")
            self.assertIn("SHA256 mismatch: Neverball", VERIFY.verify_inventory(package))

    def test_unlisted_and_absent_files_fail(self):
        with tempfile.TemporaryDirectory() as temp:
            package = self.make_package(Path(temp))
            with (package / "SHA256SUMS").open("a") as stream:
                stream.write(f"{'0' * 64}  ./ghost\n")
            (package / "extra").write_bytes(b"extra")
            failures = VERIFY.verify_inventory(package)
            self.assertTrue(any("missing files: extra" in item for item in failures))
            self.assertTrue(any("absent files: ghost" in item for item in failures))

    def test_build_requires_pin_and_embeds_inventory(self):
        text = (ROOT / "ports" / "neverball" / "build.sh").read_text()
        self.assertIn('git -C "$SRC" rev-parse HEAD', text)
        self.assertIn('cd "$PKG"', text)
        self.assertIn("mv .SHA256SUMS.tmp SHA256SUMS", text)
        self.assertIn('cp "$HERE/Install-Neverball"', text)

    def test_installer_is_a_required_package_file(self):
        self.assertIn("Install-Neverball", VERIFY.REQUIRED)
        installer = (ROOT / "ports" / "neverball" / "Install-Neverball").read_text()
        self.assertIn("@installer-version", installer)
        self.assertIn("(askdir", installer)
        self.assertIn("(copyfiles", installer)

    def test_68040_build_hardening_and_stack_contract(self):
        build = (ROOT / "ports" / "neverball" / "build.sh").read_text()
        patch = (
            ROOT
            / "ports"
            / "neverball"
            / "patches"
            / "0004-openup-68040-performance-profile.patch"
        ).read_text()
        self.assertIn("-m$CPU -m68881", build)
        self.assertIn("-fno-tree-loop-distribute-patterns", build)
        self.assertIn("check_m68040_fpcr.py", build)
        self.assertIn(" D ___stack$", build)
        self.assertIn("unsigned long __stack = 1024UL * 1024UL", patch)

    def test_performance_profile_preserves_one_binary_fallback(self):
        build = (ROOT / "ports" / "neverball" / "build.sh").read_text()
        patch = (
            ROOT
            / "ports"
            / "neverball"
            / "patches"
            / "0004-openup-68040-performance-profile.patch"
        ).read_text()
        self.assertIn('SDL_getenv("NEVERBALL_PROFILE")', patch)
        self.assertIn('strcmp(profile, "68040")', patch)
        self.assertIn("GL.module automatically chooses its CPU softpipe", build)
        self.assertIn("OpenMulticore is deliberately not a hard dependency", build)

    def test_verifier_requires_performance_contract(self):
        self.assertIn("OPENUP-PERFORMANCE.txt", VERIFY.REQUIRED)


if __name__ == "__main__":
    unittest.main()
