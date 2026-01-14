Dx3dKadai ‑ Tactical Shooting Demo

This project is a 3D tactical shooting demo built with modern C++ (C++17) and Direct3D 11.
It demonstrates a mini‑game where the player captures and fortifies Nodes on a generated field by firing projectiles.
The engine features a modular scene system, an extensible entity/component architecture, physics simulation with triggers and colliders, instanced rendering, and transparent rendering whose opacity is driven by game data.

Gameplay overview

Field generation – At the start of a match the game generates a rectangular field of blocks and surrounding walls. A set of Node entities is distributed over the field: one is assigned to the player, a few belong to the enemy AI and the remainder are neutral.

Node capture mechanics – Each Node has a health‐like power value. When the player selects a Node it can rotate around the Y axis to aim and fire projectiles. If a projectile hits another Node it transfers power: hitting a neutral or enemy Node reduces its power and may change its team; hitting a friendly Node increases its power. Victory occurs when all Nodes belong to the player; defeat occurs when the player loses all Nodes.

Projectiles – Projectiles (BulletEntity) are dynamic rigid bodies. They have capsule or sphere colliders and are launched with an initial impulse from a Node’s facing direction. They bounce off the ground blocks, are destroyed on collision with walls and play a billboard explosion effect on death. Projectiles that slow below a threshold despawn automatically.

Nodes – Nodes are stationary (kinematic) entities with capsule colliders. They rotate to aim but do not move or fall. Each Node has a team (Friendly, Enemy, Neutral) and a power/health value that decays or increases based on projectile hits. A Node’s transparency in the scene is controlled by its health: at full power it is nearly opaque and at low power it becomes highly transparent.

Blocks & walls – Static blocks form the ground and walls. Blocks use an OBB collider and can bounce or destroy projectiles based on a response type. Walls are indestructible and always destroy projectiles on impact.

Engine and technical features
Instanced rendering and draw batching

Rendering many identical meshes individually is expensive because each call binds buffers and issues a draw call. The new Renderer collects draw requests (DrawItem) during the frame. Each DrawItem stores a pointer to the mesh, material, transform, per‑instance alpha, and a transparency flag.
At the end of the frame the renderer groups all opaque draw items by their (mesh, material) pair and draws each batch using instanced rendering; the instance buffer holds per‑instance transform rows and alpha values. This reduces thousands of draw calls to just a few. Transparent draw items are sorted back‑to‑front by distance to the camera and rendered individually with alpha blending and depth writing disabled. This design yields high rendering throughput and correct blending order for semi‑transparent Nodes.

Physics and collision system

The game uses a custom physics engine:

Rigid bodies and colliders – Rigid bodies can be static, dynamic or kinematic. Colliders include spheres, oriented bounding boxes (OBB) and capsules. Colliders only carry local offsets; their world transforms are injected each frame when synchronising with the owning entity. Static blocks and walls are flagged as sleeping so the broad phase skips static‑static pairs, greatly reducing collision checks.

Broad phase and narrow phase – The engine performs a broad phase using axis‑aligned bounding boxes (AABB) followed by a narrow phase that computes penetration and contact points. Recent optimisation added a spatial grid for static geometry and skips static–static tests, reducing physics step time.

Trigger and collision callbacks – Colliders can be triggers. The physics engine reports contact phases (Enter, Stay, Exit) and the scene dispatches them to entities via onCollision. This allows Nodes to detect when projectiles enter or leave their trigger zone and respond appropriately.

Scene and entity architecture

Entity interface – All game objects implement the IEntity interface with methods init(), update(), onCollision(), onDestroy() and access to their Transform. Entities register their colliders and rigid bodies with the physics world.

World context and command buffer – The scene passes a WorldContext to entities each frame. It contains time, physics queries, entity queries and a command buffer. Entities can spawn other entities (e.g., projectiles) or schedule destruction using the command buffer. Commands are deferred until after physics and update calls finish, preventing modification during iteration.

Scenes – The game defines several scenes. BattleScene encapsulates the tactical shooting gameplay: generating the field, instantiating Nodes, handling input, performing game logic and rendering. MenuScene provides a simple UI for starting or exiting the game; TransitionScene handles fade in/out transitions between scenes. A SceneManager maintains a stack of scenes and handles transitions.

Input system – The InputManager converts mouse clicks into world space rays and handles selection and aiming. A right‑click short press sets the firing direction; a long press switches to orbit camera mode, letting the player rotate the camera around the selected Node without affecting aim.

Camera system – The camera supports multiple modes: free (WASD + mouse), locked (smoothly moves to behind a selected Node) and orbit (right‑button controlled around a Node). Camera movement and orientation uses quaternion transforms.

Materials, models and resources

Resource management – A ResourceManager caches models and textures. Paths use the asset/models and asset/textures directories. Models are loaded via Assimp and converted into vertex buffers; textures are loaded into shader resource views.

Models and materials – Mesh data is stored in Model objects; materials store shaders, textures and constant buffers. The renderer binds material data when rendering and uses default textures if none are provided. A default white texture is created at initialisation for untextured objects.

Visual effects

Health‑driven transparency – Each Node’s alpha is proportional to its power relative to its maximum. Node health is clamped to [0, 1] and mapped to an alpha range: full power yields near‑opacity, low power yields translucency. The renderer uses this alpha when computing per‑instance constant data.

Billboard explosions – When a projectile is destroyed (by hitting a wall or timing out), an ExplosionEffect entity plays a billboard animation using a sprite sheet and automatically despawns after the animation finishes.

Trails – Projectiles maintain a history of positions and render a fading trail behind them. Trails are implemented as line strips or ribbon billboards with colours fading towards the tail.

Directory structure
Dx3dKadai/
├── asset/                # Models and textures (e.g., cube.fbx, cylinder.fbx, explosion.png)
├── src/
│   ├── core/
│   │   ├── gfx/          # Rendering classes: Renderer, Camera, ShaderProgram
│   │   ├── physics/      # RigidBody, Collider, PhysicsWorld
│   │   └── resource/     # ResourceManager for models/textures
│   ├── game/
│   │   ├── entity/       # Entity base and concrete entities: NodeEntity, BlockEntity, BulletEntity
│   │   ├── runtime/      # Scene base class, WorldContext, SceneManager
│   │   ├── scene/        # BattleScene, MenuScene, TransitionScene
│   │   ├── input/        # InputManager for raycasts and mouse/keyboard handling
│   │   └── ui/           # UI elements (if any)
│   └── shaders/          # HLSL shader files (instanced and non‑instanced)
└── README.md             # Project description (this file)

Build and run

Dependencies – The project targets Windows (x64) with DirectX 11. Required dependencies include:

Direct3D 11 SDK (comes with Windows SDK),

Assimp
 for model importing,

SFML
 for windowing and input (used in early prototypes; replaced by a custom input manager in scenes),

C++17 compiler (e.g., MSVC or clang/LLVM).

Build – Clone the repository and open the provided CMake project or Visual Studio solution. Ensure dependencies are installed and configured. Build in either Debug or Release mode.

Run – Executing the built executable launches the menu scene. Click “Start Game” to begin the battle scene, select a Node, aim with the mouse and fire to capture other Nodes. Use the right mouse button for aiming/orbiting, and the WASD keys in free camera mode.

Contributing

Pull requests are welcome! When contributing, please ensure that your changes are well documented and tested. Discussions can take place in issues or through GitHub discussions.
