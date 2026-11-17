"""Integration checks: real Clang/LLD, external project, and .tea rejection."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import warnings
import zipfile

SDK_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SDK_DIR))
import styleos_sdk as sdk


class SdkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        temp = tempfile.TemporaryDirectory(prefix="styleos-sdk-tests-")
        cls.addClassCleanup(temp.cleanup)
        cls.home = Path(temp.name)
        cls.demo = cls.home / "external project" / "presentations"
        shutil.copytree(SDK_DIR / "examples" / "presentations", cls.demo)
        cls.package = cls.home / "presentations.tea"
        cls.run_sdk("build", cls.demo, "--output", cls.package)
        with zipfile.ZipFile(cls.package) as zf:
            cls.meta = json.loads(zf.read(sdk.MANIFEST))
            cls.binary = zf.read(sdk.PAYLOAD)

    @classmethod
    def run_sdk(cls, *args):
        return subprocess.run([sys.executable, SDK_DIR / "styleos_sdk.py", *args],
                              cwd=cls.home, check=True, capture_output=True, text=True)

    def write_test_package(self, manifest=None, payload=None,
                           other=None, compression=zipfile.ZIP_STORED):
        with tempfile.TemporaryDirectory(dir=self.home) as base:
            name = hashlib.sha256(str(base).encode()).hexdigest()[:16]
        path = self.home / f"test-{name}.tea"
        if manifest is None:
            manifest = self.meta
        with zipfile.ZipFile(path, "w", compression=compression) as zf:
            zf.writestr(sdk.MANIFEST, sdk.encode(manifest) if isinstance(manifest, dict) else manifest)
            zf.writestr(sdk.PAYLOAD, self.binary if payload is None else payload)
            if other:
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    zf.writestr(other, b"unexpected")
        return path

    def test_valid_package_and_reproducibility(self):
        duplicate = self.home / "second.tea"
        self.run_sdk("build", self.demo, "--output", duplicate)
        self.assertEqual(self.package.read_bytes(), duplicate.read_bytes())
        self.assertEqual(sdk.verify(duplicate)["payload"]["sha256"],
                         hashlib.sha256(self.binary).hexdigest())
        self.assertIn("VERIFY PASS", self.run_sdk("verify", duplicate).stdout)

    def test_external_template_build_and_nonoverwrite(self):
        project = self.home / "own project"
        self.run_sdk("new", project, "--id", "example.hello", "--name", "Моё приложение")
        old = (project / "main.c").read_bytes()
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_sdk("new", project, "--id", "example.test", "--name", "Other")
        self.assertEqual((project / "main.c").read_bytes(), old)
        original_output = self.home / "my original app.tea"
        self.run_sdk("build", project, "--output", original_output)
        (project / "main.c").write_bytes(old.replace(b"Hello from my StyleOS app!", b"My own demo"))
        output = self.home / "my app.tea"
        self.run_sdk("build", project, "--output", output)
        self.assertEqual(sdk.verify(output)["project"]["id"], "example.hello")
        self.assertNotEqual(sdk.verify(output)["payload"]["sha256"],
                            sdk.verify(original_output)["payload"]["sha256"])

    def test_corrupted_payload(self):
        bytes_changed = self.binary[:-1] + bytes([self.binary[-1] ^ 1])
        path = self.write_test_package(payload=bytes_changed)
        with self.assertRaisesRegex(ValueError, "integrity"):
            sdk.verify(path)
        res = subprocess.run([sys.executable, SDK_DIR/"styleos_sdk.py", "verify", path],
                             cwd=self.home, capture_output=True, text=True)
        self.assertNotEqual(res.returncode, 0)
        self.assertNotIn("VERIFY PASS", res.stdout)

    @unittest.skipUnless(os.name == "posix", "isolated HOME is a POSIX fixture")
    def test_guided_workflow_creates_and_builds_project(self):
        home = self.home / "guided home"
        home.mkdir()
        env = os.environ | {"HOME": str(home)}
        command = [sys.executable, SDK_DIR / "styleos_sdk.py", "interactive"]
        subprocess.run([*command, "new"], cwd=self.home, env=env, check=True,
                       input="Guided App\nexample.guided\n", capture_output=True, text=True)
        project = home / "StyleOSProjects" / "example.guided"
        subprocess.run([*command, "build"], cwd=self.home, env=env, check=True,
                       input=f'"{project}"\n', capture_output=True, text=True)
        self.assertEqual(sdk.verify(project / "build" / "application.tea")["project"]["name"],
                         "Guided App")

    def test_rejected_manifest_architecture_and_interface(self):
        for override in ({"arch": "aarch64"}, {"interface": "unknown"},
                         {"format": "unknown"}, {"surprise": 1}):
            with self.subTest(override=override), self.assertRaises(ValueError):
                sdk.verify(self.write_test_package(manifest=self.meta | override))

    def test_wrong_elf_machine(self):
        data = bytearray(self.binary)
        struct.pack_into("<H", data, 18, 183)
        with self.assertRaisesRegex(ValueError, "architecture"):
            sdk.verify(self.write_test_package(payload=data))

    def test_entry_in_unmapped_memory(self):
        data = bytearray(self.binary)
        struct.pack_into("<Q", data, 24, 1)
        with self.assertRaisesRegex(ValueError, "entry"):
            sdk.verify(self.write_test_package(payload=data))

    def test_duplicate_member_and_extra_path_are_rejected(self):
        for name in (sdk.MANIFEST, "../sample"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                sdk.verify(self.write_test_package(other=name))

    def test_compression_and_duplicate_json_keys_are_rejected(self):
        with self.assertRaises(ValueError):
            sdk.verify(self.write_test_package(compression=zipfile.ZIP_DEFLATED))
        duplicate = sdk.encode(self.meta).replace(b'{', b'{"arch":"x86_64",', 1)
        with self.assertRaisesRegex(ValueError, "duplicate JSON"):
            sdk.verify(self.write_test_package(manifest=duplicate))

    def test_slide_callback_preserves_exact_text(self):
        cc = shutil.which("cc")
        if not cc:
            self.skipTest("host C compiler unavailable")
        exe = self.home / "text-callback-test"
        subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-I", SDK_DIR/"include", SDK_DIR/"examples/presentations/main.c",
                        SDK_DIR/"tests/test_presentations.c", "-o", exe],
                       check=True)
        subprocess.run([exe], check=True)


if __name__ == "__main__":
    unittest.main()
