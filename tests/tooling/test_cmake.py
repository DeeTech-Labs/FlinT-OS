import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("cmake"), "CMake is not installed")
class CMakeLayoutTests(unittest.TestCase):
    def sdk_fixture(self, work, revision):
        (work / "cmake").mkdir()
        shutil.copyfile(ROOT / "cmake/flint_platform.cmake", work / "cmake/flint_platform.cmake")
        header = work / "platform/include/flint/contracts.h"
        header.parent.mkdir(parents=True)
        header.write_text(f"inline constexpr uint32_t ContractRevision = {revision};\n", encoding="utf-8")
        script = work / "check.cmake"
        script.write_text(
            f'include("{work.as_posix()}/cmake/flint_platform.cmake")\n'
            'file(REAL_PATH "${FLINT_PLATFORM_DIR}" resolved)\n'
            'file(WRITE "sdk-path.txt" "${resolved}")\n', encoding="utf-8")
        return script

    def test_default_sdk_is_the_pinned_submodule(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            script = self.sdk_fixture(work, "0x20261113")
            subprocess.run(["cmake", "-P", str(script)], cwd=work, check=True,
                           capture_output=True, text=True)
            self.assertEqual(Path((work / "sdk-path.txt").read_text()).resolve(),
                             (work / "platform").resolve())

    def test_old_sdk_contract_is_rejected_with_explicit_checkout_hint(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            script = self.sdk_fixture(work, "0x20261112")
            result = subprocess.run(["cmake", "-P", str(script)], cwd=work,
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("ContractRevision 0x20261113", result.stderr)
            self.assertIn("-DFLINT_PLATFORM_DIR=../Flint-IgniteSDK", result.stderr)
            self.assertFalse((work / "sdk-path.txt").exists())

    def test_version_header_is_generated_in_build_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            script = work / "check.cmake"
            script.write_text(
                f'include("{ROOT.as_posix()}/cmake/flint_version.cmake")\n'
                'file(WRITE "version.txt" "${PROJECT_VER}")\n', encoding="utf-8")
            subprocess.run(["cmake", "-P", str(script)], cwd=work, check=True,
                           capture_output=True, text=True)
            header = (work / "generated/config/flint_version.h").read_text()
            version = (work / "version.txt").read_text()
            self.assertIn(f'#define FLINT_OS_VERSION_STRING "{version}"', header)
            self.assertNotIn("@FLINT_", header)


if __name__ == "__main__":
    unittest.main()
