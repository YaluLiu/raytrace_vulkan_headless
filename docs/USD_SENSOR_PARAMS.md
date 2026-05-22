# USD 传感器参数说明

本文档记录 Hydra/USD 侧传感器参数的当前约定。LiDAR 和高度扫描仪都使用
USD `Camera` prim 表达，通过 camera transform 提供传感器位姿，再通过
custom 属性声明传感器类型和采样参数。

当前 `hdRobot` + `raster` 路径支持多个 LiDAR camera。每帧会按 sensor name
排序后传给 `RasterRenderer`，排序后的 index 用于点云输出和可视化选择。
Height scan camera 参数会在 Hydra 侧读取并保存到独立 sensor 数组，供后续
raster 侧扫描管线消费。

## 全局渲染设置

| 设置名                               | 类型    | 默认值  | 说明                                                  |
| ------------------------------------ | ------- | ------- | ----------------------------------------------------- |
| `hdRobot:lidar:visualizeEnabled`     | `bool`  | `false` | 是否把选中的 LiDAR 点云叠加画到 preview color。       |
| `hdRobot:lidar:visualizeSensorIndex` | `int`   | `0`     | 可视化排序后第几个 LiDAR sensor；越界时跳过 overlay。 |
| `hdRobot:lidar:visualizePointSize`   | `float` | `2.0`   | 点云 overlay 的点大小，单位为像素。                   |

## LiDAR Camera 属性

LiDAR 使用 `Camera` 的 transform 作为发射原点和朝向。角度约定为 camera
space：`+X` 向右，`+Y` 向上，`-Z` 为前方；方位角 `0` 指向前方，正俯仰角
向上，负俯仰角向下。第一版输出是 raster 侧 LiDAR point cloud contract，
不是 Hydra AOV；开启可视化时命中点会叠加到主相机 preview color。
overlay 点大小由全局 `hdRobot:lidar:visualizePointSize` 控制，不再提供
每个 LiDAR camera 独立的点半径参数。

| USD 属性名              | 类型    | 默认值  | 说明                                     |
| ----------------------- | ------- | ------- | ---------------------------------------- |
| `lidar:isLidar`         | `bool`  | `false` | 标记该 `Camera` 是 LiDAR 传感器。        |
| `lidar:azimuthMinDeg`   | `float` | `-90.0` | 方位角起点，单位为度。                   |
| `lidar:azimuthMaxDeg`   | `float` | `90.0`  | 方位角终点，单位为度。                   |
| `lidar:azimuthStepDeg`  | `float` | `0.5`   | 方位角采样间隔，实际最小值为 `1.0e-4`。  |
| `lidar:verticalMinDeg`  | `float` | `-2.0`  | 俯仰角起点，单位为度。                   |
| `lidar:verticalMaxDeg`  | `float` | `-20.0` | 俯仰角终点，单位为度；允许终点小于起点。 |
| `lidar:verticalStepDeg` | `float` | `0.2`   | 俯仰角采样间隔，实际最小值为 `1.0e-4`。  |
| `lidar:maxDistance`     | `float` | `200.0` | 单条 LiDAR 射线最大追踪距离。            |

等价 USDA 片段：

```usda
def Camera "lidar_sensor"
{
    custom bool lidar:isLidar = true
    custom float lidar:azimuthMinDeg = -90
    custom float lidar:azimuthMaxDeg = 90
    custom float lidar:azimuthStepDeg = 0.5
    custom float lidar:verticalMinDeg = -2
    custom float lidar:verticalMaxDeg = -20
    custom float lidar:verticalStepDeg = 0.2
    custom float lidar:maxDistance = 200
}
```

## Height Scan Camera 属性

高度扫描仪同样使用 `Camera` prim。camera transform 的平移提供扫描平面中心；
`heightScan:rayDirection` 是世界空间射线方向，扫描平面的两个采样轴会由该方向
推导得到。因此 `minX/maxX` 和 `minZ/maxZ` 是扫描平面上的两组偏移参数，而不是
固定的世界坐标轴。默认方向 `(0, 0, -1)` 适合 `Z-up` 场景中的向下扫描。

| USD 属性名                | 类型     | 默认值       | 说明                                                           |
| ------------------------- | -------- | ------------ | -------------------------------------------------------------- |
| `heightScan:isHeightScan` | `bool`   | `false`      | 标记该 `Camera` 是高度扫描仪。                                 |
| `heightScan:minX`         | `float`  | `-10.0`      | 扫描平面第一轴最小偏移。                                       |
| `heightScan:maxX`         | `float`  | `10.0`       | 扫描平面第一轴最大偏移。                                       |
| `heightScan:stepX`        | `float`  | `0.1`        | 扫描平面第一轴采样间隔，实际最小值为 `1.0e-4`。                |
| `heightScan:minZ`         | `float`  | `-10.0`      | 扫描平面第二轴最小偏移。                                       |
| `heightScan:maxZ`         | `float`  | `10.0`       | 扫描平面第二轴最大偏移。                                       |
| `heightScan:stepZ`        | `float`  | `0.1`        | 扫描平面第二轴采样间隔，实际最小值为 `1.0e-4`。                |
| `heightScan:rayDirection` | `float3` | `(0, 0, -1)` | 世界空间扫描射线方向；读取后会归一化，零向量会回退到默认方向。 |
| `heightScan:maxDistance`  | `float`  | `200.0`      | 单条扫描射线最大追踪距离。                                     |

等价 USDA 片段：

```usda
def Camera "height_scan_sensor"
{
    custom bool heightScan:isHeightScan = true
    custom float heightScan:minX = -10
    custom float heightScan:maxX = 10
    custom float heightScan:stepX = 0.1
    custom float heightScan:minZ = -10
    custom float heightScan:maxZ = 10
    custom float heightScan:stepZ = 0.1
    custom float3 heightScan:rayDirection = (0, 0, -1)
    custom float heightScan:maxDistance = 200
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

- 单轴采样数量为 `floor(abs(max - min) / max(abs(step), 1.0e-4) + 0.5) + 1`，
  且至少为 `1`。
- LiDAR 总射线数为方位角采样数乘以俯仰角采样数。
- Height scan 总射线数为第一轴采样数乘以第二轴采样数。
- 当 `max < min` 时，采样会按从 `min` 到 `max` 的方向递减；`step` 的符号只取绝对值。
