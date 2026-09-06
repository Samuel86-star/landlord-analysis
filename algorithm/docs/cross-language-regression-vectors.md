# Java/C++ 斗地主算法回归向量

| 编号 | 构造 | 期望类型 | 期望长度 | 期望 Combo 分 |
| ---- | ---- | ---- | ---- | ---- |
| combo-single-ace | `single(ACE)` | `SINGLE` | 1 | 0.0 |
| combo-pair-ace | `pair(ACE)` | `PAIR` | 2 | 14.0 |
| combo-triple-ace | `triple(ACE)` | `TRIPLE` | 3 | 23.0 |
| combo-straight-3-7 | `straight(THREE..SEVEN)` | `STRAIGHT` | 5 | 15.0 |
| combo-consecutive-pairs-3-5 | `consecutivePairs(THREE..FIVE)` | `CONSECUTIVE_PAIRS` | 6 | 12.0 |
| combo-bomb-king | `bomb(KING)` | `BOMB` | 4 | 43.0 |
| combo-rocket | `rocket()` | `ROCKET` | 2 | 60.0 |

## 整手拆牌向量

| 编号 | 手牌 | 必须满足 |
| ---- | ---- | ---- |
| hand-isolated-333 | `333` | 仅拆为 `TRIPLE(3)`，不得自带对子 |
| hand-555-kk | `555KK` | `TRIPLE_WITH_PAIR(5,K)` |
| hand-kkk-aa | `KKKAA` | `TRIPLE_WITH_PAIR(K,A)` |
| hand-aaa-33 | `AAA33` | `TRIPLE_WITH_PAIR(A,3)` |
| hand-straight-bomb-8 | `34567 8888 9TJQKA 2 sj` | 保留 `BOMB(8)`，组合总长度等于 17 |
| hand-straight-bomb-2 | `3456789TJQKA 2222 sj` | 保留 `BOMB(2)`，组合总长度等于 17 |

任何计分公式或整手拆牌规则变更必须在同一提交中更新 Java 和 C++ 两个测试套件及本表。

随机发牌的跨语言一致性不在本表覆盖范围内，直到两端共享 RNG 协议。
