"""Parse 斗地主 hand-card encodings and derive physical bomb counts."""

from collections import Counter


VALID_CARDS = frozenset({"3", "4", "5", "6", "7", "8", "9", "10", "j", "q", "k", "a", "2", "sj", "bj"})


def _normalize(token: str) -> str:
    token = token.strip().lower()
    token = "10" if token == "t" else token
    if token not in VALID_CARDS:
        raise ValueError(f"invalid hand card token: {token!r}")
    return token


def tokenize_hand_cards(hand_cards: str) -> list[str]:
    """Parse current comma-separated and legacy compact hand-card strings."""
    if not isinstance(hand_cards, str) or not hand_cards.strip():
        return []
    if "," in hand_cards:
        return [_normalize(token) for token in hand_cards.split(",")]

    cards = hand_cards.strip().lower()
    tokens = []
    index = 0
    while index < len(cards):
        token = next((value for value in ("10", "sj", "bj") if cards.startswith(value, index)), cards[index])
        tokens.append(_normalize(token))
        index += len(token)
    return tokens


def count_held_bombs(hand_cards: str) -> int:
    """Count four-of-a-kind ranks plus the joker rocket in a dealt hand."""
    counts = Counter(tokenize_hand_cards(hand_cards))
    rank_bombs = sum(count >= 4 for rank, count in counts.items() if rank not in {"sj", "bj"})
    return rank_bombs + int(counts["sj"] > 0 and counts["bj"] > 0)


def parse_king_status(hand_cards: str) -> str:
    tokens = tokenize_hand_cards(hand_cards)
    has_small = "sj" in tokens
    has_big = "bj" in tokens
    if has_small and has_big:
        return "王炸"
    if has_small or has_big:
        return "单王"
    return "无王"
