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

任何计分公式变更必须在同一提交中更新 Java 和 C++ 两个测试套件及本表。

随机发牌的跨语言一致性不在本表覆盖范围内，直到两端共享 RNG 协议。
