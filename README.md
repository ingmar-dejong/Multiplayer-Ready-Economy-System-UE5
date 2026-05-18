# Multiplayer-Ready Economy System in Unreal Engine 5 C++

This repository is a curated portfolio showcase of a gameplay economy slice built in Unreal Engine 5 C++.

It focuses on a data-driven economy loop with inventory-backed items, world processing stations, UI flows, and server-authoritative multiplayer behavior. The original implementation was developed inside a larger simulation project; this repository isolates the economy-related code that is most relevant from a gameplay systems and engineering perspective.

## What This Showcase Covers

- Economy orchestration through a central gameplay component
- Data-driven item state, quality, processing type, and progression rules
- Harvest, processing, packaging, selling, and upkeep flows
- Interactive processing stations with staged requests and world interaction
- UI parents for economy overview and station-specific interaction
- Multiplayer-safe station ownership, server-side validation, and cleanup paths

## Key Technical Highlights

### 1. Data-Driven Economy Flow

The system is built around gameplay data rather than hardcoded one-off item logic.

- `BloodTypes.h` defines the core economy enums and shared structs
- `BloodProductItem` carries item-specific runtime state such as source, quality, processing state, quantity, and metadata
- recipe and buyer behavior are driven through dedicated data asset types

Relevant files:

- [BloodTypes.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/BloodTypes.h)
- [BloodProductItem.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/Items/BloodProductItem.h)
- [BloodProductItem.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/Items/BloodProductItem.cpp)
- [BloodHarvestSourceDataAsset.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodHarvestSourceDataAsset.h)
- [BloodBuyerDataAsset.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodBuyerDataAsset.h)
- [BloodProcessingRecipeDataAsset.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/Data/BloodProcessingRecipeDataAsset.h)

### 2. Central Economy Orchestration

`UVampireEconomyComponent` coordinates the economy-facing gameplay loop:

- create harvested inventory items
- sell items to buyers
- process items using recipes
- track upkeep and simulation-day state
- route interaction feedback back to UI
- bridge local player intent to server-authoritative station actions

Relevant files:

- [VampireEconomyComponent.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/Economy/VampireEconomyComponent.h)
- [VampireEconomyComponent.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/Economy/VampireEconomyComponent.cpp)

### 3. Interactive Processing Stations

The economy is not only menu-driven. Processing is represented as world gameplay through station actors.

The station layer includes:

- recipe-driven validation
- staged start requests
- timing and progress state
- harvest / completion handling
- packaging-specific subclass behavior
- attachment and station unlock support
- manual interaction hooks for physical placement flows

Relevant files:

- [BloodProcessingStation.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/World/BloodProcessingStation.h)
- [BloodProcessingStation.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/World/BloodProcessingStation.cpp)
- [BloodPackagingStation.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/Vampire/World/BloodPackagingStation.h)
- [BloodPackagingStation.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/World/BloodPackagingStation.cpp)

### 4. Multiplayer-Ready Interaction Architecture

One of the strongest engineering aspects of this slice is the move from local prototype logic toward server-authoritative gameplay flow.

Examples in this code:

- processing station menu opening routed through server to owning client
- station claim / release behavior
- server-confirmed processing and placement actions
- abandoned manual-flow cleanup
- stale operator cleanup when interaction ownership breaks
- support for replicated station runtime state and replicated world interaction results

Relevant files:

- [BloodProcessingInteractableComponent.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/Interaction/BloodProcessingInteractableComponent.cpp)
- [VampireEconomyComponent.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/Economy/VampireEconomyComponent.cpp)
- [BloodProcessingStation.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/World/BloodProcessingStation.cpp)
- [BloodPackagingStation.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/Vampire/World/BloodPackagingStation.cpp)

### 5. UI as a Gameplay Client, Not the Authority

The UI layer is included because it shows how gameplay state is surfaced without making the widget the real owner of the outcome.

- `UVampireEconomySummaryMenu` acts as an overview and feedback layer
- `UVampireBarrelMenu` drives station interaction against gameplay state
- row widgets support the station batch selection flow

Relevant files:

- [VampireEconomySummaryMenu.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/UI/VampireEconomySummaryMenu.h)
- [VampireEconomySummaryMenu.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/UI/VampireEconomySummaryMenu.cpp)
- [VampireBarrelMenu.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/UI/VampireBarrelMenu.h)
- [VampireBarrelMenu.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/UI/VampireBarrelMenu.cpp)
- [VampireBloodBatchRowWidget.h](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Public/UI/VampireBloodBatchRowWidget.h)
- [VampireBloodBatchRowWidget.cpp](/mnt/c/Users/Ingmar/Desktop/Portfolio%20GitHub/Multiplayer%20Economy%20System/Source/MultiplayerEconomySystem/Private/UI/VampireBloodBatchRowWidget.cpp)

## Repository Scope

This is intentionally not the full original game project.

Included:

- selected C++ source files relevant to the economy and processing systems
- the main gameplay architecture for inventory, stations, interaction, and UI

Not included:

- full Unreal project setup
- content assets, maps, blueprints, and packaged builds
- third-party plugin source
- complete buildable project scaffolding

## Dependencies and Omitted Pieces

The original code depends on Unreal Engine modules and project integrations that are not republished here, including:

- `OwnSystemCore`
- `OwnSystemSave`
- `OwnSystemUI`
- project-specific assets and blueprint wiring

This means the repository is best read as a code showcase rather than a drop-in standalone sample.

## Why This Work Matters

From a portfolio perspective, the interesting part is not just the theme. The value is in the systems work:

- a vertical gameplay slice instead of isolated snippets
- pragmatic migration from prototype logic toward stronger architecture
- data-driven economy rules instead of one-off scripting
- multiplayer-aware gameplay decisions at the system boundary
- world interaction, UI, and inventory all connected through gameplay state
