import subprocess
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]


class WorldOverviewIndependenceTests(unittest.TestCase):
    def test_builder_profile_and_core_have_no_graphics_framework_dependency(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "tools/check_world_overview_independence.py")],
            cwd=ROOT, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
