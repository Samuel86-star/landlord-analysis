#!/usr/bin/env python3
"""Offline checks for the two SQL blocks in the handoff validation document."""

import json
import re
import sqlite3
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DOCUMENT = ROOT / "docs/analysis/result/2026-09-08-historical-backfill-validation.md"


def load_sql(filename: str) -> str:
    text = DOCUMENT.read_text(encoding="utf-8")
    pattern = rf"```sql\n-- file: {re.escape(filename)}\n(.*?)\n```"
    match = re.search(pattern, text, re.DOTALL)
    assert match, f"SQL block not found: {filename}"
    sql = re.sub(r"/\*\+.*?\*/", "", match.group(1))
    return re.sub(r"\bDIV\b", "/", sql)


def get_json_int(value: str | None, path: str) -> int | None:
    if value is None or path != "$.card_power.shuffle_times":
        return None
    try:
        result = json.loads(value).get("card_power", {}).get("shuffle_times")
        return int(result) if result is not None else None
    except (AttributeError, TypeError, ValueError, json.JSONDecodeError):
        return None


def setup_database() -> sqlite3.Connection:
    db = sqlite3.connect(":memory:")
    db.row_factory = sqlite3.Row
    db.create_function("get_json_int", 2, get_json_int)
    db.execute("ATTACH DATABASE ':memory:' AS tcy_temp")
    db.executescript(
        """
        CREATE TABLE tcy_temp.dws_ddz_daily_game (
            game_id INTEGER, app_id INTEGER, uid INTEGER, dt TEXT,
            robot INTEGER, group_id INTEGER, play_mode INTEGER,
            hand_cards TEXT, shuffle_times INTEGER, extend_content TEXT,
            bomb_bet INTEGER
        );
        CREATE TABLE tcy_temp.dws_app_allgame_stat (
            app_id INTEGER, uid INTEGER, dt TEXT, play_mode INTEGER,
            game_count INTEGER, bomb_0_games INTEGER, bomb_1_games INTEGER,
            bomb_2_games INTEGER, bomb_3plus_games INTEGER
        );
        """
    )
    return db


def insert_fixtures(db: sqlite3.Connection) -> None:
    source_rows = [
        (53, 1880053, 101, "2026-06-25", 0, 6, 1, "A", -1, "{}", 1),
        (53, 1880053, 101, "2026-06-25", 0, 6, 1, "B", 1, '{"card_power":{"shuffle_times":1}}', 2),
        (53, 1880053, 202, "2026-06-25", 0, 6, 1, "C", None, "{}", 4),
        (53, 1880053, 303, "2026-06-26", 0, 66, 2, "D", 1, '{"card_power":{"shuffle_times":1}}', 2),
        (53, 1880053, 404, "2026-06-26", 0, 66, 2, "E", 1, '{"card_power":{"shuffle_times":1}}', 4),
        (53, 1880053, 505, "2026-06-27", 0, 8, 3, "F", 1, '{"card_power":{"shuffle_times":1}}', 3),
        (53, 1880053, 606, "2026-06-28", 0, 8, 3, "G", 1, '{"card_power":{"shuffle_times":1}}', 8),
        (53, 1880053, 607, "2026-06-28", 0, 8, 3, "H", 1, '{"card_power":{"shuffle_times":1}}', 6),
        (53, 1880053, 608, "2026-06-28", 0, 8, 3, "I", 1, '{"card_power":{"shuffle_times":1}}', None),
        (53, 1880053, 609, "2026-06-28", 0, 8, 3, "J", 1, '{"card_power":{"shuffle_times":1}}', 0),
        (53, 9999999, 707, "2026-06-29", 0, 33, 1, "K", 1, '{"card_power":{"shuffle_times":1}}', 1),
    ]
    db.executemany(
        "INSERT INTO tcy_temp.dws_ddz_daily_game VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        source_rows,
    )
    target_rows = [
        (1880053, 101, "2026-06-25", 1, 2, 0, 2, 0, 0),
        (1880053, 202, "2026-06-25", 1, 1, 1, 0, 1, 0),
        (1880053, 303, "2026-06-26", 2, 1, 0, 0, 1, 0),
        (1880053, 404, "2026-06-26", 2, 1, 0, 1, 0, 0),
        (1880053, 505, "2026-06-27", 3, 1, None, 1, 0, 0),
        (1880053, 808, "2026-06-30", 1, 1, 1, 0, 0, 0),
        (1880053, 909, "2026-07-01", 1, 1, 1, 0, 0, 0),
        (1880053, 909, "2026-07-01", 1, 1, 1, 0, 0, 0),
    ]
    db.executemany(
        "INSERT INTO tcy_temp.dws_app_allgame_stat VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
        target_rows,
    )


class HandoffQualitySqlTest(unittest.TestCase):
    def test_quality_queries_detect_bad_data_and_accept_correct_buckets(self) -> None:
        db = setup_database()
        insert_fixtures(db)
        partition_rows = db.execute(
            load_sql("20260908_handoff_partition_quality.sql")
        ).fetchall()
        self.assertEqual(7, len(partition_rows))
        by_date = {row["dt"]: row for row in partition_rows}
        self.assertEqual(0, by_date["2026-07-01"]["dws_rows"])
        self.assertEqual(2, by_date["2026-06-25"]["shuffle_json_missing_rows"])
        self.assertEqual(1, by_date["2026-06-25"]["shuffle_json_missing_but_not_minus1_rows"])
        self.assertEqual(1, by_date["2026-06-29"]["non_1880053_app_human_source_rows"])

        reconcile_sql = load_sql("20260908_handoff_bomb_bucket_reconcile.sql")
        reconcile_rows = db.execute(reconcile_sql).fetchall()
        self.assertEqual(7, len(reconcile_rows))
        by_date = {row["dt"]: row for row in reconcile_rows}
        self.assertEqual(2, by_date["2026-06-25"]["mismatch_uid_mode_keys"])
        self.assertEqual(2, by_date["2026-06-26"]["mismatch_uid_mode_keys"])
        self.assertEqual(1, by_date["2026-06-27"]["invalid_bomb_rows"])
        self.assertEqual(1, by_date["2026-06-27"]["target_null_field_keys"])
        self.assertEqual(2, by_date["2026-06-28"]["invalid_bomb_rows"])
        self.assertEqual(4, by_date["2026-06-28"]["missing_target_keys"])
        self.assertEqual(1, by_date["2026-06-30"]["extra_target_keys"])
        self.assertEqual(1, by_date["2026-07-01"]["duplicate_target_keys"])

        db.execute("DELETE FROM tcy_temp.dws_app_allgame_stat")
        correct_targets = [
            (1880053, 101, "2026-06-25", 1, 2, 1, 1, 0, 0),
            (1880053, 202, "2026-06-25", 1, 1, 0, 0, 1, 0),
            (1880053, 303, "2026-06-26", 2, 1, 0, 1, 0, 0),
            (1880053, 404, "2026-06-26", 2, 1, 0, 0, 1, 0),
            (1880053, 505, "2026-06-27", 3, 1, 0, 1, 0, 0),
            (1880053, 606, "2026-06-28", 3, 1, 0, 0, 0, 1),
            (1880053, 607, "2026-06-28", 3, 1, 0, 0, 0, 1),
            (1880053, 608, "2026-06-28", 3, 1, 0, 0, 0, 0),
            (1880053, 609, "2026-06-28", 3, 1, 1, 0, 0, 0),
        ]
        db.executemany(
            "INSERT INTO tcy_temp.dws_app_allgame_stat VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
            correct_targets,
        )
        corrected = db.execute(reconcile_sql).fetchall()
        self.assertTrue(all(row["mismatch_uid_mode_keys"] == 0 for row in corrected))
        self.assertTrue(all(row["missing_target_keys"] == 0 for row in corrected))
        self.assertTrue(all(row["extra_target_keys"] == 0 for row in corrected))
        self.assertTrue(all(row["duplicate_target_keys"] == 0 for row in corrected))
        self.assertTrue(all(row["target_null_field_keys"] == 0 for row in corrected))
        corrected_by_date = {row["dt"]: row for row in corrected}
        self.assertEqual(1, corrected_by_date["2026-06-27"]["invalid_bomb_rows"])
        self.assertEqual(2, corrected_by_date["2026-06-28"]["invalid_bomb_rows"])


if __name__ == "__main__":
    unittest.main()
