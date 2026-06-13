import unittest
from types import SimpleNamespace
from unittest.mock import patch

import main


class DiskUsageTest(unittest.TestCase):
    def test_usage_percent_excludes_reserved_blocks(self):
        usage = SimpleNamespace(
            total=58_733_723_648,
            used=52_135_510_016,
            free=3_581_513_728,
        )

        with patch.object(main.shutil, "disk_usage", return_value=usage):
            disk = main.get_disk_usage("/")

        self.assertEqual(disk["usage_percent"], 93.57)


if __name__ == "__main__":
    unittest.main()
