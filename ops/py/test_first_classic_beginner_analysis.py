import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path

try:
    import pandas as pd
except ModuleNotFoundError:
    pd = None

if pd is not None:
    SCRIPT = Path(__file__).parent / "first-classic-beginner" / "run_analysis.py"
    SPEC = importlib.util.spec_from_file_location("first_classic_beginner_analysis", SCRIPT)
    analysis = importlib.util.module_from_spec(SPEC)
    SPEC.loader.exec_module(analysis)


@unittest.skipIf(pd is None, "pandas is required by run_analysis.py")
class CohortBaselineTest(unittest.TestCase):
    def test_missing_channel_does_not_remove_user_from_cohort(self):
        detail = pd.DataFrame({
            "uid": [1, 2],
            "reg_date": pd.to_datetime(["2026-09-01", "2026-09-01"]),
            "first_room_id": [4484, 4484],
            "channel_category_name": ["渠道 A", None],
            "game_seq": [1, 1],
        })
        with tempfile.TemporaryDirectory() as output:
            analysis.OUTPUT = Path(output)
            with contextlib.redirect_stdout(io.StringIO()):
                analysis.module_a(detail)
            room = pd.read_csv(Path(output) / "01a_cohort_room.csv")
            self.assertEqual(room["user_count"].sum(), 2)


if __name__ == "__main__":
    unittest.main()
