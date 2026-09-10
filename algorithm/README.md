# landlord-algorithm 模块

`algorithm/` 是 `landlord-analysis` 仓库直接维护的算法模块，也是 Java/C++ 新算法实现的唯一权威位置。

## 模块信息

| 项 | 值 |
| ---- | ---- |
| 权威仓库 | [Samuel86-star/landlord-analysis](https://github.com/Samuel86-star/landlord-analysis) |
| 模块路径 | `algorithm/` |
| 坐标 | `com.mamba.landlord:landlord-algorithm:0.0.1-SNAPSHOT` |
| 技术栈 | Spring Boot 4.0.3 / Java 21 + C++ |
| 历史来源 | 原 `Samuel86-star/landlord` 仓库，最终提交 `b729ef0` |

## 目录

- `src/`：Java 评分、拆牌、发牌与领域模型。
- `native/include/`、`native/test/`、`native/config/`：C++ 孪生实现及验证。
- `native/previous/`：线上代码参照副本，修改须记录同步来源和影响。
- `native/extracted/`、`native/tools/`：线上发牌还原、实验和分析工具。
- `docs/`：算法规则、PRD 与测试说明。

## 修改与验证

算法代码直接在本仓修改。涉及 Java/C++ 共用规则时同步更新两端及共享回归向量；涉及 `native/previous/` 时保留线上版本依据。

在仓库根目录运行完整离线验证：

```bash
python3 ops/py/verify_offline.py
```

仓库合并记录与回退依据见[算法仓库治理记录](../docs/tech/algorithm-snapshot-plan.md)。
