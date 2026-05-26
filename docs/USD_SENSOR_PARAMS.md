# USD 传感器参数说明

本文档记录 Hydra/USD 侧传感器参数的当前约定。LiDAR 和高度扫描仪使用
`hdRobotLidarUsd` 插件提供的独立 USD typed prim 表达：`LidarSensor` 和
`HeightScanSensor`。两个 schema 都是 `UsdGeomXformable`，通过 prim transform
提供传感器位姿，再通过 typed schema 属性声明采样参数。

当前 `hdRobot` + `raster` 路径支持多个 `LidarSensor` 和 `HeightScanSensor`
prim。每帧会按 sensor name 排序后传给 `RasterRenderer`，排序后的 index
用于点云/高度扫描输出和可视化选择。LiDAR 和 height scan 的可视化 index
分别索引各自按 sensor name 排序后的 sensor 列表，互不共享。两个 overlay
可以在同一帧同时开启。`Camera` prim 不再读取传感器参数。

## 全局渲染设置

| 设置名                                    | 类型    | 默认值  | 说明                                                        |
| ----------------------------------------- | ------- | ------- | ----------------------------------------------------------- |
| `hdRobot:lidar:visualizeEnabled`          | `bool`  | `false` | 是否把选中的 LiDAR 点云叠加画到 preview color。             |
| `hdRobot:lidar:visualizeSensorIndex`      | `int`   | `0`     | 可视化排序后第几个 LiDAR sensor；越界时跳过 overlay。       |
| `hdRobot:lidar:visualizePointSize`        | `float` | `2.0`   | LiDAR 点云 overlay 的点大小，单位为像素。                   |
| `hdRobot:heightScan:visualizeEnabled`     | `bool`  | `false` | 是否把选中的 height scan 命中点叠加画到 preview color。     |
| `hdRobot:heightScan:visualizeSensorIndex` | `int`   | `0`     | 可视化排序后第几个 height scan sensor；越界时跳过 overlay。 |
| `hdRobot:heightScan:visualizePointSize`   | `float` | `2.0`   | Height scan overlay 的点大小，单位为像素。                  |

## LidarSensor 属性

LiDAR 使用 `LidarSensor` prim 的 transform 作为发射原点和朝向。角度约定为
sensor local space：`+X` 向右，`+Y` 向上，`-Z` 为前方；方位角 `0` 指向前方，
正俯仰角向上，负俯仰角向下。输出是 raster 侧 LiDAR point cloud contract，
不是 Hydra AOV；开启可视化时命中点会叠加到主相机 preview color。overlay 点
大小由全局 `hdRobot:lidar:visualizePointSize` 控制，不提供每个 sensor 独立
的点半径参数。

| USD 属性名          | 类型    | 默认值  | 说明                                           |
| ------------------- | ------- | ------- | ---------------------------------------------- |
| `enabled`           | `bool`  | `true`  | 是否启用该 `LidarSensor`。                     |
| `azimuthStartDeg`   | `float` | `-90.0` | 方位角起点，单位为度。                         |
| `azimuthEndDeg`     | `float` | `90.0`  | 方位角终点，单位为度。                         |
| `azimuthStepDeg`    | `float` | `0.5`   | 方位角采样间隔，实际最小值为 `1.0e-4`。        |
| `verticalStartDeg`  | `float` | `-2.0`  | 俯仰角起点，单位为度。                         |
| `verticalEndDeg`    | `float` | `-20.0` | 俯仰角终点，单位为度；允许终点小于起点。       |
| `verticalStepDeg`   | `float` | `1.0`   | 俯仰角采样间隔，实际最小值为 `1.0e-4`。        |
| `maxRange`          | `float` | `200.0` | 单条 LiDAR 射线最大追踪距离。                  |
| `intensity`         | `float` | `1.0`   | 命中点写入点云输出的强度值；负值会被夹到 `0`。 |

等价 USDA 片段：

```usda
def LidarSensor "lidar_sensor"
{
    bool enabled = true
    float azimuthStartDeg = -90
    float azimuthEndDeg = 90
    float azimuthStepDeg = 0.5
    float verticalStartDeg = -2
    float verticalEndDeg = -20
    float verticalStepDeg = 1
    float maxRange = 200
    float intensity = 1
}
```

## HeightScanSensor 属性

高度扫描仪使用 `HeightScanSensor` prim。prim transform 的平移提供扫描平面中心；
`rayDirection` 是世界空间射线方向，扫描平面的两个采样轴会由该方向
推导得到。因此 `uStart/uEnd` 和 `vStart/vEnd` 是扫描平面上的两组偏移参数，而不是
固定的世界坐标轴。默认方向 `(0, 0, -1)` 适合 `Z-up` 场景中的向下扫描。

| USD 属性名     | 类型     | 默认值       | 说明                                                           |
| -------------- | -------- | ------------ | -------------------------------------------------------------- |
| `enabled`      | `bool`   | `true`       | 是否启用该 `HeightScanSensor`。                                |
| `uStart`       | `float`  | `-10.0`      | 扫描平面第一轴起点偏移。                                       |
| `uEnd`         | `float`  | `10.0`       | 扫描平面第一轴终点偏移。                                       |
| `uStep`        | `float`  | `0.1`        | 扫描平面第一轴采样间隔，实际最小值为 `1.0e-4`。                |
| `vStart`       | `float`  | `-10.0`      | 扫描平面第二轴起点偏移。                                       |
| `vEnd`         | `float`  | `10.0`       | 扫描平面第二轴终点偏移。                                       |
| `vStep`        | `float`  | `0.1`        | 扫描平面第二轴采样间隔，实际最小值为 `1.0e-4`。                |
| `rayDirection` | `float3` | `(0, 0, -1)` | 世界空间扫描射线方向；读取后会归一化，零向量会回退到默认方向。 |
| `maxRange`     | `float`  | `200.0`      | 单条扫描射线最大追踪距离。                                     |

等价 USDA 片段：

```usda
def HeightScanSensor "height_scan_sensor"
{
    bool enabled = true
    float uStart = -10
    float uEnd = 10
    float uStep = 0.1
    float vStart = -10
    float vEnd = 10
    float vStep = 0.1
    float3 rayDirection = (0, 0, -1)
    float maxRange = 200
}
```

## Height Scan Ground Mesh 标记

为了生成高度扫描专用的 TLAS 结果，需要在参与 height scan 的地面 mesh 上添加
custom token 属性：

```usda
custom token hdRobot:traceRole = "ground"
```

只有带有 `hdRobot:traceRole = "ground"` 的 mesh 会被视为高度扫描可追踪的
地面几何。

## 采样规则

- 单轴采样数量为 `floor(abs(end - start) / max(abs(step), 1.0e-4) + 0.5) + 1`，
  且至少为 `1`。
- LiDAR 总射线数为方位角采样数乘以俯仰角采样数。
- Height scan 总射线数为第一轴采样数乘以第二轴采样数。
- 当 `end < start` 时，采样会按从 `start` 到 `end` 的方向递减；`step` 的符号只取绝对值。
