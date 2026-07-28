# GPU Physics Fabric 实施计划

**Status: Draft**

| 项目 | 值 |
|---|---|
| Last Updated | 2026-07-28 |
| Scope | 架构、接口、迁移、测试与回滚计划；本轮不实现功能 |
| Approval Rule | 所有阻塞性待确认决策关闭并经用户审核后，才可改为 `Approved` |

> 状态枚举仅允许：`Draft` / `Approved` / `In Progress` / `Complete`。
>
> 本文中的“建议”不是已批准决策。涉及物理后端、进程边界、GPU
> 设备关系的内容均保留分支，不作默认假设。

## 1. 目标与非目标

### 1.1 目标

1. 保持现有 USD 静态场景初始化语义不变：
   - 静态场景继续在 `UsdStage::GetStartTimeCode()` 采样；
   - 几何、材质、灯光、相机、传感器配置及初始位姿仍由现有
     `LoadUsdTrainingScene` 和 `TrainingSceneRuntime::uploadToEngine`
     路径初始化；
   - 没有动画源时保持静态场景；
   - Viewer 在未播放时仍先显示静态 start-time 场景。
2. 将逐帧动画读取从 Engine 和静态场景运行时中分离为可替换的
   Physics Pose Producer。
3. 建立独立的 GPU Physics Fabric：
   - 当前 USD replay Producer 和未来真实物理 Producer 使用同一
     Producer Endpoint、同一静态绑定契约和同一逐帧 GPU ABI；
   - Engine 只依赖 Fabric Consumer Endpoint，不依赖 USD、PhysX、
     CUDA 或其他具体物理后端。
4. 同一份已解析 GPU 位姿同时驱动：
   - Preview/Tile 光栅 Transform；
   - TLAS GPU instance；
   - 若范围获批，Lidar/Height Scan 的动态传感器位姿。
5. 同时支持严格 lockstep 和 latest-wins，并定义可验证的背压、
   丢帧、reset、EOS、超时及资源复用语义。
6. 为 CUDA/物理后端到 Vulkan、Vulkan 到 Vulkan 提供按能力协商的
   同步与内存共享方案。
7. 保留现有 CPU 更新路径作为迁移期兼容和回滚路径，尤其不能破坏
   hdRobot 对 `Engine::updateInstance()` 的使用。

### 1.2 非目标

以下内容不在本计划默认范围内，除非用户明确扩大范围：

- 不选择或集成某个具体物理后端，也不假定未来后端一定是 PhysX。
- 不假定 Producer 与 Engine 同进程或跨进程。
- 不假定 CUDA 与 Vulkan 位于同一物理 GPU、同一 `VkDevice`、同一
  queue family 或同一操作系统。
- 不改变 USD 几何、材质、灯光、相机和传感器静态配置的导入语义。
- v1 不默认支持动态增删实例、热变更场景拓扑或变形几何/逐帧 BLAS
  更新。
- v1 不默认支持多 Producer 合并、多 Consumer 广播或跨节点网络传输。
- 不把 hdRobot/Hydra dirty-sync 改造成 Physics Producer；两条更新路径
  应兼容共存。
- 第一阶段不同时重构完整 frames-in-flight 提交模型；是否移除当前
  `vkQueueWaitIdle` 是独立决策和后续优化阶段。

## 2. 当前调用链与行为基线

### 2.1 当前初始化调用链

#### Headless Training

```text
TrainingRunner
  ├─ LoadUsdTrainingScene(path)
  │   ├─ UsdStage::Open(path)
  │   ├─ UsdTimeCode(stage->GetStartTimeCode())
  │   └─ TrainingSceneDescription
  ├─ TrainingSceneRuntime(description)
  ├─ LoadUsdPhysicsReplaySource(path)
  │   ├─ 再次 UsdStage::Open(path)
  │   └─ 收集 mesh/sensor prim 及祖先的动画 time samples
  ├─ Engine 初始化
  ├─ runtime.uploadToEngine(engine)
  │   ├─ uploadTexture / uploadMesh
  │   ├─ updateInstance(静态初始 transform/visible/traceMask)
  │   └─ setLights
  ├─ configureOutputs(静态 camera/sensor 配置)
  └─ createRenderResources()
```

主要入口：

- `headlessTraining/train/training_runner.cpp:95-119`
- `headlessTraining/core/usd_scene_loader.cpp:25-119`
- `headlessTraining/core/training_scene.cpp:36-80`
- `headlessTraining/core/usd_physics_replay_source.cpp:169-211`

#### Viewer

```text
ViewerApp::initialize
  ├─ LoadUsdTrainingScene
  ├─ TrainingSceneRuntime
  ├─ LoadUsdPhysicsReplaySource
  ├─ 创建 Viewer Vulkan instance/device/queue
  ├─ 将 Vulkan context 注入 Engine
  └─ initializeEngine
      ├─ runtime.uploadToEngine
      ├─ configureOutputs
      └─ createRenderResources
```

主要入口为 `headlessTraining/viewer/viewer_app.cpp:184-218,265-350`。
Viewer 初始 replay 为暂停状态，因此首个显示帧来自静态 start-time
场景，而不是自动消费动画 sample 0。

### 2.2 当前逐帧调用链

#### Headless

```text
PhysicsFrameSource::nextFrame()
  └─ UsdPhysicsReplaySource::nextFrame()
      ├─ 取下一个有序 USD sample
      └─ 生成 CPU AnimationFrameUpdates
          └─ PhysicsFabric::publish()
              └─ 覆盖单个 optional<PhysicsFrameSnapshot>

ApplyLatestFabricFrame()
  ├─ PhysicsFabric::consumeLatest()
  └─ TrainingSceneRuntime::applyPoseUpdates()
      ├─ mesh name -> engineInstanceIndex
      ├─ Engine::updateInstance()
      │   └─ GpuScene::updateInstance()
      │       ├─ 更新 CPU InstanceStore
      │       └─ 标记 TLAS dirty
      └─ 更新 CPU sensor pose
          └─ sensorOutputsDirty -> configureOutputs()

Engine::render()
  ├─ prepareFrame()
  │   └─ GpuScene::flushRayTracingUpdates()
  │       └─ RtScene 从 CPU instance 构造 TLAS instances
  ├─ record lidar / height / tile / preview passes
  └─ submit + vkQueueWaitIdle
```

对应代码：

- `headlessTraining/train/training_runner.cpp:72-89,131-184`
- `headlessTraining/core/physics_fabric.cpp:8-32`
- `headlessTraining/core/training_frame_consumer.cpp:6-20`
- `headlessTraining/core/training_scene.cpp:82-145`
- `engine/src/scene/gpu_scene.cpp:85-119`
- `engine/src/core/frame_executor.cpp:67-85`
- `engine/src/runtime/headless_vk.cpp:69-80`

当前 Runner 每个 render loop 最多推进一个 source frame，随后立即
publish/consume。调用行为接近“一步一渲染”，但当前 `PhysicsFabric`
本身只是无锁的 CPU 单槽 latest-wins 容器，不是并发协议，也没有 GPU
资源或同步原语。

#### Viewer

```text
UI frame
  ├─ advancePhysicsReplay()
  │   ├─ 固定墙钟 60 Hz，最多推进一个 source frame
  │   ├─ publish
  │   └─ ApplyLatestFabricFrame
  ├─ 必要时重新 configure sensor outputs
  ├─ Engine::render()
  └─ refresh/export
```

对应 `headlessTraining/viewer/viewer_app.cpp:713-732,1178-1244`。
当前 Viewer 不按 USD `timeCodesPerSecond` 或相邻 sample 间隔追赶。

#### hdRobot 兼容路径

```text
Hydra DirtyBits
  └─ SceneStore pending updates
      └─ RenderDelegate::CommitResources
          └─ EngineSceneSync::SyncSceneToEngine
              ├─ Engine::updateInstance()
              ├─ geometry/BLAS rebuild（必要时）
              └─ sensor output reconfigure（必要时）
```

hdRobot 不经过 `PhysicsFrameSource`、`PhysicsFabric` 或
`TrainingSceneRuntime`。新架构不得强制它改走物理 Fabric。

### 2.3 当前真实数据位置

| 数据 | 当前所有者/位置 | 当前逐帧行为 |
|---|---|---|
| USD replay stage、prim、sample cursor | `UsdPhysicsReplaySource` | CPU 采样 |
| 待应用帧 | `PhysicsFabric` 的单个 CPU `optional` | publish 覆盖前一帧 |
| 实例 transform/visible | `GpuScene::InstanceStore` 的 CPU vector | `Engine::updateInstance` 修改 |
| 光栅 model matrix | Preview/Tile draw loop | 每 draw 用 push constant 传完整 `mat4` |
| TLAS instance records | `RtScene` 临时 CPU vector + nvvk builder 内部 buffer | dirty 时重建/更新并等待 queue idle |
| Sensor pose | `TrainingSceneDescription` 及各 pass metadata | pose 变化会重新配置 output |

关键位置：

- `engine/src/scene/instance_store.cpp:9-31`
- `engine/features/preview/preview_pipeline.cpp:234-338`
- `engine/features/tile/multiview_tile_pipeline.cpp:235-323`
- `engine/src/scene/rt_scene.cpp:224-284`
- `engine/features/lidar/lidar_point_cloud_pass.cpp:51-105,252-310`
- `engine/features/height_scan/height_scan_pass.cpp:68-122,273-332`

### 2.4 必须冻结为回归基线的现有语义

1. 静态 USD 在 `startTimeCode` 采样，而非 Default time。
2. Replay 独立打开同一个 USD stage。
3. Replay 收集被跟踪 prim 及祖先 xform 的 time samples，并为 mesh
   收集 visibility samples；逐帧序列是所有 sample 的有序并集。
4. Replay 不自动插入 `startTimeCode`，也不把 ordinal `frameIndex`
   当成 USD time code。
5. 每个 sample 输出所有被跟踪 mesh/sensor 的状态，由 USD 在该
   sample 的插值/hold 规则决定值。
6. 当前动态范围是 mesh transform/visibility 以及 lidar/height
   sensor pose；camera、light、材质、几何和 sensor 参数不随 replay
   更新。
7. Mesh 最终可见性为：

   ```text
   effectiveVisible = staticMeshVisible && replayVisible
   ```

   光栅路径跳过不可见实例，TLAS 将其 mask 置零；目标路径必须保持
   两者一致。
8. `NoSource` 保持静态场景；EOS 后保留最后已应用状态。
9. `reset()` 将 source cursor 重置为 sample 0。Viewer 的 Reset
   当前还会立即应用 sample 0，而初始 paused 状态不会自动应用它；
   两者不可无意合并。
10. Sensor forward/up 的 replay 规则不是裸矩阵复制：
    `forward=-Z`、`up=+Y`，需做方向变换、归一化和退化回退；Height
    Scan 的 world gravity 语义也必须保留。

现有基线测试包括：

- `headlessTraining/tests/usd_scene_loader_test.cpp`
- `headlessTraining/tests/usd_animation_output_test.cpp`
- `headlessTraining/tests/physics_fabric_test.cpp`
- `headlessTraining/tests/training_frame_consumer_test.cpp`

## 3. 已批准的架构决策

本节只记录用户目标中已经明确批准的内容，不把本文建议冒充为批准。

| ID | 已批准决策 | 依据 |
|---|---|---|
| AD-001 | 现有 USD 静态场景初始化语义保持不变 | 用户目标 |
| AD-002 | 逐帧动画读取从静态初始化及 Engine 中分离为可替换 Producer | 用户目标 |
| AD-003 | 建立独立 GPU Fabric；USD replay 与未来真实物理 Producer 使用同一 Engine-facing GPU pose 契约 | 用户目标 |
| AD-004 | 本轮只产出可审核实施计划，不实现功能 | 用户指令 |

以下章节的 v1 结构、默认策略、内存所有权、进程/设备分支均仍为
Draft 建议，必须按第 4 节确认。

## 4. 待用户确认的决策

任何涉及具体后端、进程边界或 GPU 关系的实现不得在这些决策关闭前
启动。`Blocking Phase` 表示最晚必须确认的阶段。

| ID | 必须确认的问题 | 可选方向与本文建议（未批准） | Blocking Phase |
|---|---|---|---|
| D-001 | 首个真实物理后端、API/版本，以及它能提供 CPU 还是 GPU transform、在哪个 stream/queue 就绪 | 不预设 PhysX；先获得实际后端数据出口后再写 adapter | P7 |
| D-002 | Producer 与 Engine 是同进程、不同进程，还是两者都要支持 | Contract 保持可序列化；IPC 只在确认后实现 | P7 |
| D-003 | Producer GPU 与 Vulkan GPU 的关系 | 必须明确 CUDA/Vulkan UUID、是否同一 `VkPhysicalDevice`/`VkDevice`、queue family，以及是否跨物理 GPU | P7 |
| D-004 | 目标 OS、外部 handle 类型、timeline semaphore 可用性，以及 direct interop 是否必须支持 binary fallback | Linux 建议 `OPAQUE_FD`，Windows 建议 `OPAQUE_WIN32`；direct interop v1 建议要求 timeline，不支持时回退 host staging；只有用户明确要求时才承担 binary direct 协议复杂度 | P7 |
| D-005 | 共享内存和 semaphore 由谁分配/导出 | 建议 Vulkan/Engine device 侧创建可导出资源、Fabric 持有逻辑生命周期、Producer 导入；不作默认决定 | P2/P7 |
| D-006 | pose slot 身份和粒度 | 当前 USD 兼容建议“一条完整 prim path 对应一个 slot”；未来 rigid body 可一对多 fan-out；稳定 ID 的权威方需确认 | P1 |
| D-007 | 坐标系、单位、手性、矩阵乘法约定及是否必须保留 scale/shear | 建议 ABI 为 Engine canonical world 的 row-major affine 3x4，数学上使用列向量，并保留非均匀 scale/shear；在确认前 adapter 只能复现当前原始矩阵解释 | P1 |
| D-008 | v1 是 full snapshot 还是允许 sparse/delta | latest-wins 建议 v1 只允许完整 snapshot；sparse/delta 延后 | P1 |
| D-009 | 动态 pose 覆盖范围 | 建议 v1 覆盖 mesh、lidar、height sensor；camera 保持现状静态。是否扩展 camera/articulation 必须确认 | P1/P5 |
| D-010 | Producer 对 transform、visibility、trace mask 各通道的权威范围 | 当前 USD 建议只对 transform/visibility authoritative，trace mask 使用静态值；未来后端逐通道声明 | P1 |
| D-011 | “lockstep”的精确定义及各应用默认策略 | 建议把“严格一步一渲染”与“有序无丢帧流水”分开；Headless 建议严格 lockstep，Viewer 建议 latest-ready，但需用户批准 | P6 |
| D-012 | ring slot 数、最大 pose 数、允许延迟、stale/timeout、Producer 满环行为 | 由目标负载和 frames-in-flight 决定；不得硬编码 | P2/P6 |
| D-013 | v1 是否允许运行中拓扑增删 | 建议 v1 静态拓扑；变化时 drain、增加 topology epoch、重建 binding/TLAS | P1 |
| D-014 | Fabric-bound instance 与现有 `Engine::updateInstance()` 的权威冲突，以及 `Engine::getInstance()` 是否需要动态 CPU mirror | 建议绑定实例拒绝静默双写；CPU 查询默认只返回控制面/静态状态，若需当前动态姿态则新增显式异步 readback | P3 |
| D-015 | ABI/table/session 不匹配、NaN/Inf、超时和 device-lost 的失败策略 | 建议整帧拒绝、保留 last-known-good（首帧前为静态）、报告 session error；lockstep 不静默跳帧 | P1/P2 |
| D-016 | v1 Producer/Consumer 基数 | 建议 v1 单 Producer、单 Consumer、单有序消费队列；热切换通过新 session 完成 | P1 |
| D-017 | 是否在本项目内同时移除全帧 `vkQueueWaitIdle` 并引入 frames-in-flight | 建议先保持现有提交模型验证正确性，后续独立优化；否则改造面和同步风险显著扩大 | P0/P8 |
| D-018 | 变形几何、动态 BLAS、articulation link topology 是否在范围内 | 建议不属于 pose Fabric v1；hdRobot 继续走现有 rebuild 路径 | P1 |
| D-019 | source time 的编码、时钟域和 replay pacing/reset/EOS 细节 | 建议 `sourceFrameId` 与可选 `sourceTimeValue` 分开；USD time code 不冒充纳秒；保留当前 reset/EOS 基线 | P1/P6 |
| D-020 | 数值性能回归阈值和基准硬件/场景 | P0 先记录 P50/P95/P99；阈值由用户确认后写入验收门槛 | P0 |

## 5. 目标架构总览

### 5.1 控制面与数据面

```text
                         静态控制面（初始化/拓扑变更）

 USD static loader
      │ startTime semantics 不变
      ▼
 TrainingSceneDescription ──► TrainingSceneRuntime ──► Engine static upload
              │                         │                       │
              └──────────────► LogicalSceneBindingTable         │
                                        │                       │
                                        └──────────────► EngineResolvedBindingTable
                                                                 │
                                                        GpuScene static resources


                         动态数据面（逐帧）

 ┌──────────────────────────┐
 │ UsdPhysicsReplayProducer │  CPU USD sample + Vulkan staging
 └─────────────┬────────────┘
               │
               │  同一个 FabricProducerEndpoint / GPU Pose ABI
               │
 ┌─────────────▼────────────┐
 │ Future Physics Producer │  CUDA / Vulkan / CPU staging，具体后端待确认
 └─────────────┬────────────┘
               │
               ▼
     ┌──────────────────────┐
     │ Independent GPU      │
     │ PhysicsFabric        │
     │ slots + sync + state │
     └──────────┬───────────┘
                │ ConsumerTicket
                ▼
     ┌──────────────────────┐
     │ Engine / GpuScene    │
     │ validate + resolve   │
     └───────┬─────┬────────┘
             │     │
             │     ├──► Resolved Sensor Pose SSBO
             │
             └──► Resolved Transform SSBO
                     ├──► Preview / Tile raster
                     └──► TLAS GPU instance buffer ──► ray/lidar/height
```

核心约束：

- Producer 不调用 Engine，不拥有 Engine scene、descriptor 或 TLAS。
- Engine 不调用 USD/PhysX/CUDA API；它只消费已协商的 Fabric ticket。
- Fabric 不理解 USD prim、物理 body、mesh 或 sensor 语义；它只管理
  固定容量 frame slots、同步状态和 session。
- 外部 ABI buffer 不是 Engine 的最终 Transform SSBO。Engine 先把
  pose slot 解析到内部按 instance/sensor 排列的 buffer，避免渲染模块
  直接依赖 Producer 布局。

### 5.2 建议模块和依赖方向

建议新增独立顶层 CMake target；最终目录命名可在 P1 审核时调整：

```text
physicsFabric/
  include/physics_fabric/
    gpu_pose_abi.h              # C/GLSL/CUDA 可验证的固定 ABI
    scene_binding_table.hpp     # 无 USD/Engine/Vulkan handle
    fabric.hpp                  # endpoint/ticket/session 状态
    transport.hpp               # 传输能力描述
  src/
    fabric_state.cpp
    vulkan_local_transport.cpp
    vulkan_external_transport.cpp

依赖方向：

physics_fabric_contract
        ▲                ▲
        │                │
headlessTraining       Engine/GpuScene
USD Producer             Consumer

physics_fabric_vulkan ──► Vulkan

未来 CUDA/物理 adapter ──► physics_fabric_contract
                         └─► CUDA/具体后端（可选 target）
```

禁止的依赖：

- `physicsFabric` contract 不依赖 USD、GLM、PhysX、Engine scene 类型或
  Vulkan/CUDA runtime handle。
- Engine 不依赖 `UsdPhysicsReplaySource`。
- USD Producer 不依赖 `RtScene`、`PreviewPipeline` 或 sensor pass。

### 5.3 所有权与生命周期

| 组件 | 拥有 | 只借用/不得拥有 |
|---|---|---|
| Composition Root（Runner/Viewer 中的 session coordinator） | `Engine`、不可变 binding tables、`PhysicsFabric`、Producer；决定策略和启动/停止顺序 | 不直接解释 frame payload |
| `TrainingSceneRuntime` | 静态 `TrainingSceneDescription`、静态名称/索引信息；迁移期 CPU fallback | 目标状态下不拥有逐帧动态真值 |
| Producer | 自己的 USD stage 或物理 backend/context/stream、Producer-side import/mapping | 不拥有 Engine instance/TLAS/descriptor |
| `PhysicsFabric` | ring slot 的逻辑生命周期、ready/released sync、session state；由注入的 device context 创建实际资源 | 不拥有 Vulkan device 本身，不理解 scene |
| `FabricProducerEndpoint` | 写槽租约和 publish 权限 | 不可访问 Consumer 内部状态 |
| `FabricConsumerEndpoint` | acquire/release 状态和连续 release frontier | 不可推进物理 backend |
| `GpuScene` | resolved binding GPU buffer、Transform SSBO、Sensor Pose SSBO、persistent TLAS instance buffer/scratch | 不拥有 Producer 或外部 backend |
| Preview/Tile/RtScene/Sensor passes | 自己的 pipeline/descriptor 视图 | 不拥有 pose frame |

要求的销毁顺序：

```text
stop producer
  -> 禁止新 publication
  -> drain/cancel 已发布 ticket
  -> 等待 Engine 最后 GPU 使用完成并 signal release
  -> detach Engine consumer
  -> 销毁 Producer-side imports
  -> 销毁 Fabric slots/semaphores
  -> 销毁 Engine/Vulkan device
```

跨进程时还需定义 owner crash、handle 关闭和 session heartbeat；在
D-002 确认前不实现。

### 5.4 目标初始化调用链

```text
1. LoadUsdTrainingScene(startTimeCode)               # 完全保留
2. TrainingSceneRuntime(description)                 # 完全保留
3. 创建现有 UsdPhysicsReplaySource（可保持当前位置）
4. Engine 初始化 + runtime.uploadToEngine()          # 完全保留静态上传语义
5. 构建并 seal LogicalSceneBindingTable
6. Engine 解析为 EngineResolvedBindingTable
7. 协商 Fabric ABI/capabilities，创建 Producer/Consumer endpoints
8. Engine bind resolved table，配置静态 outputs，创建 render resources
9. Producer bind ProducerBindingView；双方进入 Ready
10. 按应用当前行为决定何时首次 advance/publish
```

在第 10 步之前，Engine 内部 Transform SSBO 和首次 TLAS 必须由步骤
1–4 的静态 start-time transform 初始化。因此 Viewer paused 首帧、
`NoSource` 和首帧前的静态行为保持不变。

### 5.5 接口草案

以下为职责草案，不是可直接提交的 API。

```cpp
// 控制面；不含 Vulkan/CUDA handle。
struct ProducerBindingView {
  ContractVersion version;
  Id128 tableContentHash;
  Id128 sessionId;
  CoordinateContract coordinateContract;
  SourceTimeDomain sourceTimeDomain;
  span<const ProducerPoseSlot> poseSlots;
};

// Producer 与具体 Fabric transport 之间的唯一逐帧写接口。
class FabricProducerEndpoint {
 public:
  WriteTicket acquireWritableSlot(PublishIntent);
  void publish(WriteTicket&&, PublicationMetadata);
  void publishEndOfStream(FinalPublication);
  void publishError(SessionError);
};

// 策略由 composition root 选择；Engine 只收到已选中的 ticket。
class FabricConsumerEndpoint {
 public:
  optional<ConsumerTicket> tryAcquireLatestReady();
  ConsumerTicket acquireExact(PublicationId, TimeoutPolicy);
  SessionStatus status() const;
};

// ticket 携带 buffer/offset、可选 queue wait 以及完成后的 release 值。
struct ConsumerTicket {
  GpuFrameView frame;
  GpuQueueWait readyWait;
  PublicationId publicationId;
  StreamEpoch epoch;
  CompletionRelease release;
};

// Engine API 只接受 ticket；无新帧时复用内部 last-known-good transform。
Engine::render(RenderRequest, optional<ConsumerTicket>);
```

Producer 控制接口与数据接口分开：

```cpp
class IPhysicsPoseProducer {
 public:
  bind(ProducerBindingView, FabricProducerEndpoint);
  start(ProducerRunMode);
  requestAdvance(StepToken);  // 仅严格 lockstep backend 使用
  reset(ResetRequest);
  stop();
  status();
};
```

- 自由运行的真实物理 backend 可以忽略 `requestAdvance`，持续向
  Producer Endpoint 发布。
- 当前 USD replay 可由 `UsdPhysicsReplayProducer` 包装现有
  `PhysicsFrameSource`；初期不重写已经验证的 USD 采样逻辑。
- Engine-facing 的共同接口是 `FabricProducerEndpoint` 所定义的数据
  协议，而不是要求未来 backend 实现 CPU pull 风格的
  `PhysicsFrameSource::nextFrame()`。

## 6. 静态 SceneBindingTable

### 6.1 分为逻辑表和 Engine 解析表

不能把 Engine index、BLAS device address 或 Vulkan handle 放入 Producer
公共契约或确定性 hash。

#### `LogicalSceneBindingTable`

```text
ContractVersion
tableContentHash : 128 bit，确定性内容 hash
topologyEpoch
CoordinateContract
SourceTimeDomain
poseSlots[]
renderTargets[]
sensorTargets[]
```

`PoseSlot` 建议字段：

| 字段 | 含义 |
|---|---|
| `poseSlotIndex` | `[0, poseSlotCount)` 的紧密 GPU 索引 |
| `stableEntityId` | Producer/控制面共享的稳定 ID；不得只信任未校验的 32-bit 字符串 hash |
| `debugPath` | 可选诊断名；当前 USD 使用完整 prim path |
| `initialWorldFromSlot` | 静态 start-time 的 affine 3x4 |
| `authorityFlags` | Transform / Visibility / TraceMask 分通道权威位 |
| `required` | invalid 时是否拒绝整帧；v1 是否允许 optional 需 D-015 确认 |

`RenderTargetBinding` 建议字段：

```text
poseSlotIndex
stableRenderTargetId
slotFromTarget             # 静态局部 offset
staticVisible
staticTraceMask
meshStableId / externalInstanceId
```

`SensorTargetBinding` 建议字段：

```text
poseSlotIndex
sensorKind                 # lidar / height / future extension
stableSensorId
slotFromSensor
static sensor parameters 的引用
```

统一组合公式：

```text
worldFromTarget = worldFromSlot * slotFromTarget
```

这允许未来一个 rigid body pose 驱动多个 visual/sensor。当前 USD replay
兼容模式建议每个被跟踪 prim 一个 pose slot，`slotFromTarget=identity`，
从而不改变现有 world transform 语义。

#### `EngineResolvedBindingTable`

由 Engine 在静态上传后解析，包含运行时易失映射：

```text
poseSlot -> engineInstanceIndex[]
poseSlot -> sensorKind/sensorIndex[]
engineInstanceIndex -> transformSsboIndex
engineInstanceIndex -> tlasSlot（若可追踪）
tlasSlot -> BLAS index/address、instanceCustomIndex、SBT offset、flags
resourceGeneration
```

`tableContentHash` 不包含 Vulkan handle、device address 或可重建的资源
generation。另使用随机 `sessionId` 区分同一场景的不同运行，防止旧进程
或旧 handle 的帧被新 session 接受。

### 6.2 构建、seal 与失效规则

1. 逻辑对象身份从静态场景构建。
2. `TrainingSceneRuntime::uploadToEngine()` 产生 Engine index 后，生成
   resolved table。
3. 在 Producer/Consumer attach 前，表必须通过：
   - 重复/未知 stable ID 检查；
   - slot 连续性和目标一对多检查；
   - 24-bit TLAS `instanceCustomIndex` 与 8-bit mask 上限检查；
   - affine 矩阵有限值检查；
   - hash 的确定性序列化检查。
4. attach 后表不可变。
5. 拓扑变化不得原地改表；必须：

   ```text
   stop publication
     -> drain tickets
     -> topologyEpoch++
     -> rebuild/reseal
     -> 创建新 sessionId 或显式 rebind generation
     -> 重建相关 GPU 资源
   ```

## 7. 逐帧 GPU ABI v1 草案

### 7.1 ABI 原则

- v1 建议只允许 full snapshot；`poseCount` 必须等于 attach 时确认的
  `poseSlotCount`。
- ABI 只使用 IEEE-754 `float32` 和固定宽度无符号整数。
- 小端；禁止 C++ `bool`、enum 实体布局、bitfield、GLM/Vulkan 类型、
  指针和 device address。
- 64-bit 值在共享结构中表示为 `lo/hi uint32`，不隐含要求 GLSL
  `shaderInt64`。
- slot base、payload offset、slot stride 按双方实际 alignment 能力协商；
  不硬编码 256 bytes。
- 外部 handle、semaphore 和 allocation size 属于 transport handshake，
  不进入帧 payload。

### 7.2 Frame Header：固定 128 bytes

`magic` 建议为 little-endian `"POSE"` (`0x45534F50`)，ABI 版本从 1.0
开始。

| Offset | 字段 | 类型/大小 | 语义 |
|---:|---|---:|---|
| 0 | `magic` | `u32` | `"POSE"` |
| 4 | `abiMajor` | `u32` | major 不匹配必须拒绝 |
| 8 | `abiMinor` | `u32` | minor 按兼容规则处理 |
| 12 | `headerBytes` | `u32` | v1 为 128 |
| 16 | `poseStrideBytes` | `u32` | v1 为 64 |
| 20 | `poseCount` | `u32` | 必须匹配已 attach 容量 |
| 24 | `frameFlags` | `u32` | full snapshot、discontinuity、has source time |
| 28 | `payloadOffsetBytes` | `u32` | 经能力协商的对齐 offset |
| 32 | `tableContentHash[4]` | `4 x u32` | 128-bit 静态表身份 |
| 48 | `sessionId[4]` | `4 x u32` | 128-bit 运行 session 身份 |
| 64 | `publicationIdLo/Hi` | `2 x u32` | Fabric publication，单调且永不因 reset 归零 |
| 72 | `streamEpochLo/Hi` | `2 x u32` | reset/discontinuity epoch |
| 80 | `sourceFrameIdLo/Hi` | `2 x u32` | Producer 原生 step/ordinal；USD 使用现有 frameIndex |
| 88 | `sourceTimeValueLo/Hi` | `2 x u32` | 解释由 attach 时的 `SourceTimeDomain` 决定 |
| 96 | `payloadBytes` | `u32` | v1 为 `poseCount * 64` |
| 100 | `reserved0` | `u32` | 必须写零 |
| 104 | `reserved[6]` | `6 x u32` | 必须写零 |

`SourceTimeDomain` 是 session 静态契约，例如：

- `None`
- `SimulationNanoseconds`
- `SimulationStep`
- `UsdTimeCodeFloat64Bits`，并携带 stage 的
  `timeCodesPerSecond`

这样 USD sample ordinal、USD time code、物理 step 和墙钟不会被混为
一个含糊的 `timestampNs`。

建议的 `frameFlags`：

```text
FULL_SNAPSHOT
HAS_SOURCE_TIME
DISCONTINUITY
```

EOS 和 fatal error 是关联 `finalPublicationId` 的 control-plane event，
不得通过发布空 pose 帧覆盖 last-known-good 状态。

### 7.3 Pose Record：固定 64 bytes

| Offset | 字段 | 类型/大小 |
|---:|---|---:|
| 0 | `row0` | `float4` |
| 16 | `row1` | `float4` |
| 32 | `row2` | `float4` |
| 48 | `stateFlags` | `u32` |
| 52 | `traceMask` | `u32`，仅低 8 bit 可进入 TLAS |
| 56 | `auxFlags` | `u32`，v1 Engine 未定义位必须忽略 |
| 60 | `reserved` | `u32`，必须写零 |

矩阵约定草案：

```text
[ m00 m01 m02 m03 ]
[ m10 m11 m12 m13 ]
[ m20 m21 m22 m23 ]
[  0   0   0   1  ]

p_world = worldFromSlot * p_slot
```

内存按行连续，数学使用列向量。Resolver 写 GLSL `mat4` 时必须逐列
装配或显式转置；该 3x4 行布局可直接映射 TLAS transform 的 48 bytes。
为保留当前 USD 语义，不能把 ABI 缩成只支持 rigid quaternion +
translation，除非 D-007 明确放弃 scale/shear。

建议的 `stateFlags`：

```text
VALID
VISIBLE
TRACE_MASK_VALID
DISCONTINUITY_OR_TELEPORT
```

通道解析由静态 `authorityFlags` 控制。当前 USD 兼容建议：

```text
worldFromSlot      = frame transform
effectiveVisible   = staticVisible && frame.VISIBLE
effectiveTraceMask = staticTraceMask
```

未来 Producer 若获准控制 trace mask，其组合是 override 还是与静态
mask 做 AND，必须由 D-010 决定。

### 7.4 容量和验证

```text
payloadOffset = alignUp(128, negotiatedPayloadAlignment)
slotBytes     = alignUp(payloadOffset + poseCount * 64,
                        negotiatedSlotAlignment)
slotOffset(t) = ((t - 1) % ringSize) * slotBytes
```

Consumer 不得使用未验证 header 数值作为 dispatch count 或地址边界：

1. attach 时固定合法 capacity、slotBytes、poseCount。
2. ticket 只提供已验证范围内的 buffer/offset。
3. GPU validation pass 检查 magic/version/hash/session/publication/epoch、
   header 常量及 required pose 的 NaN/Inf。
4. frame-level validation 通过后才执行 resolve，避免半帧更新。
5. 异常帧整帧拒绝并释放 slot；保留 last-known-good 或静态值，具体
   fail policy 由 D-015 批准。

## 8. PhysicsFabric 策略

### 8.1 共同 ring/timeline 协议

v1 建议单 Producer、单 Consumer。使用两个单调同步域：

- `producerReady`
- `consumerReleased`

协议不变量：

```text
publication t 从 1 单调递增，只对实际发布的 snapshot 加一
slot(t) = (t - 1) % N

Producer 写 t 前必须满足：
consumerReleased >= t - N

Producer 完整写入 header + payload 后：
signal producerReady = t

Consumer 只获取已经 ready 的 publication。
最后一次 GPU 读取完成后：
推进 consumerReleased 的连续完成 frontier
```

不能让后提交但先完成的 frame 直接 signal 一个更大 release 值，从而
释放仍被更早 GPU work 使用的 slot。若未来允许多帧在途，
`consumerReleased` 必须是“所有更早 ticket 均已完成”的累计 frontier。

Reset：

- timeline/publication counter 永不归零；
- drain 或取消旧 epoch ticket；
- `streamEpoch++`；
- 记录新 epoch 的首个 publication cutoff；
- USD 的 `sourceFrameId` 可重新从 0 开始。

EOS：

- sideband 发布 `EOS(finalPublicationId)`；
- Engine 保留最后 resolved state；
- 不发布零 pose snapshot；
- acquire 超过 final publication 时返回 EOS，而不是无限等待。

### 8.2 严格 lockstep

严格 lockstep 定义为“一次 Producer step 对应一次 Engine render”，而
不仅是 FIFO 无丢帧：

```text
Coordinator -> Producer: Advance(epoch, stepToken=k)
Producer     -> Fabric:  acquire writable slot t
Producer     -> Backend: advance/sample exactly step k
Producer     -> Fabric:  publish t
Coordinator -> Fabric:  acquireExact(t)
Engine       -> GPU:     wait ready(t), resolve, render
Engine GPU   -> Fabric:  release(t)
Coordinator -> Producer: ack/允许 Advance(k+1)
```

语义：

- 无丢帧、无重排、`sourceFrameId` 与 render 输出可一一对应。
- Producer 应先取得可写 slot 再推进对应物理步；如果后端不能暂停，
  则必须能无损保留该步 snapshot，不能在 Fabric 背压时悄悄越过它。
- 为严格复现当前 USD runner，初版窗口建议为 1。
- 超时、Producer crash、EOS 和 device-lost 是显式 session 状态；
  lockstep 不静默复用旧帧或跳到最新。
- 不建议向 Vulkan queue 提交不可取消的无限 semaphore wait；host
  先按可配置 timeout 等待/查询 ready，再提交有限且已知可满足的
  queue wait。

如果允许 Producer 预先发布 N 帧、Consumer 按序全部渲染，这是
`ordered/no-drop pipeline`，不是本文的严格 lockstep。是否需要该第三
种模式由 D-011 确认。

### 8.3 Latest-wins

```text
Producer 自由运行
  -> 有可写槽时发布当前完整 snapshot
  -> ring 满时不得覆盖未 release 槽

每个 render：
  r = 当前已完成 producerReady 的最大值
  if r > lastAccepted:
      acquire publication r
      跳过 [lastAccepted+1, r-1]
      GPU resolve/render r
      完成后 release frontier 到 r
  else:
      不重新持有旧 slot
      复用 Engine 内部 last-known-good transform/TLAS
```

关键点：

- 选择 `latest-ready`，不是“最新已提交然后等待”；后者可能让 Viewer
  无界卡顿。
- publication ID 不留洞。Fabric 满时可以丢弃未发布的物理中间 step，
  下一次发布当前完整状态，因此 `sourceFrameId` 可跳跃。
- full snapshot 是安全跳帧的前提。
- Producer 永远不能覆盖 Consumer/GPU 尚未 release 的 slot。
- 若应用要求 Producer 不得丢 simulation step，则应选择严格 lockstep
  或 ordered/no-drop，而不是 latest-wins。

## 9. Engine 数据路径改造

### 9.1 内部 Resolved Transform SSBO

外部 Pose ABI 按 pose slot 排列；Engine 内部 buffer 按 Engine instance
排列。建议每实例 128 bytes：

```text
model matrix       : mat4 / 64 bytes
normal rows        : 3 x float4 / 48 bytes
metadata           : uvec4 / 16 bytes
                     visible, traceMask, flags, source pose slot
```

Resolver compute：

1. 验证 frame；
2. 对每个 `RenderTargetBinding` 计算
   `worldFromTarget = worldFromSlot * slotFromTarget`；
3. 计算与当前 shader
   `transpose(inverse(mat3(model)))` 等价的 normal transform；
4. 合并静态/动态 visibility 与 trace mask；
5. 写 Resolved Transform SSBO；
6. 同时写 TLAS instance buffer 或为后续 TLAS pass 提供同一 transform。

初始化时 SSBO 由静态 start-time instance 数据填充。无新 Fabric frame
时不改 buffer。

### 9.2 Preview/Tile 光栅路径

当前 descriptor bindings 0–4 不含 transform。迁移建议：

1. 保持现有 0–4 不变，在 scene descriptor 末尾追加 Transform SSBO
   binding，避免无必要地改变既有 binding ABI。
2. Preview 与 Tile 的 `MeshDrawPushConstants` 移除完整 model matrix，
   保留/增加 `transformIndex`、`objIndex`、`instanceId`。
3. 两套 vertex shader 都从同一个 Resolved Transform SSBO 读取 model
   和 normal transform。
4. 当前 CPU draw loop 根据 `SceneInstance.visible` 跳过 draw。GPU
   visibility 上线后 CPU 不再知道逐帧状态，迁移期：
   - 对所有静态可绘制实例仍发 draw；
   - shader 依据 resolved visibility clip/discard，确保 color/depth/
     object-ID 都不写；
   - 后续可用 GPU draw compaction/indirect 降低不可见 draw 成本，但
     不作为正确性首阶段前置条件。
5. 保留 CPU `Engine::updateInstance` 路径；它也写入同一 resolved
   buffer 的 staging/update 通道，供 hdRobot 和未绑定实例使用。

涉及：

- `engine/src/scene/gpu_scene.{hpp,cpp}`
- `engine/src/scene/instance_store.cpp`
- `engine/src/scene/scene_descriptors.{hpp,cpp}`
- `engine/shaders/common/host_device.h`
- `engine/features/preview/preview_pipeline.{hpp,cpp}`
- `engine/features/preview/shaders/mesh.vert`
- `engine/features/tile/multiview_tile_pipeline.{hpp,cpp}`
- `engine/features/tile/shaders/tile_multiview.vert`

### 9.3 TLAS GPU instance buffer

当前 `RtScene` 每次从 CPU `SceneInstance` 构造
`VkAccelerationStructureInstanceKHR` vector，并交给 nvvk builder 创建
临时 GPU buffer。目标路径要求 `RtScene`/`GpuScene` 显式拥有：

- 固定容量、持久化 TLAS instance buffer；
- 持久化 update scratch buffer；
- TLAS build/update metadata；
- 静态 `tlasSlot -> BLAS address/custom index/SBT offset/flags` 数据。

目标 record 顺序：

```text
queue wait producerReady(t)（若 ticket 需要）
  -> validate/resolve pose compute
  -> write Resolved Transform SSBO
  -> write 64-byte raw TLAS instance records
  -> barrier:
       COMPUTE_SHADER / SHADER_WRITE
       ->
       VERTEX_SHADER / SHADER_READ
       ACCELERATION_STRUCTURE_BUILD_KHR /
         ACCELERATION_STRUCTURE_READ_KHR
  -> vkCmdBuildAccelerationStructuresKHR(mode=UPDATE)
  -> barrier:
       ACCELERATION_STRUCTURE_BUILD_KHR /
         ACCELERATION_STRUCTURE_WRITE_KHR
       ->
       ray-query compute stages /
         ACCELERATION_STRUCTURE_READ_KHR
  -> tile/lidar/height/preview
  -> queue signal consumerReleased(t)
```

TLAS raw record 必须显式打包并测试：

```text
transform 3x4                         48 bytes
instanceCustomIndex:24 | mask:8        4 bytes
SBT record offset:24 | flags:8         4 bytes
BLAS device address                    8 bytes
```

不得在 GLSL/CUDA 共享 ABI 中复用 C++ bitfield 布局。

为使用 UPDATE：

- 首次 build 必须带
  `VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR`；
- instance/geometry 数和 build flags 必须满足 Vulkan update 约束；
- v1 固定 TLAS slot 数，不可见实例通过 `mask=0` 保留 slot；
- 拓扑或 BLAS 引用变化走 full rebuild safe point；
- TLAS handle 在 transform-only update 中保持稳定，避免每帧重写
  descriptor。

Vulkan 规范对 UPDATE 的 flags/geometryCount 等一致性要求见
[Acceleration Structures specification](https://docs.vulkan.org/spec/latest/chapters/accelstructures.html)。

需要把当前 `prepareFrame()` 中命令录制前的
`GpuScene::flushRayTracingUpdates()` 拆为：

- CPU/static topology maintenance；
- command-buffer 内的 `recordDynamicSceneUpdate()`。

无新 frame 时直接复用 Transform SSBO 和 TLAS，不做无意义 refit。

涉及：

- `engine/src/scene/rt_scene.{hpp,cpp}`
- `engine/src/scene/gpu_scene.{hpp,cpp}`
- `engine/src/core/frame_executor.{hpp,cpp}`
- `engine/src/core/render_resource_lifecycle.{hpp,cpp}`
- `engine/src/runtime/headless_vk.{hpp,cpp}`
- 新增 scene-pose resolve compute shader

### 9.4 Sensor pose

若 D-009 批准纳入 v1：

- Lidar/Height 的 scan pattern、range、resolution、enabled 等保持静态
  output 配置。
- `SensorTargetBinding` 将 pose slot 解析为独立 Resolved Sensor Pose
  SSBO。
- Lidar/Height compute 读取动态 origin/forward/up；pose-only 变化不再
  调用 `configureOutputs()`。
- Resolver 必须复现当前 `forward=-Z`、`up=+Y`、normalize、共线回退；
  Height Scan 的 world gravity 不随 sensor pose 旋转。
- `sensorOutputsDirty` 最终只表示 topology/static-parameter 变化，不再
  表示逐帧 pose 变化。

涉及：

- `engine/features/sensor_common/sensor_generation_buffers.hpp`
- `engine/features/sensor_common/sensor_generation_pipeline.{hpp,cpp}`
- `engine/src/core/output_controller.{hpp,cpp}`
- `engine/features/lidar/lidar_point_cloud_pass.{hpp,cpp}`
- `engine/features/lidar/shaders/lidar_points.comp`
- `engine/features/height_scan/height_scan_pass.{hpp,cpp}`
- `engine/features/height_scan/shaders/height_scan.comp`

## 10. GPU 同步与互操作方案

### 10.1 能力协商先于方案选择

初始化必须查询而不是假定：

- external buffer memory 的 handle compatibility、export/import features、
  dedicated-only 要求；
- external semaphore 的 handle compatibility；
- timeline semaphore feature 及对应 CUDA/runtime 支持；
- Vulkan device UUID 与 CUDA device UUID；
- queue families、exclusive/concurrent sharing；
- external memory allocation alignment/size；
- 操作系统 handle 所有权和关闭规则。

仓库当前在 Engine/Viewer Vulkan 初始化中已经列出若干 external
memory/semaphore/fence 扩展，但“启用扩展名”不等于指定 handle 类型、
timeline import 和设备匹配一定可用：

- `engine/src/runtime/engine_session.cpp:44-66`
- `headlessTraining/viewer/viewer_app.cpp:52-75`

### 10.2 CUDA/物理后端 -> Vulkan

本分支仅在 D-001 确认后端能在 CUDA stream 上得到或生成 pose 时适用。
不假定 PhysX 的具体读取 API；若后端只给 CPU pose，则改用 Vulkan
host-staging transport。

建议的同物理 GPU 路径：

```text
初始化：
Vulkan 创建可导出的 device-local Fabric slots
  -> 导出 OPAQUE_FD / OPAQUE_WIN32
  -> CUDA import external memory
  -> map 为 CUDA device pointer

Vulkan 创建并导出：
  producerReady timeline semaphore
  consumerReleased timeline semaphore
  -> CUDA import external semaphores

逐帧 t：
CUDA stream:
  wait consumerReleased >= t-N
  -> 后端完成 simulation pose
  -> CUDA pack/convert kernel 写 GPU Pose ABI slot(t)
  -> cudaSignalExternalSemaphoresAsync(producerReady, t)

Vulkan queue:
  wait producerReady >= t
  -> 必要的 external queue-family acquire
  -> resolve/TLAS/render
  -> 必要的 external queue-family release
  -> signal consumerReleased = release frontier
```

直接 CUDA/Vulkan interop 必须匹配设备 UUID。NVIDIA 官方流程也要求：
Vulkan 创建/导出资源、CUDA 按 UUID 选择匹配设备、导入 memory/
semaphore，并通过 signal/wait 定义访问顺序：
[CUDA Vulkan interoperability](https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/graphics-interop.html)。

同步优先级草案：

1. external timeline semaphore：两个 semaphore、64-bit 累计值；
2. 若 timeline external import 不可用：默认建议显式
   host-staging/copy transport；
3. 只有 D-004 明确要求 direct binary fallback 时，才实现每 slot 成对
   binary ready/released semaphore和额外序号控制面；
4. 不允许在能力不满足时静默当成零拷贝。

外部 semaphore 销毁前必须确保所有异步 wait/signal 已完成。Linux FD
和 Windows handle 的 import 后所有权规则不同，必须由 transport 层
封装，不能让 Producer/Engine 双重关闭。

### 10.3 Vulkan -> Vulkan

| 实际关系 | 内存 | 同步 | 所有权处理 |
|---|---|---|---|
| 同一 `VkDevice`、同一 queue | 普通 Vulkan buffer | queue 顺序 + 明确 buffer memory barrier；可省 OS handle/semaphore；多线程提交同一 `VkQueue` 时仍须外部互斥或统一 submitter | 无 queue-family transfer |
| 同一 `VkDevice`、不同 queue | 普通 Vulkan buffer | timeline semaphore signal/wait | exclusive sharing 时做 queue-family release/acquire；或明确选择 concurrent sharing |
| 不同 `VkDevice`，但 external handle 兼容 | external memory import/export；除 device UUID 外还核对适用的 driver UUID/handle compatibility | external timeline；binary direct fallback 仅在 D-004 批准后实现 | 需要 `VK_QUEUE_FAMILY_EXTERNAL` 相关 transfer（按实际资源/实现要求） |
| 跨进程 | 同上，另需 handle IPC | 同上，另需 session/control IPC | Linux FD 传递或 Windows handle duplication；生命周期必须显式 |
| 不同物理 GPU | 不假定能直接 import | 由用户选择 peer copy、device group、pinned-host staging 或其他 transport | 必须做 capability/性能验证 |

Vulkan external memory/sync 的 handle 与能力模型参考
[Vulkan External Memory and Synchronization](https://docs.vulkan.org/guide/latest/extensions/external.html)；
timeline semaphore 的累计值和多队列用法参考
[Vulkan timeline semaphore sample](https://docs.vulkan.org/samples/latest/samples/extensions/timeline_semaphore/README.html)；
external queue family 语义参考
[`VK_QUEUE_FAMILY_EXTERNAL`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_QUEUE_FAMILY_EXTERNAL.html)。

### 10.4 当前提交模型的迁移

当前 `submitCurrentCommandBufferAndWait()` 每帧 `vkQueueWaitIdle`。建议分两步：

1. 正确性阶段：扩展 submit API 以接受 Fabric waits/signals，但仍保留
   现有 queue idle，降低同时变更多个同步维度的风险。
2. 性能阶段：在 D-017 批准后引入 per-frame fences/frames-in-flight，
   release timeline 由实际 GPU 完成顺序推进。

第一步能验证 ABI、barrier 和 TLAS update，但不宣称已经实现 CPU/GPU
完全重叠。

## 11. 分阶段任务清单和依赖关系

### 11.1 依赖图

```text
P0 基线冻结
  -> P1 Contract / Binding / Fabric state machine
      -> P2 Vulkan-local Fabric + USD Producer shadow path
          -> P3 Transform SSBO + Raster
              -> P4 Persistent TLAS GPU update
                  -> P5 Sensor pose
                      -> P6 Runner/Viewer 策略切换
                          -> P8 默认切换、优化与清理

P7 External CUDA/Vulkan transports
  依赖 P1、D-001..D-005；
  与 P2-P6 可并行开发，但端到端验收依赖 P4/P6，
  最终并入 P8 前必须完成所选配置验证。
```

任何阶段失败都保持后一阶段未开始；不跨越验收门禁。

### P0：冻结语义与性能基线

**依赖：** 无。

**任务：**

- [ ] 将第 2.4 节语义转成 golden/regression tests。
- [ ] 补充 Viewer 初始 paused、Reset 立即 sample 0、EOS/NoSource 行为测试。
- [ ] 记录静态 visibility 与动态 visibility 四种组合。
- [ ] 记录当前 raster、TLAS、lidar、height 输出 golden。
- [ ] 按第 15 节协议记录当前 CPU/GPU 性能和资源行为。
- [ ] D-020 写入经批准的数值回归阈值。

**预计修改文件：**

- `headlessTraining/tests/usd_scene_loader_test.cpp`
- `headlessTraining/tests/usd_animation_output_test.cpp`
- `headlessTraining/tests/physics_fabric_test.cpp`
- `headlessTraining/tests/training_frame_consumer_test.cpp`
- 新增 Engine 输出 parity/performance harness

**验收标准：**

- 所有现有测试仍通过。
- 第 2.4 节每条语义均有自动测试或明确人工基线。
- 基准记录包含硬件、driver、构建配置、场景、分辨率和原始结果。
- 尚未改变运行时调用链。

**风险与回滚：**

- 仅测试/测量，无功能切换；若 golden 暴露未记录的现有行为，先更新
  本文，不进入 P1。

### P1：独立 Contract、SceneBindingTable 与 Fabric 状态机

**依赖：** P0；D-006–D-010、D-013、D-015、D-016、D-019 已确认。

**任务：**

- [ ] 新建独立 `physicsFabric` CMake target。
- [ ] 定义 Logical/Resolved binding table、确定性序列化/hash、
  `sessionId` 和 topology epoch。
- [ ] 固化 Header/Pose ABI 的 size/alignment/offset assertions。
- [ ] 实现不依赖 GPU 的 SPSC ring 状态机和 fake timeline。
- [ ] 定义 strict lockstep/latest-ready/EOS/reset/error 状态。
- [ ] 保留现有 `headlessTraining/core/physics_fabric.*`，先作为兼容 adapter。

**预计修改文件：**

- 新增 `physicsFabric/`
- 根 `CMakeLists.txt`
- `engine/CMakeLists.txt`
- `headlessTraining/CMakeLists.txt`
- `headlessTraining/core/training_scene.{h,cpp}`
- 新增 `headlessTraining/core/scene_binding_table_builder.*`
- `headlessTraining/core/physics_fabric.{h,cpp}`
- 新增 contract/fabric unit tests

**验收标准：**

- C++ ABI `sizeof/alignof/offsetof` 与 golden 一致。
- binding hash 在遍历顺序扰动后仍确定；重复/未知 ID 明确失败。
- 一 pose 对多 target、旧 table/session、epoch 切换测试通过。
- ring wrap、满环背压、latest 跳帧、多个在途 ticket 的 release frontier、
  reset cutoff、EOS、timeout、Producer 退出测试通过。
- Engine/Runner/Viewer 默认仍走 legacy CPU 路径。

**风险与回滚：**

- 新 target 完全旁路，不启用即可回滚。
- ABI 在本阶段结束后视为 v1 freeze；若发现错误，P2 前修改 major/minor，
  不带着隐式布局变化进入 GPU 实现。

### P2：Vulkan-local Fabric 与 USD Producer shadow path

**依赖：** P1；D-005、D-012 已确认。

**任务：**

- [ ] 创建 Vulkan-local slots、staging、ready/released sync。
- [ ] 用现有 `PhysicsFrameSource` 包装
  `UsdPhysicsReplayProducer`，不重写 USD 采样逻辑。
- [ ] 将 CPU `AnimationFrameUpdates` pack 为 GPU ABI full snapshot。
- [ ] 增加 GPU readback validator 和 session metrics。
- [ ] shadow 模式同时生成 legacy CPU snapshot 与 GPU ABI，但 Engine
  仍消费 legacy；逐帧比较 transform/visibility/sensor pose。

**预计修改文件：**

- `physicsFabric/src/vulkan_local_transport.*`
- 新增 `headlessTraining/core/usd_physics_replay_producer.*`
- `headlessTraining/core/physics_frame.{h,cpp}`
- `headlessTraining/core/usd_physics_replay_source.{h,cpp}`（仅必要的只读
  metadata 暴露，不改变 sample 算法）
- `headlessTraining/CMakeLists.txt`
- 新增 ABI readback/shadow parity tests

**验收标准：**

- identity、translation、rotation、非均匀 scale、shear、visibility、
  NaN/Inf ABI 测试通过。
- 每个 USD sample 的 GPU ABI 解码结果与 legacy
  `AnimationFrameUpdates` 在容差内一致。
- source frame ordinal、USD time code、reset epoch、EOS 不混淆。
- 稳态 slot 无每帧 allocation/import/export。
- 默认运行路径仍未切换。

**风险与回滚：**

- 关闭 shadow feature flag 即完全回到 P1/legacy。
- 不允许在本阶段顺便改静态 loader 或 replay sample 集合。

### P3：Resolved Transform SSBO 与 Preview/Tile

**依赖：** P2；D-007、D-014 已确认。

**任务：**

- [ ] `GpuScene` 拥有并初始化 Resolved Transform SSBO。
- [ ] 上传 EngineResolvedBindingTable。
- [ ] 实现 validate/resolve compute。
- [ ] legacy `Engine::updateInstance` 同步写同一内部 transform store。
- [ ] 修改 scene descriptors、Preview/Tile pipelines 和 vertex shaders。
- [ ] 实现 GPU visibility 的正确 raster discard/clip；保留 ID/AOV。
- [ ] 双路径渲染比较：legacy push constant 与 SSBO。

**预计修改文件：**

- `engine/include/engine/engine.hpp`
- `engine/src/scene/engine_scene.cpp`
- `engine/src/scene/gpu_scene.{hpp,cpp}`
- `engine/src/scene/instance_store.{hpp,cpp}`
- `engine/src/scene/scene_descriptors.{hpp,cpp}`
- `engine/src/core/renderer_internal.hpp`
- `engine/src/core/render_resource_lifecycle.{hpp,cpp}`
- `engine/shaders/common/host_device.h`
- `engine/features/preview/preview_pipeline.{hpp,cpp}`
- `engine/features/preview/shaders/mesh.vert`
- `engine/features/tile/multiview_tile_pipeline.{hpp,cpp}`
- `engine/features/tile/shaders/tile_multiview.vert`
- 新增 resolve compute shader/tests

**验收标准：**

- 两个 raster pipeline 的位置、法线、可见性、object/instance ID golden
  与 legacy 一致。
- 非均匀 scale/shear 的 normal matrix 结果在批准容差内一致。
- 静态 false / 动态 true 不会错误重新显示对象。
- hdRobot 和未绑定实例继续通过 `Engine::updateInstance` 工作。
- Fabric 未绑定或无新帧时使用静态/last-known-good transform。

**风险与回滚：**

- runtime/build feature flag 选择 legacy push constants 或 Transform SSBO。
- descriptor/pipeline 任一 parity 失败时不进入 P4。

### P4：持久 TLAS instance buffer 与 GPU update

**依赖：** P3；D-013、D-017 的阶段策略已确认。

**任务：**

- [ ] `RtScene` 拥有 persistent TLAS instance/update scratch buffers。
- [ ] GPU 生成标准 64-byte TLAS records。
- [ ] command buffer 内录制 barrier 和 TLAS UPDATE。
- [ ] topology/BLAS 变化保留 full rebuild；transform/visibility 走 update。
- [ ] 将动态更新从命令录制前的同步 flush 迁入 frame command。
- [ ] 保持旧 host-vector/nvvk TLAS 路径用于 shadow compare/回滚。

**预计修改文件：**

- `engine/src/scene/rt_scene.{hpp,cpp}`
- `engine/src/scene/gpu_scene.{hpp,cpp}`
- `engine/src/core/frame_executor.{hpp,cpp}`
- `engine/src/core/render_resource_lifecycle.{hpp,cpp}`
- `engine/src/runtime/engine_session.cpp`
- `engine/src/runtime/headless_vk.{hpp,cpp}`
- 新增 TLAS record/validation tests

**验收标准：**

- GPU 生成 record 与 CPU `VkAccelerationStructureInstanceKHR` 参考逐字段
  一致，特别是两个 24/8-bit packed word。
- Validation Layer 无同步、地址、AS build/update 报错。
- 移动、隐藏、恢复实例的 ray/lidar/height 命中与 legacy 一致。
- transform-only 帧不分配临时 TLAS instance/scratch buffer。
- 无新 pose 帧不 refit；拓扑变化可靠走 full rebuild。

**风险与回滚：**

- feature flag 切回 legacy host TLAS builder。
- shadow 模式可同时保留两份 instance records 并比较，但不在同一输出
  中混用两个 TLAS。

### P5：Sensor Pose GPU 化

**依赖：** P4；D-009 已批准包含哪些 sensor。

**任务：**

- [ ] 添加 Resolved Sensor Pose SSBO 和静态 sensor binding。
- [ ] 修改 Lidar/Height generation shaders 使用动态 pose。
- [ ] pose-only frame 不再触发 `configureOutputs`。
- [ ] 保留 static parameter/topology reconfigure 路径。
- [ ] 对 current USD forward/up 退化和 Height gravity 做 parity。

**预计修改文件：**

- `engine/features/sensor_common/sensor_generation_buffers.hpp`
- `engine/features/sensor_common/sensor_generation_pipeline.{hpp,cpp}`
- `engine/src/core/output_controller.{hpp,cpp}`
- `engine/features/lidar/lidar_point_cloud_pass.{hpp,cpp}`
- `engine/features/lidar/shaders/lidar_points.comp`
- `engine/features/height_scan/height_scan_pass.{hpp,cpp}`
- `engine/features/height_scan/shaders/height_scan.comp`
- `headlessTraining/core/training_scene.{h,cpp}`
- sensor parity tests

**验收标准：**

- 所有现有 lidar/height replay golden 与 legacy 一致。
- sensor pose 动画不重建 output configuration/buffer。
- static parameter 或 enabled 变化仍正确触发配置更新。
- camera 行为保持 D-009 已批准范围。

**风险与回滚：**

- feature flag 切回 CPU sensor metadata/reconfigure。
- mesh GPU pose 可独立保留，不要求因 sensor 回滚而全部回滚。

### P6：Runner/Viewer 端到端切换

**依赖：** P5；D-011、D-012、D-015、D-019 已确认。

**任务：**

- [ ] Composition root 按目标生命周期持有 table/fabric/producer。
- [ ] Headless/Viewer 从 `ApplyLatestFabricFrame` 切到 ConsumerTicket。
- [ ] 实现严格 lockstep/latest-ready 的应用编排和 metrics。
- [ ] 保留 `PhysicsFrameSource`、`ApplyLatestFabricFrame` 和 legacy
  `PhysicsFabric` 作为可选回滚路径。
- [ ] 定义 UI pause/play/step/reset/EOS 的新状态机并与 baseline 对比。
- [ ] 输出 frame metadata 记录
  publication/source frame/epoch，便于训练结果追踪。

**预计修改文件：**

- `headlessTraining/train/training_runner.cpp`
- `headlessTraining/viewer/viewer_app.{h,cpp}`
- `headlessTraining/core/physics_fabric.{h,cpp}`
- `headlessTraining/core/training_frame_consumer.{h,cpp}`
- `headlessTraining/core/physics_frame.{h,cpp}`
- `headlessTraining/core/training_scene.{h,cpp}`
- 相关 CLI/options/tests

**验收标准：**

- Headless strict lockstep：每个输出 render 与 source frame 一一对应，
  无丢帧/重排。
- Viewer latest-ready：不等待未 ready 的“最新提交”帧，允许跳
  publication，且不覆盖 GPU in-flight slot。
- pause/step/reset/EOS/NoSource 与批准后的语义一致。
- GPU 路径无逐帧 mesh name lookup、`TrainingSceneRuntime::applyPoseUpdates`
  或 pose-only `configureOutputs`。
- legacy feature flag 可在同一构建中恢复旧路径。

**风险与回滚：**

- 默认仍保持 legacy，先按测试/小规模运行显式 opt-in。
- 出现 session/输出不一致时切回 legacy，不删除旧 API。

### P7：所选 External Interop Transport

**依赖：** P1；D-001–D-005 全部确认；端到端验收还依赖 P4/P6。

**任务：**

- [ ] 实现 capability report 和明确的 unsupported 原因。
- [ ] 按批准配置实现 CUDA->Vulkan 或 external Vulkan->Vulkan transport。
- [ ] 实现 UUID/handle type/alignment/dedicated allocation 校验。
- [ ] 实现 timeline；必要时实现 per-slot binary fallback。
- [ ] 若跨进程，增加 handle IPC、session handshake、heartbeat/crash
  cleanup。
- [ ] 若不同 GPU，单独实现获批 copy transport，不冒充 zero-copy。

**预计修改文件：**

- `physicsFabric/src/vulkan_external_transport.*`
- 可选 `physicsFabric/src/cuda_external_transport.*`
- `engine/src/runtime/engine_session.cpp`
- `engine/src/runtime/headless_vk.{hpp,cpp}`
- `headlessTraining/viewer/viewer_app.cpp`
- CMake 中可选 CUDA/后端 target
- hardware interop tests

**验收标准：**

- 所选 OS/driver/GPU 配置上的 positive interop test 通过。
- UUID mismatch、unsupported handle、timeline 不可用、Producer crash、
  device-lost 等 negative tests 不死锁、不错误消费。
- ready/released 顺序经长时间 ring wrap stress 验证。
- 无每帧 handle import/export 或资源重建。
- fallback transport 的行为和性能被明确报告。

**风险与回滚：**

- transport 工厂回退到 Vulkan-local/host-staging；
- optional CMake target 可完全关闭，不影响 Engine/USD legacy 构建。

### P8：默认切换、提交优化和清理

**依赖：** P6；若本期承诺真实 backend interop，则还依赖 P7；所有
Definition of Done 条件满足。

**任务：**

- [ ] 经批准将选定应用的默认路径从 legacy 改为 GPU Fabric。
- [ ] 若 D-017 批准，移除每帧 queue idle，加入 frames-in-flight、
  completion frontier 和精确 release。
- [ ] 性能调优：可选 GPU indirect draw compaction、早期 release
  submission。
- [ ] 完成 legacy deprecation 评审；在至少一个发布周期/明确批准前
  不删除回滚路径。
- [ ] 更新 public docs、FileMap、构建选项和调试指标。
- [ ] 修改索引范围内代码后运行 `bash install.sh graphify_index`。

**预计修改文件：**

- `engine/src/runtime/headless_vk.{hpp,cpp}`
- `engine/src/runtime/engine_session.cpp`
- `engine/src/core/frame_executor.{hpp,cpp}`
- Runner/Viewer 默认配置
- `engine/CMakeLists.txt`
- `headlessTraining/CMakeLists.txt`
- `docs/FILEMAP.md` 及受影响 domain map
- 本计划 Status/Progress Log

**验收标准：**

- 第 12 节总体 Definition of Done 全部满足。
- 批准场景的性能阈值满足，且无 steady-state per-frame allocation。
- Validation Layer、sanitizer、长时 stress 和硬件 interop lane 通过。
- 回滚演练成功并有文档。

**风险与回滚：**

- 默认配置可切回 legacy；
- frames-in-flight 优化可独立回退到同步 submit，不回退已验证的 ABI/
  Transform/TLAS 数据路径。

## 12. 总体 Definition of Done

只有以下全部满足，Status 才能改为 `Complete`：

- [ ] 本文所有阻塞性决策均有用户确认和记录，Status 曾经过
  `Approved`、`In Progress`。
- [ ] USD 静态 start-time 初始化、首帧前状态和 NoSource 行为与
  baseline 完全一致。
- [ ] 当前 USD replay Producer 通过统一 Producer Endpoint 写 GPU ABI；
  Engine 不包含 USD 专用逐帧逻辑。
- [ ] 已批准的真实物理/transport 配置可通过同一 Endpoint 提供 pose，
  或若本期只批准接口，则有独立 conformance Producer 证明可替换性。
- [ ] Engine hot path 不再通过 mesh name lookup、
  `AnimationFrameUpdates` 和逐实例 `Engine::updateInstance` 应用 Fabric
  帧。
- [ ] Preview、Tile、TLAS 以及获批 sensor 使用同一 publication 解析的
  transform/visibility。
- [ ] strict lockstep、latest-ready、reset、EOS、timeout、backpressure、
  ring wrap 和 release frontier 的自动测试全部通过。
- [ ] ABI/table/session mismatch 和 invalid frame 不会导致 OOB、半帧状态
  或错误场景绑定。
- [ ] transform-only TLAS 更新使用持久 instance/scratch buffer，无
  steady-state per-frame allocation。
- [ ] hdRobot 及未绑定实例的 `Engine::updateInstance`、动态 geometry/
  BLAS 和 sensor 配置路径无回归。
- [ ] 所选 CUDA/Vulkan 或 Vulkan/Vulkan 配置完成 positive/negative
  interop test；未支持组合能明确失败或走已批准 fallback。
- [ ] Vulkan Validation Layer、sanitizer、图像/传感器 golden、长时 stress
  全部通过。
- [ ] P0 记录且 D-020 批准的性能阈值满足。
- [ ] 回滚开关、资源销毁顺序、故障恢复和运维指标有文档且演练通过。
- [ ] CMake、FileMap、架构文档和 Graphify 索引已更新。

## 13. 风险、回滚点和兼容策略

### 13.1 风险表

| 风险 | 影响 | 预防/检测 | 回滚点 |
|---|---|---|---|
| row/column-major、乘法方向或坐标单位错误 | raster/RT/sensor 全部错位 | ABI golden；identity/rotation/scale/shear；CPU/GPU matrix compare | P2 ABI freeze 前；P3 legacy raster |
| 静态与动态 visibility 合并错误 | 静态隐藏对象被重新显示；raster/TLAS 不一致 | 四组合测试；同 publication raster/RT compare | P3 shader visibility；P4 host TLAS |
| normal matrix 与当前 inverse-transpose 不等价 | 非均匀缩放光照错误 | 图像 golden 和数值容差测试 | P3 push constant path |
| GPU TLAS record 的 24/8-bit 打包或地址错误 | 错误命中、validation/device loss | 64-byte 逐字段 compare；Validation Layer | P4 host-vector/nvvk path |
| TLAS UPDATE 违反 topology/build 约束 | validation error 或未定义行为 | 固定 slot；generation；变化强制 full rebuild | P4 full rebuild/legacy builder |
| sensor forward/up/gravity 语义漂移 | Lidar/Height 输出变化 | 退化方向、祖先 transform、gravity golden | P5 CPU metadata path |
| ring ABA、过早 release、乱序完成 | Producer 覆盖 GPU 正在读的 slot | monotonic publication、session/epoch、连续 release frontier stress | P2 local fabric off |
| Reset/EOS 与 timeline 混用 | 接受旧 epoch、死等不存在帧 | reset cutoff、EOS finalPublication、timeout tests | P6 legacy state machine |
| external handle/UUID/capability 不匹配 | import 失败、未定义共享或跨 GPU 隐式拷贝 | 启动时完整 capability report；negative tests | P7 staging/local transport |
| Producer crash 或 device-lost | queue 无限等待/资源泄漏 | host-side timeout、session error、销毁顺序 | 停止 session；legacy/static |
| Fabric-bound instance 与 CPU update 双写 | 非确定 transform | 每通道 authority；API 明确拒绝/epoch 更新 | 解绑 Fabric 或 legacy |
| `Engine::getInstance()` 返回陈旧动态值 | 上层误判 | 明确 API 语义；必要时显式 readback | 保持 CPU mirror（若批准） |
| descriptor/push constant ABI 变化 | shader/pipeline 不兼容 | bindings 追加、pipeline tests、双路径 | P3 legacy descriptors |
| 同时重构 frames-in-flight 放大风险 | 难定位同步错误 | 正确性和性能拆阶段 | P8 回到 queue-idle submit |
| 新独立模块与现有构建/索引脱节 | CI/维护风险 | CMake、FileMap、Graphify 更新门禁 | optional target 关闭 |

### 13.2 兼容策略

1. **保留 legacy CPU 路径**
   - `PhysicsFrameSource`
   - `UsdPhysicsReplaySource`
   - `PhysicsFabric` CPU adapter
   - `ApplyLatestFabricFrame`
   - `TrainingSceneRuntime::applyPoseUpdates`
   - host TLAS builder

   在 parity、性能和回滚演练完成前不删除。

2. **分层 feature flags**

   建议可独立选择：

   ```text
   pose source:       legacy-cpu / gpu-fabric
   raster transform: push-constant / transform-ssbo
   TLAS update:       host-builder / gpu-instance-buffer
   sensor pose:       cpu-metadata / sensor-pose-ssbo
   transport:         vulkan-local / external / host-staging
   policy:            strict-lockstep / latest-ready / ordered-no-drop（若批准）
   ```

3. **双写/影子比较**
   - P2 比较 CPU snapshot 与 GPU ABI；
   - P3 比较 push constant 与 SSBO；
   - P4 比较 CPU/GPU TLAS records 和输出；
   - P5 比较 sensor outputs。

4. **hdRobot 共存**
   - 未 Fabric-bound 的实例继续通过 CPU `Engine::updateInstance`；
   - geometry/BLAS/topology dirty 继续由 Hydra 路径处理；
   - Fabric 只覆盖明确绑定且获授权的动态通道。

5. **版本与失效**
   - ABI major 不兼容直接拒绝；
   - minor 只允许向后兼容的 reserved 扩展；
   - binding table、session、topology epoch 和 resource generation 分离；
   - 不依赖隐式对象顺序或运行时 Vulkan 地址作为稳定身份。

## 14. 测试计划

| 层级 | 必测内容 |
|---|---|
| USD 静态 | startTimeCode、无 animation、disabled sensor、几何/材质/light/camera/sensor 初始值 |
| USD replay | sample 有序并集、祖先 xform、visibility、全对象 snapshot、frame ordinal/time code、Reset、EOS、NoSource |
| Binding | 确定性 hash、重复/未知 ID、一对多、local offset、table/session/epoch mismatch、24/8-bit 上限 |
| ABI | C++/GLSL/CUDA `sizeof/alignof/offsetof`；alignment；identity/translate/rotate/nonuniform-scale/shear；NaN/Inf |
| Fabric | SPSC 并发、ring wrap、满环、背压、latest 跳帧、strict step、乱序 GPU completion、连续 release frontier、reset cutoff、EOS、timeout/crash |
| Raster | Preview/Tile transform、normal、visibility、depth、object/instance ID/AOV parity |
| TLAS | raw 64-byte record、mask、BLAS address、UPDATE/full rebuild、无新帧不 refit、ray-query parity |
| Sensor | lidar/height origin/axes、共线回退、ancestor transform、world gravity、pose-only 无 reconfigure |
| 兼容 | hdRobot CPU instance 更新、dynamic geometry/BLAS、sensor config、Fabric 未绑定场景 |
| Interop | CUDA/Vulkan 和 Vulkan/Vulkan positive；UUID/handle/timeline mismatch；跨进程退出；device-lost |
| 长时稳定性 | 高 publication 计数、epoch/reset 循环、ring wrap、pause/resume、Producer 快于/慢于 Consumer |

测试输出必须记录：

```text
tableContentHash
sessionId
streamEpoch
publicationId
sourceFrameId
sourceTimeDomain/value
selected policy
transport/capability report
```

这样训练输出或图像差异可以追溯到精确物理 publication。

## 15. 性能基线与验收测量

### 15.1 当前结构性基线

当前 replay 动态帧包含：

- CPU USD 采样和 `AnimationFrameUpdates` 构造；
- CPU `PhysicsFabric` publish/consume；
- 按名称查找并逐实例调用 `Engine::updateInstance`；
- Preview/Tile 每 draw push 完整 model matrix；
- TLAS dirty 时 CPU 构造 instance vector，nvvk 上传临时 buffer；
- sensor pose 变化时重新配置 output metadata；
- 整帧提交后 `vkQueueWaitIdle`。

这些是结构性基线，不是尚未测量的数值结论。

### 15.2 P0 基准协议

数值基线必须在 P0 记录，本文不虚构硬件无关阈值。

每组报告固定：

- git commit、build type、validation on/off；
- OS、CPU、RAM；
- GPU 型号、Vulkan device UUID、driver、Vulkan/CUDA 版本；
- scene 名称、mesh/instance/pose/sensor 数；
- 输出分辨率、ray/sensor 参数；
- warm-up 帧数、测量帧数、重复次数；
- Headless/Viewer、policy、transport。

至少记录：

| 指标 | 统计 |
|---|---|
| 总 CPU frame time | P50/P95/P99 |
| USD sample/pack time | P50/P95/P99 |
| pose apply/resolve CPU time | P50/P95/P99 |
| GPU pose validate/resolve time | P50/P95/P99 |
| TLAS build/update CPU 和 GPU time | P50/P95/P99 |
| raster/ray/sensor pass GPU time | P50/P95/P99 |
| simulation-ready 到 render-consume 延迟 | P50/P95/P99 |
| dropped/skipped publications | count/rate |
| Producer backpressure duration | P50/P95/P99 |
| per-frame allocations、handle operations、queue-idle calls | count |
| Fabric GPU memory | ring size、slot bytes、总量 |

### 15.3 结构性性能门槛

不依赖 D-020 数值阈值的最低要求：

- GPU Fabric steady state 无每帧 allocation、external import/export。
- GPU Producer 路径无逐帧 pose CPU readback和 mesh name lookup。
- transform-only 帧不创建 TLAS instance/scratch 临时 buffer。
- 无新 pose 帧不执行 resolve/TLAS refit。
- latest-ready 不等待未 ready publication。
- strict lockstep 不丢帧，等待时间可观测。
- 若 D-017 批准异步提交，最终 steady state 不以
  `vkQueueWaitIdle` 作为 Fabric 正确性的同步手段。

数值回归阈值必须由 D-020 明确后写入本文。

## 16. Progress Log

| 日期 | Status | 进展 | 证据/下一步 |
|---|---|---|---|
| 2026-07-28 | Draft | 完成现有调用链、所有权、USD 语义、raster/TLAS/sensor 数据路径的只读审计；形成 GPU Fabric、binding、ABI、同步、阶段、测试和回滚草案。未实现任何功能。 | 等待用户确认 D-001–D-020，并审核本文 |

### 状态推进规则

```text
Draft
  -> 用户关闭阻塞决策并批准本文
Approved
  -> 首个实现阶段实际开始
In Progress
  -> 第 12 节全部满足且回滚演练完成
Complete
```

每次实现变更必须在 Progress Log 增加：

- 日期和阶段；
- 已完成 task/验收证据；
- 新发现风险或决策；
- 是否触发/演练回滚；
- 下一阶段进入条件。
