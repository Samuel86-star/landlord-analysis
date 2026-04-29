# DWS 中间表：金流场景配置表

## 表基本信息

| 项目 | 说明 |
| ---- | ---- |
| 库名 | `tcy_temp` |
| 表名 | `fin_flow_scene_dict` |
| 全名 | `tcy_temp.fin_flow_scene_dict` |
| 类型 | DWS 层维表 |
| 描述 | 金流场景配置表，记录银子变动的场景和类型信息 |

## 字段说明

| 字段名 | 类型 | 说明 | 示例值 |
| ------ | ---- | ---- | ------ |
| scene_id | int | 场景 ID | 1001 |
| scene_name | varchar(64) | 场景名称 | 对局输赢 |
| scene_remark | varchar(255) | 场景信息备注 | 斗地主对局银子输赢 |
| fin_flow_type_id | tinyint | 场景类型 ID | 1 |
| fin_flow_type_name | varchar(64) | 场景类型名称 | 游戏 |
| fin_flow_type_remark | varchar(255) | 场景类型备注 | 游戏相关银子变动 |

## 构建 SQL

### 建表语句

```sql
CREATE TABLE IF NOT EXISTS tcy_temp.fin_flow_scene_dict (
    scene_id INT NOT NULL COMMENT '场景id',
    scene_name VARCHAR(64) NOT NULL COMMENT '场景名称',
    scene_remark VARCHAR(255) DEFAULT NULL COMMENT '场景信息备注',
    fin_flow_type_id TINYINT NOT NULL COMMENT '场景类型id',
    fin_flow_type_name VARCHAR(64) NOT NULL COMMENT '场景类型名称',
    fin_flow_type_remark VARCHAR(255) DEFAULT NULL COMMENT '场景类型备注'
) ENGINE = OLAP
DUPLICATE KEY (scene_id)
COMMENT '金流场景配置表'
DISTRIBUTED BY HASH(scene_id) BUCKETS 1
PROPERTIES (
    "replication_num" = "1",
    "compression" = "LZ4"
);
```

### 初始化数据

```sql
INSERT INTO tcy_temp.fin_flow_scene_dict
SELECT scene_id, scene_name, scene_remark, fin_flow_type_id, fin_flow_type_name, fin_flow_type_remark
FROM hive_catalog_cdh5.dwd.dim_fin_flow_scene_dict
WHERE scene_id IN (
    SELECT DISTINCT fin_flow_scn_id
    FROM tcy_dwd.dwd_silver_si
    WHERE dt BETWEEN 20260210 AND 20260428
      AND app_id = 1880053
);
```

## 使用说明

该表为维表，用于关联 `dwd_silver_si` 表中的 `fin_flow_scn_id` 字段，获取场景的详细信息。

### 关联查询示例

```sql
SELECT
    s.dt,
    s.uid,
    s.silver_diff,
    d.scene_name,
    d.fin_flow_type_name
FROM tcy_dwd.dwd_silver_si s
LEFT JOIN tcy_temp.fin_flow_scene_dict d
    ON s.fin_flow_scn_id = d.scene_id
WHERE s.dt = 20260429
  AND s.app_id = 1880053;
```

## 注意事项

1. 该表为配置维表，数据量较小，适合广播 join
2. 需定期更新场景配置信息
3. 关联时使用 `fin_flow_scn_id = scene_id` 进行匹配
