import re
import unittest
from pathlib import Path


class DdzDailyGameSqlTest(unittest.TestCase):
    def test_mobile_newbie_room_1404_is_classic_like_pc_room_420(self):
        sql = (Path(__file__).parent / "batch_insert_ddz_daily_game.py").read_text(encoding="utf-8")
        classic_rooms = {
            room.strip()
            for room in re.search(r"room_id IN \(([^)]+)\) THEN 1", sql).group(1).split(",")
        }

        self.assertIn("420", classic_rooms)
        self.assertIn("1404", classic_rooms)


if __name__ == "__main__":
    unittest.main()
