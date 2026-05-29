# Multiplayer Economy, Processing, and Station Placement System

This repository is a curated portfolio showcase of a gameplay systems slice built in Unreal Engine 5 C++.

The original implementation lives inside a larger co-op vampire simulation project. This extract focuses on the code that is most relevant from a gameplay engineering perspective: a data-driven blood economy, inventory-backed products, interactive processing stations, physical packaging interactions, server-authoritative multiplayer flow, and a workspace system for placing and moving stations.

Link to showcase:
https://onedrive.live.com/?id=%2Fpersonal%2F9d9f3cd318aeacbc%2FDocuments%2FPortfolio%2F2%2E%20Slice%20of%20Economy%20Processing%20Station%20%28Co%2Dop%29&viewid=b61252c1%2D4089%2D4b48%2Dba1f%2D36bf4fdf86eb

## What This Showcase Covers

- Data-driven economy items, processing states, quality rules, recipes, and buyer behavior
- Harvest, aging, packaging, selling, daily upkeep, and thrall-support flows
- Processing stations with staged requests, replicated runtime state, and save/load hooks
- Manual station interaction through preview actors, placement targets, and physical object manipulation
- Packaging as a shared co-op world interaction with replicated placed-slot state
- Inventory-driven station placement, workspace records, move locks, and observer previews
- UI that acts as a client of gameplay state instead of owning authoritative outcomes

## Key Technical Highlights

### 1. Data-Driven Economy Flow

The economy is built around gameplay data and item state rather than one-off scripting.

- `BloodTypes.h` defines shared enums and economy structs.
- `UBloodProductItem` stores source, quality, processing type, quantity, creation day, and replicated presentation data.
- Harvest sources, buyers, processing recipes, and station attachments are driven by data assets.
- Blood products now integrate with OwnSystem inventory item lifecycle hooks so loaded or replicated inventory state can rebuild presentation cleanly.

Relevant files:

- [BloodTypes.h](Source/MultiplayerEconomySystem/Public/Vampire/BloodTypes.h)
- [BloodProductItem.h](Source/MultiplayerEconomySystem/Public/Vampire/Items/BloodProductItem.h)
- [BloodProductItem.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Items/BloodProductItem.cpp)
- [BloodHarvestSourceDataAsset.h](Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodHarvestSourceDataAsset.h)
- [BloodBuyerDataAsset.h](Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodBuyerDataAsset.h)
- [BloodProcessingRecipeDataAsset.h](Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodProcessingRecipeDataAsset.h)
- [BloodProcessingAttachmentDataAsset.h](Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodProcessingAttachmentDataAsset.h)

### 2. Central Gameplay Orchestration

`UVampireEconomyComponent` coordinates the economy-facing gameplay loop and the local player's interaction modes.

It handles:

- creating harvested blood items
- selling item groups to buyers
- resolving the owning OwnSystem inventory
- tracking daily upkeep and thrall cost state
- routing server-authoritative station interaction requests
- opening station menus on the owning client
- starting, rotating, confirming, and canceling station placement mode
- moving already placed stations through a multiplayer-safe lock and commit flow

Relevant files:

- [VampireEconomyComponent.h](Source/MultiplayerEconomySystem/Public/Vampire/Economy/VampireEconomyComponent.h)
- [VampireEconomyComponent.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Economy/VampireEconomyComponent.cpp)

### 3. Processing Stations as World Gameplay

Processing is represented by station actors, not only menus. A station owns the runtime processing state and acts as the boundary between UI, inventory, world interaction, and multiplayer authority.

The station layer includes:

- recipe validation and processing duration/progress state
- staged manual processing requests
- replicated active operator and abandoned-flow cleanup
- station camera and prompt suppression while interacting
- save/load integration through OwnSystem savable actor hooks
- level-authored and runtime-spawned placement context
- replicated move-in-progress state and observer preview state

Relevant files:

- [BloodProcessingStation.h](Source/MultiplayerEconomySystem/Public/Vampire/World/BloodProcessingStation.h)
- [BloodProcessingStation.cpp](Source/MultiplayerEconomySystem/Private/Vampire/World/BloodProcessingStation.cpp)
- [BloodProcessingInteractableComponent.h](Source/MultiplayerEconomySystem/Public/Vampire/Interaction/BloodProcessingInteractableComponent.h)
- [BloodProcessingInteractableComponent.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Interaction/BloodProcessingInteractableComponent.cpp)

### 4. Manual Packaging and Physical Interaction

The packaging station is the first manual processing vertical slice. It turns a selected blood batch into handelsklaar product through physical placement steps instead of a single instant button.

The current flow:

- reserve a valid blood batch in station-owned state
- spawn one manual preview item at a configured station spawn point
- let the player manipulate and place that item into one of several valid packaging slots
- confirm the slot on the server
- replicate placed-slot state so observers see the same boxed items
- commit output once the packaging target is complete
- clean up loose preview actors when the manual flow is canceled or abandoned

Relevant files:

- [BloodPackagingStation.h](Source/MultiplayerEconomySystem/Public/Vampire/World/BloodPackagingStation.h)
- [BloodPackagingStation.cpp](Source/MultiplayerEconomySystem/Private/Vampire/World/BloodPackagingStation.cpp)
- [PhysicsManipulationComponent.h](Source/MultiplayerEconomySystem/Public/Vampire/Interaction/PhysicsManipulationComponent.h)
- [PhysicsManipulationComponent.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Interaction/PhysicsManipulationComponent.cpp)
- [ManipulatableObjectComponent.h](Source/MultiplayerEconomySystem/Public/Vampire/Interaction/ManipulatableObjectComponent.h)
- [ManipulatableObjectComponent.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Interaction/ManipulatableObjectComponent.cpp)
- [InteractionPlacementTargetComponent.h](Source/MultiplayerEconomySystem/Public/Vampire/Interaction/InteractionPlacementTargetComponent.h)
- [InteractionPlacementTargetComponent.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Interaction/InteractionPlacementTargetComponent.cpp)

### 5. Workspace Station Placement

The system has grown beyond fixed map stations. Stations can now be represented as inventory items, placed into workspace grid cells, registered as placed station records, and moved later with multiplayer coordination.

This layer includes:

- `UPlaceableStationDataAsset` definitions for footprint, category, preview mesh, and placed actor class
- `UPlaceableStationItem` inventory items with stable station instance ids
- `AWorkspaceRoom` placement validation, occupied-cell tracking, placement records, and actor spawning
- `AStationPlacementPreviewActor` visual feedback for placement validity
- server-side move reservation so only one player can move a station at a time
- replicated observer ghost previews while another player is moving a station

Relevant files:

- [PlaceableStationDataAsset.h](Source/MultiplayerEconomySystem/Public/Vampire/Data/PlaceableStationDataAsset.h)
- [PlaceableStationItem.h](Source/MultiplayerEconomySystem/Public/Vampire/Items/PlaceableStationItem.h)
- [PlaceableStationItem.cpp](Source/MultiplayerEconomySystem/Private/Vampire/Items/PlaceableStationItem.cpp)
- [WorkspaceRoom.h](Source/MultiplayerEconomySystem/Public/Vampire/World/WorkspaceRoom.h)
- [WorkspaceRoom.cpp](Source/MultiplayerEconomySystem/Private/Vampire/World/WorkspaceRoom.cpp)
- [StationPlacementPreviewActor.h](Source/MultiplayerEconomySystem/Public/Vampire/World/StationPlacementPreviewActor.h)
- [StationPlacementPreviewActor.cpp](Source/MultiplayerEconomySystem/Private/Vampire/World/StationPlacementPreviewActor.cpp)

### 6. UI as a Gameplay Client

The UI layer is included because it shows how the player-facing flow talks to gameplay state without becoming the authority.

- `UVampireEconomySummaryMenu` displays economy state and triggers high-level actions.
- `UProcessingStationMenuBase` / `UVampireBarrelMenu` drive station batch selection and manual processing actions against station state.
- UI calls route through the economy component and station APIs so server validation, ownership checks, and cleanup still happen in gameplay code.

Relevant files:

- [VampireEconomySummaryMenu.h](Source/MultiplayerEconomySystem/Public/UI/VampireEconomySummaryMenu.h)
- [VampireEconomySummaryMenu.cpp](Source/MultiplayerEconomySystem/Private/UI/VampireEconomySummaryMenu.cpp)
- [ProcessingStationMenuBase.h](Source/MultiplayerEconomySystem/Public/UI/ProcessingStationMenuBase.h)
- [VampireBarrelMenu.h](Source/MultiplayerEconomySystem/Public/UI/VampireBarrelMenu.h)
- [VampireBarrelMenu.cpp](Source/MultiplayerEconomySystem/Private/UI/VampireBarrelMenu.cpp)
- [VampireBloodBatchRowWidget.h](Source/MultiplayerEconomySystem/Public/UI/VampireBloodBatchRowWidget.h)
- [VampireBloodBatchRowWidget.cpp](Source/MultiplayerEconomySystem/Private/UI/VampireBloodBatchRowWidget.cpp)

## Repository Scope

This is intentionally not the full original game project.

Included:

- selected C++ source files relevant to economy, processing, manual interaction, station placement, and UI
- the main gameplay architecture for inventory-backed products, stations, workspace placement, interaction, replication, and save-facing state

Not included:

- full Unreal project setup
- content assets, maps, blueprints, and packaged builds
- third-party plugin source
- complete buildable project scaffolding

## Dependencies and Omitted Pieces

The original code depends on Unreal Engine modules and project integrations that are not republished here, including:

- OwnSystem inventory, UI, interaction, and save framework classes
- project-specific Blueprint widget classes and station actor Blueprints
- input mappings, meshes, maps, and content assets
- broader game framework code outside this focused systems slice

This means the repository is best read as a code showcase rather than a drop-in standalone sample.

## Why This Work Matters

From a portfolio perspective, the value is in the system boundaries and the migration path:

- a connected vertical gameplay slice instead of isolated snippets
- data-driven rules for economy behavior and station authoring
- physical world interaction layered over backend processing logic
- server-authoritative multiplayer decisions at interaction and placement boundaries
- explicit save-facing state for placed stations and processing actors
- UI, inventory, world actors, and co-op replication all connected through gameplay-owned state
