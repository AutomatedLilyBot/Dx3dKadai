# Golf Game - 战术射击 Demo 设计文档

## 目录
1. [资源管理系统](#1-资源管理系统)
2. [核心游戏玩法 Demo](#2-核心游戏玩法-demo)
3. [UI 与场景管理](#3-ui-与场景管理)
4. [视觉效果](#4-视觉效果)
5. [实现计划](#5-实现计划)
6. [技术细节备注](#6-技术细节备注)
7. [问题与待确认项](#7-问题与待确认项)
8. [资源清单](#8-资源清单)
9. [成功标准](#9-成功标准)

---

## 1. 资源管理系统

### 1.1 ResourceManager 设计

**职责**：
- 统一管理模型（Model）和纹理（Texture）资源
- 避免重复加载相同资源
- 提供资源生命周期管理（延迟加载、引用计数、卸载）

**接口设计**：
```cpp
class ResourceManager {
public:
    // 获取或加载模型（如果已加载则返回缓存）
    Model* getModel(const std::wstring& path);

    // 获取或加载纹理
    ID3D11ShaderResourceView* getTexture(const std::wstring& path);

    // 预加载资源列表
    void preloadResources(const std::vector<std::wstring>& paths);

    // 清理未使用的资源
    void cleanup();

private:
    std::unordered_map<std::wstring, std::unique_ptr<Model>> models_;
    std::unordered_map<std::wstring, ComPtr<ID3D11ShaderResourceView>> textures_;
};
```

**存储位置**：
- 头文件：`src/core/resource/ResourceManager.hpp`
- 实现文件：`src/core/resource/ResourceManager.cpp`
- Scene 持有 ResourceManager 实例，传递给实体工厂

**资源路径约定**：
- 模型：`asset/models/{name}.fbx`
- 纹理：`asset/textures/{name}.png`
- 使用相对路径，由 ResourceManager 内部解析为绝对路径

---

## 2. 核心游戏玩法 Demo

### 2.1 游戏场景：BattleScene

继承自 `Scene`，替代当前的 `PlayScene`。

**场景组成**：
1. 地面平台（16×16×1 的方块网格）
2. 围墙（用于限制子弹球的运动范围，墙本身不销毁）
3. 多个 Node（建筑节点）
   - 初始有：1 个玩家 Node，若干敌方 Node（随难度），大量中立 Node
   - Node 永远不销毁，只改变 team 来表示归属
4.子弹球（BulletEntity），受物理影响并与墙/地板/Node 交互

---

### 2.2 核心实体设计

#### 2.2.1 BlockEntity（方块实体）

**继承**：`StaticEntity`

**属性**：
- 尺寸：1×1×1（标准立方体）
- 碰撞体：OBB，尺寸与模型匹配
- 模型：使用 `cube.fbx`（复用现有资源）

**特殊功能开关**：
```cpp
class BlockEntity : public StaticEntity {
public:
    enum class ResponseType {
            None,
    DestroyBullet,   // 只销毁子弹
    BounceBullet,    // 只反弹子弹
    SpecialEvent     // 用于可破坏墙/机关等，未来扩展
    };

    ResponseType responseType = ResponseType::None;

    void onCollision(WorldContext& ctx, EntityId other,
                     TriggerPhase phase, const OverlapResult& c) override;
};
```

**围墙特殊处理**：
- 围墙使用 `ResponseType::DestroyBullet`
- 被子弹击中时调用 `ctx.commands->destroyEntity(other)`

---

#### 2.2.2 NodeEntity（发射塔实体）

反馈修改点：

Node 不参与位移，不下落，仅旋转 → 不当作 Dynamic 受重力刚体。

主碰撞体使用 Capsule Collider。

Node 只改变 team，不被销毁。

**继承**：逻辑上当作“静止但可旋转”的对象：

- 可以继承 StaticEntity 或单独增加 KinematicEntity 基类。
- 刚体 invMass = 0（不受重力），只用 Transform 控制旋转。

**模型**：
- 主体：圆柱体模型 `cylinder.fbx`（或临时拉伸 cube）
- 尺寸：底面半径 0.5，高度 1.0

**碰撞体组成**：
1. **主碰撞体**（竖直 capsule（胶囊体），下端接触地面，上端略高于模型顶部）
   - 位置：实体中心
   - 用途：实体本体碰撞检测

2. **前方触发器**（Trigger OBB）
   - 位置：实体前方（局部坐标 z 轴正方向偏移）
   - 尺寸：1×1×2（宽×高×深）
   - 用途：检测发射路径是否被占用

**阵营标记**：
```cpp
enum class NodeTeam {
    Friendly,  // 友方(玩家可控)
    Enemy      // 敌方 (有AI,后期编写自动交战AI)
    Neutral    // 中立(不可控，无AI，供占领，地图生成时的主要node构成) 
};
```

**状态机**（简化设计）：

> **设计说明**：Node 自身只需要两个状态：`Idle` 和 `Firing`。
> - "选中"功能由 `InputManager` 维护当前选中的 Node ID，不需要 Node 自身保存选中状态。
> - 选中一个正在 `Firing` 的 Node，然后左键点击其他地方取消选中，该 Node 仍保持 `Firing` 状态。
> - 选中过程中指定开火方向后，Node 进入 `Firing` 状态，但仍保持选中状态，玩家可以继续调整方向或进行其他操作。

```cpp
enum class NodeState {
    Idle,           // 空闲（未开火）
    Firing          // 自动发射模式（持续发射直到手动停止或被击中改变阵营）
};
```

**核心属性**：
```cpp
class NodeEntity : public StaticEntity {
public:
    NodeTeam  team  = NodeTeam::Neutral;
    NodeState state = NodeState::Idle;

    // 发射参数（Demo 简化版）
    float fireInterval   = 2.0f;    // 发射间隔（秒）
    float fireTimer      = 0.0f;    // 当前计时器
    float bulletSpeed    = 10.0f;   // 子弹初速度
    float bulletRadius   = 0.25f;   // 子弹半径（用于创建 capsule/sphere）

    // 主碰撞体：Capsule（竖直）
    std::unique_ptr<CapsuleCollider> bodyCollider;

    // 可选：前方 Trigger
    std::unique_ptr<OBBCollider> frontTrigger;

    // 当前朝向（世界空间下的前方单位向量，XZ 平面为主）
    DirectX::XMFLOAT3 facingDirection = {0, 0, 1};

    void update(WorldContext& ctx, float dt) override;
    void setFacingDirection(const DirectX::XMFLOAT3& worldTarget); // 更新朝向（只绕Y旋转）
    bool isFrontClear(WorldContext& ctx) const;                     // 可选：检测前方是否无阻挡
    void fireBullet(WorldContext& ctx);                             // 发射子弹
    void startFiring();                                             // 开始发射
    void stopFiring();                                              // 停止发射

    // 子弹命中时调用，用于改变 team（Demo 里直接切阵营）
    void onHitByBullet(WorldContext& ctx, NodeTeam attackerTeam);
};
```

**更新逻辑**：
```cpp
void NodeEntity::update(WorldContext& ctx, float dt) {
    // 只有处于 Firing 状态的 Node 会发射子弹
    if (state == NodeState::Firing) {
        fireTimer += dt;
        if (fireTimer >= fireInterval /* && isFrontClear(ctx) 可选 */) {
            fireBullet(ctx);
            fireTimer = 0.0f;
        }
    }

    // 将 facingDirection 写回 Transform.rotation（只绕 Y 轴）
    // 注意：只允许绕 Y 轴旋转，pitch/roll = 0
}

void NodeEntity::startFiring() {
    state = NodeState::Firing;
    fireTimer = 0.0f; // 可选：立即开始计时或重置
}

void NodeEntity::stopFiring() {
    state = NodeState::Idle;
}
```
**阵营改变（重要：Node 不销毁，只换队）：**
```cpp
void NodeEntity::onHitByBullet(WorldContext& ctx, NodeTeam attackerTeam) {
if (team == attackerTeam) {
// 同阵营：未来可以做强化逻辑，Demo 暂时什么也不做或加数值
return;
}

    // Demo 简化版：被击中的 Node 直接归属攻击方
    team = attackerTeam;

    // TODO: 更新材质/颜色表现不同队伍
}
```

**方向设置**：
- 接收世界坐标中的目标点
- 计算从 Node 位置到目标点的方向向量（忽略 Y 轴，保持水平）
- 更新 `transform.rotation`（四元数）
- 同步更新 `frontTrigger` 的位置和旋转

---

#### 2.2.3 BulletEntity（子弹实体）

反馈修改点：

- 子弹撞墙时：销毁子弹 + Billboard 死亡特效，墙不销毁。

- 子弹速度低于某阈值也会自动销毁。

- 未来：不同队伍的子弹互相碰撞，按 power 判定；Demo 暂不实现 power 判定 & 敌方 AI。


**继承**：`DynamicEntity`

**模型**：
- 使用现有的 `ball.fbx`
- 半径：0.25（可配置）

**物理**：
- 刚体：动态
- 碰撞体：Sphere Collider
- 初始化时施加冲量：`rb.applyImpulse(direction * speed)`

阵营 & power（未来扩展）：
```cpp
class BulletEntity : public DynamicEntity {
public:
NodeTeam team = NodeTeam::Friendly; // 发射方阵营
float power   = 1.0f;               // Demo 可固定为 1，未来与 Node 强度挂钩

    float minSpeedToLive = 1.0f;        // 速度低于此值 → 自动销毁
    // ...
};
```

更新逻辑（速度过低自动销毁）：
```cpp
void BulletEntity::update(WorldContext& ctx, float dt) {
auto* rb = rigidBody();
if (!rb) return;

    const auto& v = rb->velocity;
    float speedSq = v.x*v.x + v.y*v.y + v.z*v.z;
    if (speedSq < minSpeedToLive * minSpeedToLive) {
        // 速度太低，直接销毁 + 播放特效
        spawnDeathEffect(ctx, transformRef().position);
        ctx.commands->destroyEntity(id());
    }
}
```

**碰撞响应(demo)**：
```cpp
void BulletEntity::onCollision(WorldContext& ctx, EntityId other,
                               TriggerPhase phase, const OverlapResult& c) {
    if (phase != TriggerPhase::Enter) return;

    IEntity* otherEntity = ctx.entities->find(other);
    if (!otherEntity) return;

    // 1. 撞到 Node：改变 Node 的 team
    if (auto* node = dynamic_cast<NodeEntity*>(otherEntity)) {
        node->onHitByBullet(ctx, team);
        spawnDeathEffect(ctx, c.contactPoint); // 在命中点播放爆炸
        ctx.commands->destroyEntity(id());
        return;
    }

    // 2. 撞到 Block：交给 BlockEntity 处理（墙/地板）
    if (auto* block = dynamic_cast<BlockEntity*>(otherEntity)) {
        switch (block->responseType) {
        case BlockEntity::ResponseType::DestroyBullet:
            spawnDeathEffect(ctx, c.contactPoint);
            ctx.commands->destroyEntity(id());
            break;
        case BlockEntity::ResponseType::BounceBullet:
            // 修改 rb->velocity 做反弹
            bounceFromSurface(c.normal);
            break;
        case BlockEntity::ResponseType::SpecialEvent:
            // 未来扩展：可破坏墙、机关等
            break;
        case BlockEntity::ResponseType::None:
        default:
            // 默认可以视为 Bounce 或 DestroyBullet，按需要调整
            bounceFromSurface(c.normal);
            break;
        }
        return;
    }

    // 3. 子弹 vs 子弹（未来扩展：按 power 判定）
    if (auto* otherBullet = dynamic_cast<BulletEntity*>(otherEntity)) {
        // 未来：比较 power，销毁较弱一方并反弹较强一方
        // Demo 暂时可以简单点：双方都反弹，或者都销毁
    }
}
```

**胜利处理**： 胜负判断逻辑（不再是“击中一个敌方 Node 就胜利”）：

胜负完全由 全场 Node 的归属 决定：

若 所有 Node 的 team 都是 Friendly → 玩家胜利。

若 玩家 Friendly Node 数量变为 0（且场上仍有 Enemy/Neutral） → 玩家失败。

该逻辑在 BattleScene 中统一判断（见 2.6）。

---

### 2.3 玩家交互系统

反馈修改点（3）总结：

选中 Node 后：

Node 允许绕 Y 轴 360° 旋转。

视角俯仰（pitch）要限制范围，避免穿墙视角。

当前自由相机使用鼠标控制旋转；

选中后鼠标左键 / 右键用途冲突：

设计为：右键短按 = 指定发射方向；右键长按 = 进入“绕 Node 旋转视角”模式，在该模式下鼠标用于绕 Node 旋转相机。

#### 2.3.1 输入管理器（InputManager）

**职责**：
- 处理鼠标点击 / 长按
- 执行射线检测（Raycast）
- 管理选中 Node
- 区分“右键短按”和“右键长按”（长按进入绕 Node 旋转视角）

**关键功能**：
```cpp
class InputManager {
public:
    // 从屏幕坐标发射射线
    Ray screenPointToRay(int screenX, int screenY, const Camera& camera);

    // 射线检测场景中的实体
    EntityId raycastEntities(const Ray& ray, const Scene& scene, float maxDist = 100.0f);

    // 射线与 XZ 平面相交（用于右键短按选择发射方向）
    bool raycastPlane(const Ray& ray, float planeY, DirectX::XMFLOAT3& hitPoint);

    // 右键按下时开始计时，返回是否视为“长按”
    void updateMouseButtons(float dt);
    bool isRightClickShort() const;
    bool isRightClickLong() const;

private:
    float rightButtonHoldTime_ = 0.0f;
    bool  rightButtonDown_     = false;
    bool  rightClickShort_     = false;
    bool  rightClickLong_      = false;
    float rightLongPressThreshold_ = 0.3f; // >0.3s 视为长按
};

```

**实现位置**：
- `src/game/input/InputManager.hpp`
- 在 `BattleScene` 中创建实例

#### 2.3.2 交互流程

**左键点击（选择/取消选择）**：
1. 发射射线 `raycastEntities()`
2. 如果击中友方 Node：
   - 调用 `inputManager.selectNode(nodeId)`
   - 触发相机切换到 Orbit 模式（移动到 Node 后方俯视）
3. 如果击中其他（或空地）：
   - 如果有选中的 Node，调用 `inputManager.deselectNode()`
   - **注意：取消选中不改变 Node 的开火状态（Firing 的 Node 继续 Firing）**
   - 恢复相机自由控制

**右键点击（指定方向）**：
- 仅在有选中 Node 时有效
- 发射射线与 `y = selectedNode.position.y` 平面相交
- 获取交点 `hitPoint`
- 调用 `selectedNode->setFacingDirection(hitPoint)`
- 调用 `selectedNode->startFiring()` 使 Node 进入 Firing 状态
- **注意：指定方向后仍保持选中状态，玩家可继续调整方向或操作相机**

**右键长按（控制相机旋转）：**

- 条件：当前有选中 Node，且相机已处于 Orbit 模式。
- 行为：
  - 按住右键时，鼠标移动控制相机绕当前选中 Node 旋转：
    - Yaw：绕 Node 的 Y 轴 360° 旋转（无限制）。
    - Pitch：限制在一个合理范围（例如 -30° ~ +60°），防止摄像机钻入墙里或地板下。
  - 松开右键时，相机停止响应鼠标移动，保持当前角度。
- **注意：不需要"按住才进入环绕模式"的设计，选中 Node 后相机直接进入 Orbit 模式，右键只是决定是否接受鼠标输入。**

---

### 2.4 相机控制系统

#### 2.4.1 相机模式

```cpp
enum class CameraMode {
    Free,       // 自由移动（WASD + 鼠标旋转）
    Orbit       // 绕当前选中 Node 旋转（选中 Node 后自动进入）
};
```
- Free：

    - 玩家未选中 Node，使用当前逻辑（鼠标控制视角，键盘移动）。

- Orbit：

    - **选中 Node 后直接进入此模式**，摄像机以选中 Node 为中心：
        - **右键长按时**：鼠标移动控制 Yaw/Pitch 旋转相机。
        - **右键未按住时**：相机保持当前角度，不响应鼠标移动。
        - Yaw ∈ [0, 360°) 自由旋转。
        - Pitch 被 clamp（例如 pitchMin = -30°，pitchMax = +60°）。

#### 2.4.2 相机动画


**平滑移动**：
- 使用 `Transform::lerp()` 或 `moveTowards()`
- 动画时长：1.0 秒
- 选中 Node 时平滑移动相机到 Node 后方俯视位置

**Orbit 模式下的旋转：**

- 保持摄像机与 Node 的距离不变，仅改变围绕 Node 的极坐标角度。
- **只有右键按住时才接受鼠标输入进行旋转**，松开右键时保持当前角度不变。

- 对 Pitch 做 clamp：
```cpp
pitch = std::clamp(pitch, pitchMin, pitchMax);
```

---

### 2.5 场景布局 & Node 随机生成

反馈修改点：

游戏开始时自动生成场地和随机数量的 Node。

从中选 1 个作为玩家 Node，若干个作为敌方 Node，大多数为中立 Node。

Node 不销毁，只改变 team。

全部 Node 变为 Friendly → 胜利；玩家没有任何 Friendly Node → 失败。

场地生成（示例逻辑）：

- 地板：

仍然生成一个矩形区域（例如 16×16 或 20×20）由 BlockEntity 铺成。

- 围墙：

在场地周围一圈生成多层 BlockEntity，设置 ResponseType::DestroyBullet，限制子弹飞出场外。

- Node 生成：
```cpp
struct BattleConfig {
int fieldSizeX = 16;
int fieldSizeZ = 16;
int totalNodesMin = 8;
int totalNodesMax = 16;

    int enemyNodeCountEasy   = 2;
    int enemyNodeCountNormal = 3;
    int enemyNodeCountHard   = 4;
};
```

- 生成流程：

1.在场地范围内随机采样若干坐标作为 Node 位置（保证与墙和彼此间至少有一定距离）。

2.随机排列这些位置，选择第一个作为玩家 Node（team = Friendly）。

3.按难度从剩余位置中选若干个作为敌方 Node（team = Enemy）。

4.其余全部设置为 team = Neutral。

#### 胜负判断（BattleScene 内统一处理）：

```cpp
void BattleScene::updateWinLose(WorldContext& ctx) {
int friendlyCount = 0;
int totalCount    = 0;

    for (auto& e : entities_) {
        if (auto* node = dynamic_cast<NodeEntity*>(e.get())) {
            ++totalCount;
            if (node->team == NodeTeam::Friendly) ++friendlyCount;
        }
    }

    if (totalCount > 0 && friendlyCount == totalCount) {
        // 所有 Node 被玩家控制 → 胜利
        onBattleResult(/*win=*/true);
    } else if (friendlyCount == 0) {
        // 玩家失去所有 Node → 失败
        onBattleResult(/*win=*/false);
    }
}
```
### 2.6 场景切换与胜利/失败（立刻切换）

反馈修改点：

胜负出现后立刻开始切换场景，不需要长时间停留。

可以经过 TransitionScene 做简单淡入淡出，但触发应是即时的。

接口设计（示例）：
```cpp
class BattleScene : public Scene {
void onBattleResult(bool win) {
// 通知 SceneManager 立即切换
if (auto* mgr = sceneManager()) {
mgr->transitionTo(std::make_unique<MenuScene>(), /*useFade=*/true);
}
}
};

```
- transitionTo 内部可以：

  - 先推入 TransitionScene → 立即开始 FadeOut → 切换 → FadeIn → MenuScene。

- 整个流程从逻辑上立即开始，不会在战斗场景里额外停几秒。

---

## 3. UI 与场景管理

### 3.1 场景架构

```
MenuScene (菜单场景)
    ↓ [Start Game]
TransitionScene (过场动画)
    ↓
BattleScene (战斗场景)
    ↓ [Victory/Return]
TransitionScene
    ↓
MenuScene
```

### 3.2 MenuScene 设计

**UI 元素**（简单版）：
- 标题文字："Golf Game - 战术射击"
- 按钮："开始游戏" → 切换到 BattleScene
- 按钮："退出" → 关闭程序

**实现方式**：
- 使用 SFML 的文本渲染（`sf::Text`）
- 或使用 ImGui（如果集成）
- 或使用 Billboard 纹理（纯 3D 方案）

**相机**：
- 固定位置，正视 UI 平面

### 3.3 TransitionScene 设计

**效果**：
- 简单淡入淡出（Fade In/Out）
- 持续时间：0.5-1.0 秒

**实现**：
- 渲染全屏四边形（Quad）
- Alpha 值从 0 → 1 → 0
- 使用 Renderer 的后处理功能或直接绘制

**时序**：
```
1. 当前场景 → TransitionScene (Fade Out)
2. TransitionScene 切换目标场景
3. TransitionScene → 目标场景 (Fade In)
```

### 3.4 场景管理器扩展

在 `golfgame.cpp` 中添加场景栈或状态机：

```cpp
class SceneManager {
public:
    void pushScene(std::unique_ptr<Scene> scene);
    void popScene();
    void replaceScene(std::unique_ptr<Scene> scene);
    void transitionTo(std::unique_ptr<Scene> scene); // 自动插入过场

    Scene* currentScene() { return sceneStack_.back().get(); }

private:
    std::vector<std::unique_ptr<Scene>> sceneStack_;
};
```

---

## 4. 视觉效果

### 4.1 Trail（拖尾效果）

**实现方式**：
- **方案 1：Line Strip**
  - BulletEntity 记录历史位置（固定长度队列）
  - 每帧绘制连接这些点的线条（使用 Line Topology）
  - 颜色渐变（头部亮，尾部暗）

- **方案 2：Ribbon（带状拖尾）**
  - 基于 Line Strip，但生成四边形网格
  - 使用纹理实现更好的视觉效果

**属性**：
```cpp
class TrailRenderer {
public:
    void addPoint(const XMFLOAT3& position);
    void render(Renderer& renderer);

private:
    std::deque<XMFLOAT3> points_;
    int maxPoints_ = 20;
    XMFLOAT4 color_ = {1, 0.5f, 0, 1}; // 橙色
};
```

**集成**：
- 在 `BulletEntity` 中添加 `TrailRenderer` 成员
- 在 `update()` 中调用 `trail_.addPoint(transform.position)`
- 在场景 `render()` 中绘制所有 Trail

### 4.2 爆炸动画（Billboard）

**实现方式**：
- 使用 Billboard 技术（始终朝向相机的四边形）
- 纹理：爆炸序列帧（Sprite Sheet）或单张纹理

**流程**：
1. 子弹销毁时，在其位置生成 `ExplosionEffect` 实体
2. `ExplosionEffect` 播放动画（0.5 秒）
3. 动画结束后自动销毁

**实体设计**：
```cpp
class ExplosionEffect : public IEntity {
public:
    float lifetime = 0.5f;
    float currentTime = 0.0f;
    XMFLOAT3 position;

    void update(WorldContext& ctx, float dt) override {
        currentTime += dt;
        if (currentTime >= lifetime) {
            ctx.commands->destroyEntity(id());
        }
    }

    // 在 render() 中绘制 Billboard
};
```

**Billboard 渲染**：
- 顶点着色器中计算 Billboard 朝向
- 或在 CPU 端每帧更新四边形方向

---

## 5. 实施计划

### 阶段 1：资源管理系统（1-2 天）
**任务**：
- [ ] 创建 `ResourceManager` 类
- [ ] 实现模型加载与缓存
- [ ] 实现纹理加载与缓存
- [ ] 修改 `Scene` 持有 `ResourceManager` 实例
- [ ] 修改 `CommandBuffer::spawnBall` 使用 ResourceManager

**验收**：
- 能够通过 ResourceManager 加载模型，重复获取不重复加载
- 日志输出资源加载信息

---

### 阶段 2：核心实体实现（3-4 天）

#### 2.1 BlockEntity
- [ ] 创建 `BlockEntity` 类
- [ ] 实现 `ResponseType` 枚举和碰撞响应
- [ ] 测试地板反弹子弹、墙壁销毁子弹(墙不消失)

#### 2.2 NodeEntity
- [ ] 创建 `NodeEntity` 类
- [ ] 实现圆柱体模型（或用 cube 临时替代）
- [ ] 实现前方 Trigger 碰撞体
- [ ] 实现状态机（Idle/Firing）
- [ ] 实现 `setFacingDirection()` 旋转功能
- [ ] 实现 `isFrontClear()` 检测逻辑
- [ ] 实现 `fireBullet()` 发射逻辑

#### 2.3 BulletEntity
- [ ] 扩展现有的 `BallEntity` 为 `BulletEntity`
- [ ] 实现初始冲量施加
- [ ] 实现碰撞检测（击中 Node/墙壁）
  - Node：改变 Node.team，销毁子弹并生成爆炸特效
  - 地板：反弹
  - 墙体：销毁子弹并生成爆炸特效
- [ ] 实现低速自销毁逻辑

**验收**：
- Node 可以旋转并朝指定方向发射子弹
- 子弹在场地中运动，撞地板会弹起，撞墙会被销毁 + 播放特效。
- 子弹击中 Node 时，Node 的 team 会立即切换。
- 全场 Node 被玩家占领时，会立即触发胜利切换场景。
---

### 阶段 3：BattleScene 场景搭建（2 天）

- [ ] 创建 `BattleScene` 类
- [ ] 生成 16×16 地面平台
- [ ] 生成围墙（2 层高）
- [ ] 放置友方和敌方 Node
- [ ] 替换 `golfgame.cpp` 中的 `PlayScene` 为 `BattleScene`

**验收**：
- 场景正确渲染，包含所有实体
- 友方 Node 自动发射（测试用）

---

### 阶段 4：输入与相机控制（3 天）

#### 4.1 InputManager
- [ ] 创建 `InputManager` 类
- [ ] 实现屏幕坐标到射线转换
- [ ] 实现射线与实体相交检测
- [ ] 实现射线与平面相交检测

#### 4.2 相机控制
- [ ] 扩展 `Camera` 类支持模式切换
- [ ] 实现相机动画（平滑移动到 Node 后方）
- [ ] 集成到 BattleScene 输入处理

#### 4.3 交互流程
- [ ] 实现左键选择/取消选择
- [ ] 实现右键指定方向
- [ ] 测试完整交互流程

**验收**：
- 玩家可以左键选择友方 Node
- 相机平滑移动到 Node 后方
- 右键点击可以指定 Node 朝向
- Node 自动发射并击中目标

---

### 阶段 5：UI 与场景管理（2 天）

- [ ] 创建 `MenuScene` 类（简单文本菜单）
- [ ] 创建 `TransitionScene` 类（淡入淡出）
- [ ] 实现 `SceneManager` 类
- [ ] 集成场景切换逻辑（Menu → Battle → Menu）
- [ ] 在 BattleScene 中实现返回菜单功能

**验收**：
- 程序启动显示菜单
- 点击开始进入战斗场景
- 胜利后返回菜单
- 过场动画正常播放

---

### 阶段 6：视觉效果（2-3 天）

#### 6.1 Trail
- [ ] 创建 `TrailRenderer` 类
- [ ] 集成到 `BulletEntity`
- [ ] 调整拖尾长度、颜色参数

#### 6.2 爆炸动画
- [ ] 创建 `ExplosionEffect` 实体
- [ ] 实现 Billboard 渲染
- [ ] 准备爆炸纹理（或使用程序生成）
- [ ] 集成到子弹销毁逻辑

**验收**：
- 子弹飞行时有明显拖尾
- 子弹销毁时显示爆炸效果

---

### 阶段 7：测试与优化（1-2 天）

- [ ] 完整游戏流程测试
- [ ] 性能优化（如有必要）
- [ ] Bug 修复
- [ ] 参数调优（发射间隔、子弹速度、相机距离等）

---

## 6. 技术细节备注

### 6.1 射线检测实现

使用现有的碰撞检测系统：
```cpp
EntityId InputManager::raycastEntities(const Ray& ray, const Scene& scene, float maxDist) {
    EntityId closestEntity = 0;
    float closestDist = maxDist;

    // 遍历场景中所有实体的碰撞体
    for (const auto& [id, entity] : scene.getAllEntities()) {
        for (auto* collider : entity->colliders()) {
            float t;
            if (collider->intersectsRay(ray, t) && t < closestDist) {
                closestDist = t;
                closestEntity = id;
            }
        }
    }

    return closestEntity;
}
```

**需要为 Collider 添加**：
```cpp
virtual bool intersectsRay(const Ray& ray, float& t) const = 0;
```

### 6.2 冲量施加

在 `RigidBody` 中添加：
```cpp
void RigidBody::applyImpulse(const XMFLOAT3& impulse) {
    if (mass <= 0) return; // 静态物体忽略

    XMVECTOR v = XMLoadFloat3(&velocity);
    XMVECTOR j = XMLoadFloat3(&impulse);
    v = XMVectorAdd(v, XMVectorScale(j, 1.0f / mass));
    XMStoreFloat3(&velocity, v);
}
```

### 6.3 资源路径约定

所有资源使用相对于可执行文件的路径：
```
golfgame.exe
asset/
    models/
        cube.fbx
        cylinder.fbx
        ball.fbx
    textures/
        explosion.png
```

---

### 6.4 胜利 / 失败切换时机

- 本文设计为：一旦检测到胜负条件，在当前帧立即调用 SceneManager 进行场景切换（通过 TransitionScene 做短暂 Fade）。



## 7. 问题与待确认项


## 8. 资源清单

### 需要创建的模型：
- [x] `cube.fbx` - 已存在
- [x] `ball.fbx` - 已存在
- [ ] `cylinder.fbx` - 需要创建（圆柱体，半径 0.5，高度 1.0）

### 需要创建的纹理：
- [ ] `explosion.png` - 爆炸效果（64×64 或 128×128）
- [ ] (可选) `trail.png` - 拖尾纹理

### 可使用临时替代：
- 圆柱体模型 → 临时使用拉伸的 cube
- 爆炸纹理 → 程序生成渐变圆形

---

## 9. 成功标准

完成所有阶段后，应实现以下完整流程：

1. 启动程序 → 显示菜单
2. 点击"开始游戏" → 过场动画 → 进入战斗场景
3. 场景中包含：地面、围墙、友方 Node、敌方 Node
4. 左键点击友方 Node → 相机平滑移动到后方俯视（进入 Orbit 模式）
5. 右键短按地面 → Node 绕 Y 轴旋转至该方向，并进入 Firing 状态（保持选中）
6. 右键长按 → 玩家可以绕 Node 360° 水平旋转视角，Pitch 受限。
7. 子弹在场中飞行：
   - 撞地板反弹；
   - 撞墙销毁子弹并播放爆炸特效；
   - 撞 Node 改变 Node.team（Node 不销毁）。
8. 当所有 Node 都变为 Friendly 时 → 立即触发胜利切换（Fade + 返回菜单，或显示胜利画面）。
9. 若玩家失去所有 Friendly Node → 立即触发失败切换。

---

**文档版本**：v1.0
**创建日期**：2025-12-14
**最后更新**：2025-12-14
