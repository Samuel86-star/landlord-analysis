import os
import sys
import unittest
from datetime import date
from types import SimpleNamespace
from unittest.mock import Mock, patch

sys.path.insert(0, os.path.dirname(__file__))

try:
    import requests  # noqa: F401
except ModuleNotFoundError:
    sys.modules["requests"] = Mock()

from backfill_runner import checked_row_count, run_backfill


def result_with_count(value):
    return {
        "results": [
            {"resultSet": {"columns": [{"name": "cnt"}], "rows": [[value]]}}
        ]
    }


class CheckedRowCountTest(unittest.TestCase):
    def test_positive_count_is_returned(self):
        self.assertEqual(checked_row_count(result_with_count("12")), 12)

    def test_zero_count_fails_by_default(self):
        with self.assertRaisesRegex(RuntimeError, "0 rows"):
            checked_row_count(result_with_count(0))

    def test_zero_count_can_be_explicitly_allowed(self):
        self.assertEqual(checked_row_count(result_with_count(0), allow_zero=True), 0)

    def test_missing_result_shape_fails(self):
        with self.assertRaisesRegex(RuntimeError, "verification count"):
            checked_row_count({"results": []})

    def test_nonempty_malformed_rows_fail(self):
        with self.assertRaisesRegex(RuntimeError, "verification count"):
            checked_row_count(
                {"results": [{"resultSet": {"columns": [], "rows": ["12"]}}]}
            )


class RunBackfillTest(unittest.TestCase):
    def test_zero_verification_count_stops_after_delete_insert_and_count(self):
        class FakeClient:
            def __init__(self):
                self.statements = []

            def login(self):
                return self

            def connect(self):
                return self

            def execute_write(self, sql):
                self.statements.append(sql)

            def execute(self, sql):
                self.statements.append(sql)
                return result_with_count(0)

        client = FakeClient()
        args = SimpleNamespace(start=date(2026, 1, 1), end=date(2026, 1, 1), app_id=7, dry_run=False)
        with patch("backfill_runner.parse_args", return_value=args), patch(
            "backfill_runner.StarRocksClient", return_value=client
        ):
            with self.assertRaisesRegex(SystemExit, "1") as error:
                run_backfill(
                    "test_table",
                    "DELETE {dt} {app_id}",
                    "INSERT {dt_int}",
                    "COUNT {dt_next_int}",
                )

        self.assertEqual(error.exception.code, 1)
        self.assertEqual(
            client.statements,
            ["DELETE 2026-01-01 7", "INSERT 20260101", "COUNT 20260102"],
        )


if __name__ == "__main__":
    unittest.main()
