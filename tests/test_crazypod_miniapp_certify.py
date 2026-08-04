#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile


REPO_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = REPO_ROOT / "tools" / "crazypod_miniapp_certify.py"
SPEC = importlib.util.spec_from_file_location("miniapp_certify", MODULE_PATH)
certify = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(certify)


class MiniAppCertificationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.volume = self.root / "IPOD"
        info = self.volume / ".rockbox" / "rockbox-info.txt"
        info.parent.mkdir(parents=True)
        info.write_text(
            "Target: ipod6g\n"
            "Target id: 71\n"
            "Memory: 64\n"
            "Version: test\n",
            encoding="utf-8",
        )
        self.release = self.root / "release.zip"
        with zipfile.ZipFile(self.release, "w") as archive:
            archive.writestr(".rockbox/rockbox.ipod", b"firmware")
            for index in range(321):
                archive.writestr(
                    f".rockbox/test/{index:03}.bin", bytes([index % 251])
                )
            archive.writestr(
                ".rockbox/crazypod/miniapps/packages/"
                "game2048-5.0.1.cpk",
                b"game",
            )
            archive.writestr(
                ".rockbox/crazypod/miniapps/packages/"
                "capability-lab-5.0.1.cpk",
                b"lab",
            )
            archive.writestr(
                ".rockbox/crazypod/miniapps/packages/"
                "native-reference-1.0.0.cpk",
                b"reference",
            )
            archive.writestr(
                ".rockbox/crazypod/miniapps/packages/"
                "now-playing-neon-1.3.1.cpk",
                b"theme",
            )
        self.packages = (
            self.root / "game2048-5.0.1.cpk",
            self.root / "capability-lab-5.0.1.cpk",
            self.root / "native-reference-1.0.0.cpk",
            self.root / "now-playing-neon-1.3.1.cpk",
        )
        self.packages[0].write_bytes(b"game")
        self.packages[1].write_bytes(b"lab")
        self.packages[2].write_bytes(b"reference")
        self.packages[3].write_bytes(b"theme")

    def tearDown(self):
        self.temporary.cleanup()

    def test_preflight_rejects_wrong_target(self):
        info = self.volume / ".rockbox" / "rockbox-info.txt"
        info.write_text(
            "Target: ipod5g\nTarget id: 71\nMemory: 64\n",
            encoding="utf-8",
        )
        with self.assertRaises(certify.CertificationError):
            certify.preflight(self.volume, self.release, self.packages)

    def test_cli_accepts_an_explicit_release_variant(self):
        parsed = certify.parser().parse_args(
            [
                "install",
                str(self.volume),
                "--backup",
                str(self.root / "backup"),
                "--release",
                str(self.release),
                "--yes",
            ]
        )
        self.assertEqual(parsed.release, self.release)

    def test_stage_is_atomic_and_hash_checked(self):
        install = self.volume / "MiniApps"
        install.mkdir(parents=True)
        (install / "._stale.cpk").write_bytes(b"AppleDouble")
        result = certify.stage_packages(self.volume, self.packages)
        self.assertEqual(
            (install / self.packages[0].name).read_bytes(), b"game"
        )
        self.assertEqual(
            (install / self.packages[1].name).read_bytes(), b"lab"
        )
        self.assertEqual(len(result), 4)
        self.assertEqual(list(install.glob(".*.tmp-*")), [])
        self.assertEqual(list(install.glob("._*.cpk")), [])

    def test_install_backs_up_and_removes_only_stale_system_packages(self):
        package_directory = (
            self.volume / ".rockbox/crazypod/miniapps/packages"
        )
        package_directory.mkdir(parents=True)
        stale = package_directory / "game2048-4.0.1.cpk"
        stale.write_bytes(b"old")
        keep = self.volume / ".rockbox/user-kept.txt"
        keep.write_bytes(b"keep")
        backup = self.root / "backup"

        result = certify.install_release(
            self.volume, backup, self.release, self.packages
        )

        self.assertFalse(stale.exists())
        self.assertEqual(
            (
                backup
                / ".rockbox/crazypod/miniapps/packages"
                / stale.name
            ).read_bytes(),
            b"old",
        )
        self.assertEqual(keep.read_bytes(), b"keep")
        self.assertEqual(
            (self.volume / ".rockbox/rockbox.ipod").read_bytes(),
            b"firmware",
        )
        self.assertEqual(result["installedFiles"], 326)
        self.assertEqual(len(result["packageSha256"]), 4)

    def test_collect_creates_honest_evidence_template(self):
        log = (
            self.volume
            / ".crazypod"
            / "miniapps"
            / "capability-lab"
            / "miniapp.log"
        )
        log.parent.mkdir(parents=True)
        log.write_text("Native ABI mounted\n", encoding="utf-8")
        repro = self.volume / ".crazypod" / "repro"
        repro.mkdir(parents=True)
        (repro / "summary.json").write_text(
            '{"status":"PASS"}\n', encoding="utf-8"
        )
        output = self.root / "evidence"
        result = certify.collect_evidence(
            self.volume,
            output,
            self.release,
            self.packages,
        )
        self.assertEqual(
            result["reproFiles"],
            ["miniapp-repro/summary.json"],
        )
        verdict = (output / "verdict.md").read_text(encoding="utf-8")
        self.assertIn("NOT RUN", verdict)
        self.assertNotIn("PASS |", verdict)
        self.assertTrue((output / "environment.json").is_file())


if __name__ == "__main__":
    unittest.main()
