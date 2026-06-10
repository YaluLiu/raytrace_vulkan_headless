# World0.usd 在 usdtweak 可打开但 hydra_capture + hdRobot 崩溃的原因

## 结论

`World0.usd` 本身可以被 OpenUSD 正常打开和组合；崩溃不是 `UsdStage::Open()` 失败。

当前崩溃发生在 `hydra_capture` 选择 `HdRobotRendererPlugin` 后第一次执行 Hydra render/commit resources 时。`hdRobot` 会把 Hydra scene store 里的所有 active/valid mesh 上传到后端 engine；其中包括 `World0.usd` 里机器人 collision 几何对应的 `purpose = "guide"` mesh。该类 guide collision mesh 在 hdRobot 的 `EngineSceneSync::_UploadInitialScene()` 路径里仍被上传，最终在后端 Vulkan/NVIDIA GL interop 的 mesh resource allocation 中段错误。

`usdtweak` 能打开，是因为它主要走 USD stage open/compose/edit/view 路径，不一定选择 `HdRobotRendererPlugin`，也不一定进入 hdRobot 的 mesh upload/resource allocation 路径。即使它显示场景，也可能使用不同 renderer 或不提交 guide collision mesh 到 hdRobot 后端。

## 已验证现象

在 `/home/yalu/hydra/hydra_capture` 中复现：

```bash
./run.sh aovs
```

现象：

```text
Using camera: /World/envs/env_0/Robot/camera_optical_face/front_camera
Rendering /home/yalu/docker/assets/demo5/World0.usd at 1280x720
[Engine] Tile multiview supported ...
Segmentation fault
```

这说明 stage 已经打开，并且 camera 已经找到；崩溃发生在 render 阶段，不是打开 USD 文件阶段。

gdb 栈的关键路径：

```text
HydraCaptureEngine::Render()
UsdImagingGLEngine::Render()
HdEngine::Execute()
hdrobot::EngineSession::Impl::CommitResources()
hdrobot::EngineSceneSync::EnsureResources()
hdrobot::EngineSceneSync::_UploadInitialScene()
hdrobot::UploadMeshToRenderer()
MeshStore::uploadMesh()
nvvk::ResourceAllocator::createBuffer()
nvvk::DeviceMemoryAllocator::allocInternal()
libnvidia-glcore.so
```

对照实验：

```bash
# 可以成功
./build-codex/hydra_capture \
  --renderer-config config/plugins/hdStorm/plugin.json \
  --usd /home/yalu/docker/assets/demo5/World0.usd \
  --output-dir output/world0_hdStorm \
  --aov color

# 可以成功
./build-codex/hydra_capture \
  --renderer-config config/plugins/hdRobot/plugin.json \
  --usd /home/yalu/docker/assets/tile/pao/tile_pao.usd \
  --output-dir output/tile_pao_hdRobot \
  --aov color
```

`World0.usd` 也通过了：

```bash
usdchecker /home/yalu/docker/assets/demo5/World0.usd
```

输出为：

```text
Success!
```

## 最小触发条件

`World0.usd` 中 `Robot` 子树下有大量 collision prim，典型结构如下：

```text
/World/envs/env_0/Robot/FL_calf          Xform
/World/envs/env_0/Robot/FL_calf/visuals Mesh      purpose = default
/World/envs/env_0/Robot/FL_calf/collisions Cube   purpose = guide
```

验证结果：

- 单独 reference `FL_calf/visuals`：成功。
- 单独 reference `FL_calf/collisions`：成功。
- reference 整个 `FL_calf`：崩溃。
- reference 整个 `FL_calf`，但禁用 `collisions`：成功。
- reference 整个 `FL_calf`，只把 `collisions.purpose` 从 `guide` 改成 `default`：成功。
- reference 完整 `World0.usd`，禁用所有名为 `collisions` 的 prim：成功，并且 LiDAR CSV export 与 overlay 也成功。

因此当前最明确的触发点是：hdRobot 初始资源上传阶段把本帧不该显示的 `purpose=guide` collision mesh 上传进后端 engine。

## hydra_capture 与 usdtweak 的差异

`hydra_capture` 的路径：

1. 创建 GL context。
2. 创建 `UsdImagingGLEngine`。
3. `UsdStage::Open(World0.usd)`。
4. `SetRendererPlugin("HdRobotRendererPlugin")`。
5. 设置 AOV、camera、viewport。
6. 调用 `UsdImagingGLEngine::Render()`。
7. Hydra 执行 hdRobot render delegate 的 `CommitResources()`。
8. hdRobot 把 scene store 里的 mesh 上传到 renderer backend。
9. guide collision mesh 被上传，后端分配资源时崩溃。

`usdtweak` 的路径不同：

1. 打开/组合 USD stage。
2. 读取 layer、prim、属性、关系和材质。
3. 可能使用自己的 viewport/renderer；即使显示，也不等价于 `hydra_capture` 中的 `HdRobotRendererPlugin` 离屏渲染路径。
4. 通常不会执行 hdRobot 的 `EngineSceneSync::_UploadInitialScene()` 和后端 `engine.uploadMesh()`。

所以“usdtweak 可以打开”只能证明 USD 文件可读、可组合；不能证明 hdRobot renderer 能处理该场景的所有 Hydra rprim。

## hdRobot 源码中的相关路径

源码目录：

```text
/home/yalu/RayTracing/robot_raster/hdRobot
```

关键文件：

- `renderDelegate.cpp`
  - `HdRobotRenderDelegate::CommitResources()` 会调用 `_sceneStore.ApplyPendingUpdates()` 和 `_engineSession->CommitResources(...)`。
- `engineSession.cpp`
  - `EngineSession::Impl::CommitResources()` 会调用 `sceneSync.EnsureResources(...)`，随后 `sceneSync.SyncSceneToEngine(...)`。
- `engineSceneSync.cpp`
  - `_EnsureInitialResources()` 会调用 `_UploadInitialScene()`。
  - `_UploadInitialScene()` 当前遍历所有 `sceneStore.GetMeshRecordsSnapshot()`，只检查 `record.active` 和 `record.data.valid`，没有按当前帧 render tags 过滤。
  - `ApplyFrameRenderTags()` 后续才根据 render tags 更新 instance visibility。
- `mesh.cpp`
  - `HdRobotMesh::Sync()` 读取 `sceneDelegate->GetRenderTag(id)` 并写入 `HydraMesh::renderTag`。
- `sceneData.h`
  - `HydraMesh::renderTag` 默认是 `HdRenderTagTokens->geometry`。

核心问题是时序：

```text
CommitResources()
  -> EnsureResources()
      -> _UploadInitialScene()          # 这里已经上传所有 active/valid mesh
  -> SyncSceneToEngine()

RenderPass::_Execute(renderTags)
  -> PassBridge::RenderFrameAndCopyAovs(...)
      -> ApplyFrameRenderTags(renderTags) # 这里才按 renderTags 控制可见性
      -> engine.render()
```

如果 `renderTags` 不包含 guide，guide mesh 也应该避免进入本次后端资源上传；至少不应该在不可见之前先上传到会崩溃的后端资源路径。

## 建议修改方案

优先推荐在 hdRobot 内部修复，而不是要求上游 USD 文件删除 collision prim。

### 方案 A：上传阶段跳过非 geometry render tag

在 `EngineSceneSync::_UploadInitialScene()`、`_UpdateGeometry()`、`_UpdateInstances()` 中，不上传 `record.data.renderTag != HdRenderTagTokens->geometry` 的 mesh。

优点：

- 改动范围小。
- 与 `hydra_capture` 当前 render params 一致：默认不显示 guide。
- 可以避免 guide collision mesh 进入后端 resource allocation。

风险：

- 如果未来需要显示 guide/proxy/render tag，需要扩展为按当前 active render tags 上传，而不是硬编码只允许 geometry。

### 方案 B：把 active render tags 提前传入资源同步

把 render pass 收到的 `renderTags` 提前传给 `EngineSession::CommitResources()` / `EngineSceneSync::EnsureResources()`，让 `_UploadInitialScene()` 只上传当前 pass 需要的 mesh。

优点：

- 语义更接近 Hydra：render tag 是 pass 级过滤条件。
- 将来支持 guide/proxy 更自然。

风险：

- 改动链路较长：`HdRobotRenderPass::_Execute()`、`PassBridge`、`EngineSession`、`EngineSceneSync` 都要调整。
- 当前 `CommitResources()` 是 render delegate 层调用，早于 render pass `_Execute()` 拿到 renderTags；需要重新设计缓存和延迟上传时机。

### 方案 C：保留上传，但让 guide mesh 走安全 no-op

保留 `HydraMesh` 记录，但对于 `renderTag == HdRenderTagTokens->guide` 的 mesh，不调用 `engine.uploadMesh()`，只创建一个空 binding 或延迟 binding。

优点：

- 对 `SceneStore` 数据模型影响小。
- 保留未来按需上传 guide 的空间。

风险：

- 需要小心 `_GetBinding()`、`UpdateRendererInstances()`、material update 等逻辑处理空 binding。

## 推荐落地步骤

1. 先实现方案 A，作为最小修复：

   - 在 `engineSceneSync.cpp` 增加 helper：

     ```cpp
     bool ShouldUploadMeshToRenderer(const HydraMesh& mesh)
     {
       return mesh.renderTag == HdRenderTagTokens->geometry;
     }
     ```

   - `_UploadInitialScene()` 中把条件从：

     ```cpp
     if(!record.active || !record.data.valid)
     {
       continue;
     }
     ```

     改成：

     ```cpp
     if(!record.active || !record.data.valid || !ShouldUploadMeshToRenderer(record.data))
     {
       continue;
     }
     ```

   - `_UpdateGeometry()` 和 `_UpdateInstances()` 中，当 binding 不存在且 mesh 不是 geometry tag 时，不要调用 `UploadMeshToRenderer()`。

2. 如果需要保留 guide 显示能力，再做方案 B：

   - 增加 active render tags 缓存。
   - 只有当当前 pass 包含 `HdRenderTagTokens->guide` 时才上传 guide mesh。
   - renderTags 变化时允许增量上传之前跳过的 mesh。

3. 增加诊断日志，便于确认过滤生效：

   ```text
   [EngineSceneSync] Skip mesh /.../collisions renderTag=guide
   ```

   建议只在 debug 环境或首次跳过时打印，避免大场景刷屏。

## 验证用例

修复后至少验证以下命令：

```bash
cd /home/yalu/hydra/hydra_capture

./build-codex/hydra_capture \
  --renderer-config config/plugins/hdRobot/plugin.json \
  --usd /home/yalu/docker/assets/demo5/World0.usd \
  --output-dir output/world0 \
  --aov color \
  --export-lidar-point-cloud output/lidar_json/lidar_point_cloud.csv
```

期望：

- 不再 Segmentation fault。
- 输出 `Render iterations: 1 (...)`。
- 输出 `Exported LiDAR point cloud ...`。
- 生成 `output/world0/World0/color.png`。

同时验证原来能工作的场景没有回退：

```bash
./build-codex/hydra_capture \
  --renderer-config config/plugins/hdRobot/plugin.json \
  --usd /home/yalu/docker/assets/tile/pao/tile_pao.usd \
  --output-dir output/tile_pao_hdRobot \
  --aov color
```

## 注意事项

- 不建议在 `hydra_capture` 里默认改 USD 或默认禁用 `*/collisions`。那只是绕过；根因在 hdRobot 对 render tag/purpose 的资源上传处理。
- `purpose=guide` 的 prim 可能仍有合法用途。不要简单删除 scene store 记录；更稳妥的是在后端上传阶段按 render tag 跳过或延迟。
- 如果未来要支持 `showGuides=true`，不能永远硬编码跳过 guide；应使用当前 pass 的 renderTags 决定是否上传。
