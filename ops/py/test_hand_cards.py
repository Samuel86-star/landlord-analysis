import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).parent))
import hand_cards


class HandCardParserTest(unittest.TestCase):
    def test_parses_current_comma_separated_format(self):
        self.assertEqual(
            hand_cards.tokenize_hand_cards("6,q,8,7,10,sj,bj"),
            ["6", "q", "8", "7", "10", "sj", "bj"],
        )

    def test_parses_current_format_with_trailing_separator(self):
        self.assertEqual(
            hand_cards.tokenize_hand_cards("6,q,8,7,10,sj,bj,"),
            ["6", "q", "8", "7", "10", "sj", "bj"],
        )
        with self.assertRaisesRegex(ValueError, "invalid hand card"):
            hand_cards.tokenize_hand_cards("6,q,,bj,")

    def test_parses_legacy_compact_format(self):
        self.assertEqual(
            hand_cards.tokenize_hand_cards("3456789TJQKA2sjbj"),
            ["3", "4", "5", "6", "7", "8", "9", "10", "j", "q", "k", "a", "2", "sj", "bj"],
        )

    def test_counts_rank_bombs_and_rocket(self):
        self.assertEqual(hand_cards.count_held_bombs("3,3,3,3,10,sj,bj"), 2)

    def test_missing_hand_is_not_classified_as_no_bomb_or_no_king(self):
        for value in (None, "", "   "):
            self.assertIsNone(hand_cards.count_held_bombs(value))
            self.assertIsNone(hand_cards.parse_king_status(value))

    def test_rejects_unknown_card_tokens(self):
        with self.assertRaisesRegex(ValueError, "invalid hand card"):
            hand_cards.tokenize_hand_cards("3,4,x")

    def test_tolerant_metrics_mark_invalid_hand_for_skipping(self):
        self.assertEqual(
            hand_cards.try_parse_hand_metrics("3,4,x"),
            (None, None, False),
        )


if __name__ == "__main__":
    unittest.main()
