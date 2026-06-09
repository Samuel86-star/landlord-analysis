# DWS 中间表：APP 端银子玩法每日统计表（金流 + 参与度）

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `dws_app_silvergame_stat` |
| 全名 | `tcy_temp.dws_app_silvergame_stat` |
| 类型 | DWS 层聚合表（每日增量） |
| 描述 | APP 端用户每日游戏统计表，仅汇总银子玩法的金流与参与度，不含倍数 / 炸弹等玩法体验指标。510K（多轮玩法）金流并入此表，玩法体验信号见 `dws_app_allgame_stat` |
| 粒度 | uid × dt（一个用户一天一行，跨银子玩法） |
