import unittest
from pathlib import Path


class AllGameStatSqlTest(unittest.TestCase):
    def test_bomb_buckets_do_not_divide_multiplier(self):
        sql = (Path(__file__).parent / "batch_insert_allgame_stat.py").read_text(encoding="utf-8")
        self.assertNotIn("g.bomb_bet / 2", sql)
        self.assertIn("g.bomb_bet DIV 2 = 1", sql)
        self.assertIn("g.bomb_bet DIV 2 = 2", sql)
        self.assertIn("g.bomb_bet DIV 2 >= 3", sql)


if __name__ == "__main__":
    unittest.main()
