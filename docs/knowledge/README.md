# 领域知识库（分析加速）

> 目标：把"每次分析都要重新摸索的数据模型 / 口径 / 陷阱"固化下来。启动任务时按下表**精准加载 1–2 个最相关文件**，跳过重复探查——**分析更快、结论更准**。
>
> 本库只写**已验证、可复用**的框架与口径；transient 数据（某渠道上线日期）标注"截至 YYYY-MM"。

## 为什么会慢（本库要消除的 4 个耗时点）

1. **标识符维度混淆**：`zgde/zgdx` 是 app_code 还是 channel_id？平台怎么看？→ [identifier-map.md](identifier-map.md)
2. **不知道房间/玩法属于哪张表**：room_id 1124 在哪？→ [game-combat-analysis.md](game-combat-analysis.md) 的「房间定位法」
3. **DAU/活跃口径每次重推**：用哪个表、哪些字段、怎么分桶？→ [dau-active-cohort.md](dau-active-cohort.md)
4. **结果解读反复求证**：胜率 93% 正常吗？`robot=0` 说明没机器人吗？→ [data-gotchas.md](data-gotchas.md)

## 精准加载路由

按任务关键词加载**最相关**的 1–2 个文件即可，无需全读：

| 任务关键词 | 加载 | 一句话 |
| ---------- | ---- | ------ |
| app_code / group_id / channel_id / 平台 / 渠道 | [identifier-map.md](identifier-map.md) | 标识符维度区分（最易混） |
| DAU / 日活 / 活跃 / 渗透 / 人群 / 注册 | [dau-active-cohort.md](dau-active-cohort.md) | DAU 与人群口径 + 模板 SQL |
| 对局 / 房间 / 战绩 / 胜率 / game_id / room_id | [game-combat-analysis.md](game-combat-analysis.md) | 对局表矩阵 + 分析 recipe |
| 解读结果 / 排查异常 / 反直觉 | [data-gotchas.md](data-gotchas.md) | robot、胜率、符号等陷阱 |

## 维护约定

- 发现新陷阱 / 新渠道 / 新表 → 更新对应文件，并在本 README 路由表补关键词。
- 字段是否下线以 [SQL_STYLE.md](../../SQL_STYLE.md) 第八节为准，本库只放业务口径。
- 沉淀的分析方案进 `analysis/plan/`，结论进 `analysis/result/`；本库只放"可复用知识"，不放一次性结论。
