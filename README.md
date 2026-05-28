# StalkerOnline

StalkerOnline is a custom online survival game project inspired by S.T.A.L.K.E.R., Survarium, War Inc. and classic post-apocalyptic multiplayer games.

The project is a custom client-server game architecture with an authoritative C# server, a shared network/protocol library, and a native C++ DirectX 11 client.

This is not an Unreal Engine project.

## Current Status

Status date: 2026-05-28

The project has moved past the first network prototype. The server now has basic auth, persistence, character loading, inventory, loot, world objects, movement, interest management, and autosave. The C++ client can connect, login/register, receive player/world state, move the player, pick up/drop items, and render a first visual world prototype.

Current focus:

1. Build a real level workflow through `StalkerOnline.LevelEditor`.
2. Load/edit terrain heightmaps from `.raw` files.
3. Integrate SpeedTree vegetation into the game client and level editor.
4. Turn edited terrain data into playable client/server world data.
5. Continue building gameplay loops: inventory movement, survival stats, combat, loot and world interaction.

## Repository Layout

```txt
StalkerOnline
|-- StalkerOnline.Server      - C# authoritative game server
|-- StalkerOnline.Shared      - shared packets, serializers and game models
|-- StalkerOnline.GameClient  - C++ DirectX 11 game client
|-- server.json               - server runtime configuration
|-- database.json             - MariaDB/MySQL connection configuration
|-- @Server.bat               - quick server launcher
|-- @Client.bat               - quick client launcher
`-- @LevelEditor.bat          - quick level editor launcher
```

## Implemented

### Network Core

- Length-prefixed packet protocol
- `PacketType`
- `PacketReader` / `PacketWriter`
- `PacketProtocol`
- Server-side `ClientSession`
- Packet send/receive logic
- Disconnect handling
- Protection against oversized packets
- Unknown packet validation
- Ping / pong
- Client timeout
- Packet rate limiting

Implemented base packets:

- `LoginRequest`
- `LoginResponse`
- `RegisterRequest`
- `RegisterResponse`
- `Ping`
- `Pong`
- `Disconnect`
- `ServerMessage`
- `ErrorMessage`

### Account / Auth

- `AccountService`
- `AccountRepository`
- `AccountModel`
- Register request/response handling
- Login request/response handling
- Password hashing
- Login and email uniqueness checks
- Ban flag
- Admin flag
- Last login timestamp

### Database / Persistence

- `DatabaseConfig`
- `DatabaseConnectionFactory`
- `DatabaseInitializer`
- Migration table
- Accounts table
- Characters table
- Item templates table
- Inventories table
- Inventory items table
- Starter item templates
- Character save/load
- Inventory save/load
- Auto-save for online players
- Save on server stop / disconnect

Current database target: MariaDB/MySQL through `MySqlConnector`.

### Character System

- `PlayerState`
- Account-linked character loading
- Default character creation
- Nickname from login
- Position and rotation persistence
- Health/stamina/hunger/thirst/radiation/toxicity fields
- Alive/dead flag

### World Server

- `GameWorld`
- `WorldObject`
- `WorldPlayer`
- `WorldItem`
- World object ids
- Player add/remove
- World item spawn/despawn
- Test loot spawn on server start
- Nearby world object queries
- Server-side player movement application

### Interest Management

- `InterestManager`
- Visible player tracking per session
- Visible world item tracking per session
- Player spawn/despawn packets
- World item spawn/despawn packets
- Position broadcast to nearby observers
- Loot pickup/drop visibility updates

### Movement

- `MoveRequest`
- Server-authoritative movement update
- Direction normalization
- Delta time sanitizing
- Rotation update
- Position update packet
- Nearby player broadcast packet
- Client-side WASD movement
- Q/E rotation
- Right mouse look
- First-person / third-person camera toggle

### Inventory / Loot

- `PlayerInventory`
- `InventorySnapshot`
- Inventory item model and repository
- Item template model and repository
- Starter inventory
- Weight calculation
- Pickup item request/response
- Drop item request/response
- World item visibility
- Basic inventory UI in the client

### C++ Game Client

- Win32 window
- DirectX 11 renderer
- Depth buffer and resize handling
- Dear ImGui UI
- Login/register screen
- Server status check
- Remember-login file
- Network client
- Packet serialization/deserialization
- Receive loop
- Player state display
- Inventory display
- Visible world item list
- Movement controls
- Pickup/drop controls
- DirectX world renderer

### Visual World Prototype

- Procedural terrain surface
- Dirt road and cross road
- Ruin blocks
- Fence segments
- Dead trees
- Crates/debris
- Anomaly marker
- Sector markers
- Sky gradient and sun
- Local player character made from simple 3D primitives
- Remote player spawn/despawn/position tracking in the C++ client
- Remote player rendering
- World item rendering as small loot crates

### Level Editor Prototype

- Standalone `StalkerOnline.LevelEditor.exe`
- Shared Win32 / DirectX 11 / Dear ImGui base
- RAW heightmap loading
- RAW heightmap saving
- Supported RAW formats:
  - UE5 `.r16` / RAW16 unsigned little-endian
  - 8-bit unsigned
  - 16-bit unsigned little-endian
  - 32-bit float little-endian
- Auto-detect square terrain size from file size
- Manual width/height for generic `.raw` files because `.raw` has no metadata
- Flat heightmap creation
- Perspective terrain viewport
- Free fly camera
- Separate terrain `Scale X` and `Scale Y`
- UE-style `Z scale` input
- Derived full height range from `UE Z scale * 512`
- Terrain color preview by height
- Wireframe preview
- Brush cursor
- Brush modes:
  - Raise
  - Lower
  - Smooth
  - Flatten
- RMB mouse look
- WASD camera movement
- Q/E vertical camera movement
- Shift camera speed boost
- Mouse wheel camera speed adjustment

Example `.r16` size:

- 8192 KiB `.r16` = 2048 x 2048 heightmap at 16 bits per sample

### SpeedTree Integration

- SpeedTree SDK folder support at `StalkerOnline.GameClient/third_party/SpeedTreeSDK`
- CMake detection for SpeedTree SDK 7.x headers/sources
- SpeedTree Core and Forest static targets built from SDK source when the SDK is present
- Shared `SpeedTreeIntegration` module linked into both game client and level editor
- SpeedTree SDK status/version surfaced in the client/editor
- Level editor SpeedTree panel for loading `.srt` assets and reading model bounds
- Editor-side SpeedTree coordinate setup uses right-handed Y-up to match the current DirectX terrain/editor space
- SDK DirectX 11 renderer sources build and link by default when the SDK is present
- `STALKERONLINE_BUILD_SPEEDTREE_DX11=OFF` can be used to skip the SDK renderer target while keeping Core/Forest available

## Packet Groups

```txt
10-99    Core/session packets
100-199  Auth packets
200-299  Player state snapshots
300-399  Movement packets
400-499  Player world spawn/despawn packets
500-599  World item packets
600-699  Inventory and item interaction packets
900      Disconnect
```

## In Progress

### Level / World Editing

The game client scene is still not a real authored level. The new direction is to build levels in `StalkerOnline.LevelEditor`, then load/export them into client and server runtime world data.

Next work:

- Add a proper `.sollevel` metadata format next to `.raw`
- Keep `.r16` as terrain height data for UE5-style import/export
- Store terrain scale, height scale and spawn points
- Add object placement tools
- Add SpeedTree `.srt` placement tools
- Add transform gizmos
- Add layers/material painting
- Add vegetation layers / density painting
- Add terrain collision export
- Add server-side level loading
- Add client-side level loading instead of procedural debug terrain

### World / Player Feel

The world is still a prototype. It is now visible and readable in the client, but it is not yet a real authored zone.

Next work:

- Add a server-side map/zone model
- Add spawn points
- Add terrain collision / walkable areas
- Add world bounds
- Add real world object templates
- Add object categories: cover, loot, anomaly, safe zone, radiation zone
- Add better player controller smoothing
- Add camera collision
- Replace primitive player model with imported or generated assets later

### Inventory

- Implement inventory move request/response
- Slot validation
- Stack splitting
- Stack merging
- Container inventory
- Equipment slots

### Survival

- Tick hunger/thirst/stamina
- Regeneration rules
- Radiation/toxicity accumulation
- Damage over time
- Death/respawn loop

### Combat

- Weapon templates
- Ammo templates
- Fire request
- Server validation
- Hit detection
- Damage calculation

## Planned Systems

- Equipment
- Weapons
- Damage / death / respawn
- NPCs and mutants
- AI
- Loot spawners and loot tables
- Quests
- Traders and economy
- Crafting
- Chat
- Party/squad/clan
- Raid/extraction mode
- Persistent open world events
- Admin tools
- Logging/monitoring
- Launcher/update system

## Build

Build the C# server/shared projects:

```powershell
dotnet build StalkerOnline.sln
```

Build the C++ client:

```powershell
cmake --build StalkerOnline.GameClient\build --config Debug
```

Build only the level editor:

```powershell
cmake --build StalkerOnline.GameClient\build --config Debug --target StalkerOnline.LevelEditor
```

Disable the SpeedTree DirectX 11 renderer source target if you only need Core/Forest:

```powershell
cmake -S StalkerOnline.GameClient -B StalkerOnline.GameClient\build -DSTALKERONLINE_BUILD_SPEEDTREE_DX11=OFF
```

The current verified state builds cleanly with:

- `dotnet build StalkerOnline.sln`
- `cmake --build StalkerOnline.GameClient\build --config Debug`
- `cmake --build StalkerOnline.GameClient\build --config Debug --target StalkerOnline.LevelEditor`

## Run

Start the server:

```powershell
.\@Server.bat
```

Start the client:

```powershell
.\@Client.bat
```

Start the level editor:

```powershell
.\@LevelEditor.bat
```

Make sure `server.json` and `database.json` are configured for your local or remote server/database before running.

## Immediate Roadmap

1. Add `.sollevel` metadata beside `.raw` heightmaps.
2. Add object placement in the level editor.
3. Add SpeedTree placement and rendering in editor viewport.
4. Add spawn point editing.
5. Load authored terrain in the game client.
6. Load authored terrain/world data on the server.
7. Implement inventory move and stack logic.
8. Add survival stat ticking.
9. Start basic combat.

## License

No license has been selected yet.
