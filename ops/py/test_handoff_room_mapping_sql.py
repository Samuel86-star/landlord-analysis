"""Offline query behavior only; SQLite does not validate StarRocks integration."""
import re
import sqlite3
import unittest
from pathlib import Path


class RoomMappingQualityTest(unittest.TestCase):
    def test_missing_duplicate_and_cross_game_rooms_are_not_hidden(self):
        report = Path(__file__).resolve().parents[2] / "docs/analysis/result/2026-09-08-online-contract-verification.md"
        sql = re.search(r"```sql\n(.*?)\n```", report.read_text(), re.S).group(1)
        with sqlite3.connect(":memory:") as db:
            db.row_factory = sqlite3.Row
            db.execute("ATTACH DATABASE ':memory:' AS tcy_temp")
            db.execute("CREATE TABLE tcy_temp.dq_game_room_config (game_id, room_id, game_rule, room_level)")
            db.execute("CREATE TABLE tcy_temp.dws_ddz_daily_game (game_id, room_id, dt)")
            self.assertEqual(dict(db.execute(sql).fetchone())["fact_rows"], 0)
            db.executemany("INSERT INTO tcy_temp.dq_game_room_config VALUES (?, ?, ?, ?)", [
                (53, 420, "经典", "新手房"), (521, 420, "疯狂", "初级房")])
            db.execute("INSERT INTO tcy_temp.dws_ddz_daily_game VALUES (53, 420, '2026-06-25')")
            clean = dict(db.execute(sql).fetchone())
            self.assertEqual((clean["fact_rows"], clean["joined_rows"], clean["cross_game_reused_room_ids"]), (1, 1, 1))
            self.assertEqual([clean[key] for key in (
                "duplicate_keys", "conflicting_keys", "incomplete_keys", "inflated_rows", "unmapped_rows")], [0, 0, 0, 0, 0])
            db.execute("INSERT INTO tcy_temp.dq_game_room_config VALUES (53, 420, '不洗牌', '')")
            db.execute("INSERT INTO tcy_temp.dws_ddz_daily_game VALUES (53, 999, '2026-06-25')")
            db.execute("INSERT INTO tcy_temp.dws_ddz_daily_game VALUES (53, 420, '2026-07-02')")
            broken = dict(db.execute(sql).fetchone())
            self.assertEqual([broken[key] for key in (
                "duplicate_keys", "conflicting_keys", "incomplete_keys", "inflated_rows", "unmapped_rows")], [1, 1, 1, 1, 1])
            self.assertEqual((broken["fact_rows"], broken["joined_rows"]), (2, 3))


if __name__ == "__main__":
    unittest.main()
