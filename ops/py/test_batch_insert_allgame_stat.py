import ast
import sqlite3
import unittest
from pathlib import Path


class AllGameStatSqlTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (Path(__file__).parent / "batch_insert_allgame_stat.py").read_text(encoding="utf-8")
        tree = ast.parse(cls.source)
        cls.insert_template = next(
            ast.literal_eval(node.value)
            for node in tree.body
            if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == "INSERT_TEMPLATE" for target in node.targets)
        )

    def test_bomb_buckets_do_not_divide_multiplier(self):
        self.assertNotIn("g.bomb_bet / 2", self.source)
        self.assertIn("g.bomb_bet DIV 2 = 1", self.source)
        self.assertIn("g.bomb_bet DIV 2 = 2", self.source)
        self.assertIn("g.bomb_bet DIV 2 >= 3", self.source)

    def test_ddz_source_is_scoped_to_requested_app(self):
        db = sqlite3.connect(":memory:")
        db.execute(
            "CREATE TABLE dws_ddz_daily_game "
            "(app_id, uid, play_mode, game_datetime, game_id, dt, robot, group_id)"
        )
        db.executemany(
            "INSERT INTO dws_ddz_daily_game VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
            [
                (101, 7, 1, "2026-09-01 10:00:00", 53, "2026-09-01", 0, 6),
                (202, 7, 1, "2026-09-01 11:00:00", 53, "2026-09-01", 0, 6),
                (101, 8, 1, "2026-09-01 12:00:00", 53, "2026-09-01", 1, 6),
                (101, 9, 1, "2026-09-01 13:00:00", 53, "2026-09-01", 0, 1),
                (101, 10, 1, "2026-09-02 10:00:00", 53, "2026-09-02", 0, 6),
            ],
        )
        ddz_modes_template = self.insert_template.split("WITH ddz_modes AS (\n", 1)[1].split(
            "\n),\nddz_streaks AS (", 1
        )[0]
        for app_id in (101, 202):
            ddz_modes = ddz_modes_template.format(app_id=app_id, dt="2026-09-01")
            query = (
                "WITH ddz_modes AS (\n" + ddz_modes.replace(
                    "tcy_temp.dws_ddz_daily_game", "dws_ddz_daily_game"
                ) + "\n) SELECT app_id, uid FROM ddz_modes"
            )
            self.assertEqual(db.execute(query).fetchall(), [(app_id, 7)])

        self.assertIn("AND app_id = {app_id}", ddz_modes_template)


if __name__ == "__main__":
    unittest.main()
