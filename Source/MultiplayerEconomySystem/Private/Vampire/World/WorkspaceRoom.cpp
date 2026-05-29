#include "Vampire/World/WorkspaceRoom.h"

#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Vampire/World/BloodProcessingStation.h"

#define LOCTEXT_NAMESPACE "WorkspaceRoom"

AWorkspaceRoom::AWorkspaceRoom()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AWorkspaceRoom::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		RegisterLevelAuthoredStations();
	}
}

void AWorkspaceRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWorkspaceRoom, PlacedStations);
}

void AWorkspaceRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ShouldDrawWorkspaceDebug())
	{
		DrawWorkspaceDebug();
	}
}

bool AWorkspaceRoom::ShouldTickIfViewportsOnly() const
{
	return true;
}

bool AWorkspaceRoom::IsCellWithinBounds(FIntPoint Cell) const
{
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.X < GridWidth && Cell.Y < GridHeight;
}

FVector AWorkspaceRoom::GetCellWorldLocation(FIntPoint Cell) const
{
	return GetActorLocation() + FVector(static_cast<float>(Cell.X) * CellSize, static_cast<float>(Cell.Y) * CellSize, 0.0f);
}

FRotator AWorkspaceRoom::GetPlacementWorldRotation(int32 RotationQuarterTurns) const
{
	const int32 NormalizedTurns = ((RotationQuarterTurns % 4) + 4) % 4;
	return GetActorRotation() + FRotator(0.0f, static_cast<float>(NormalizedTurns) * 90.0f, 0.0f);
}

FVector AWorkspaceRoom::GetGridWorldSize() const
{
	return FVector(
		static_cast<float>(GridWidth) * CellSize,
		static_cast<float>(GridHeight) * CellSize,
		0.0f);
}

FVector AWorkspaceRoom::GetGridWorldCenter() const
{
	return GetActorTransform().TransformPosition(FVector(
		(static_cast<float>(GridWidth) * CellSize) * 0.5f,
		(static_cast<float>(GridHeight) * CellSize) * 0.5f,
		0.0f));
}

bool AWorkspaceRoom::ValidatePlacement(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns, FText& OutReason, const FGuid* IgnoredStationInstanceId) const
{
	if (!StationDefinition)
	{
		OutReason = LOCTEXT("PlacementMissingDefinition", "Geen stationdefinitie beschikbaar voor deze plaatsing.");
		return false;
	}

	const TArray<FIntPoint> OccupiedCells = BuildOccupiedCells(StationDefinition, AnchorCell, RotationQuarterTurns);
	if (OccupiedCells.IsEmpty())
	{
		OutReason = LOCTEXT("PlacementMissingFootprint", "Dit station heeft geen geldige footprint-cellen.");
		return false;
	}

	for (const FIntPoint& Cell : OccupiedCells)
	{
		if (!IsCellWithinBounds(Cell))
		{
			OutReason = LOCTEXT("PlacementOutOfBounds", "Dit station past niet volledig binnen de werkruimte.");
			return false;
		}

		if (IsDoorCell(Cell))
		{
			OutReason = LOCTEXT("PlacementBlockedByDoor", "Een station mag geen deurcel bezetten.");
			return false;
		}

		if (IsCellOccupied(Cell, IgnoredStationInstanceId))
		{
			OutReason = LOCTEXT("PlacementCellOccupied", "Dit station overlapt met een bestaand geplaatst object.");
			return false;
		}
	}

	if (StationDefinition->bRequiresFullFrontClearance)
	{
		for (const FIntPoint& Cell : BuildFrontAccessCells(StationDefinition, AnchorCell, RotationQuarterTurns))
		{
			if (!IsCellWithinBounds(Cell) || IsDoorCell(Cell) || IsCellOccupied(Cell, IgnoredStationInstanceId))
			{
				OutReason = LOCTEXT("PlacementBlockedFrontAccess", "De volledige werkzijde van dit station moet vrij blijven.");
				return false;
			}
		}
	}

	if (StationDefinition->bRequiresAnyAdjacentClearCell)
	{
		bool bFoundClearAdjacentCell = false;
		for (const FIntPoint& Cell : BuildAdjacentAccessCells(StationDefinition, AnchorCell, RotationQuarterTurns))
		{
			if (IsCellWithinBounds(Cell) && !IsDoorCell(Cell) && !IsCellOccupied(Cell, IgnoredStationInstanceId))
			{
				bFoundClearAdjacentCell = true;
				break;
			}
		}

		if (!bFoundClearAdjacentCell)
		{
			OutReason = LOCTEXT("PlacementMissingAdjacentAccess", "Dit object heeft minstens een vrije aangrenzende cel nodig.");
			return false;
		}
	}

	return true;
}

bool AWorkspaceRoom::AddPlacementRecord(const FWorkspacePlacedStationRecord& Record, FText& OutReason)
{
	if (!HasAuthority())
	{
		OutReason = LOCTEXT("PlacementServerOnly", "Plaatsingsrecords mogen alleen server-side worden toegevoegd.");
		return false;
	}

	if (!Record.StationDefinition)
	{
		OutReason = LOCTEXT("PlacementRecordMissingDefinition", "Kan geen plaatsingsrecord toevoegen zonder stationdefinitie.");
		return false;
	}

	if (!Record.StationInstanceId.IsValid())
	{
		OutReason = LOCTEXT("PlacementRecordMissingGuid", "Kan geen plaatsingsrecord toevoegen zonder stationinstance-id.");
		return false;
	}

	if (FindPlacementRecordByInstanceId(Record.StationInstanceId))
	{
		OutReason = LOCTEXT("PlacementRecordDuplicateGuid", "Dit station-instance-id bestaat al in deze werkruimte. Geef nieuwe placeable station-items altijd een unieke instance-id.");
		return false;
	}

	if (!ValidatePlacement(Record.StationDefinition, Record.AnchorCell, Record.RotationQuarterTurns, OutReason))
	{
		return false;
	}

	PlacedStations.Add(Record);
	ForceNetUpdate();
	return true;
}

bool AWorkspaceRoom::RemovePlacementRecordByInstanceId(const FGuid& StationInstanceId)
{
	if (!HasAuthority() || !StationInstanceId.IsValid())
	{
		return false;
	}

	const int32 RemovedCount = PlacedStations.RemoveAll([&StationInstanceId](const FWorkspacePlacedStationRecord& Record)
	{
		return Record.StationInstanceId == StationInstanceId;
	});

	if (RemovedCount > 0)
	{
		ForceNetUpdate();
		return true;
	}

	return false;
}

const FWorkspacePlacedStationRecord* AWorkspaceRoom::FindPlacementRecordByInstanceId(const FGuid& StationInstanceId) const
{
	if (!StationInstanceId.IsValid())
	{
		return nullptr;
	}

	return PlacedStations.FindByPredicate([&StationInstanceId](const FWorkspacePlacedStationRecord& Record)
	{
		return Record.StationInstanceId == StationInstanceId;
	});
}

bool AWorkspaceRoom::RegisterPlacedStationActor(ABloodProcessingStation* ProcessingStation, const FWorkspacePlacedStationRecord& Record, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!HasAuthority())
	{
		OutReason = LOCTEXT("RegisterPlacedActorServerOnly", "Placed stationregistratie mag alleen server-side gebeuren.");
		return false;
	}

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("RegisterPlacedActorMissingStation", "Geen geldig processing station beschikbaar voor registratie.");
		return false;
	}

	if (!Record.StationDefinition)
	{
		OutReason = LOCTEXT("RegisterPlacedActorMissingDefinition", "Placed stationregistratie mist een stationdefinitie.");
		return false;
	}

	if (!Record.StationInstanceId.IsValid())
	{
		OutReason = LOCTEXT("RegisterPlacedActorMissingGuid", "Placed stationregistratie mist een geldige station-instance-id.");
		return false;
	}

	const FWorkspacePlacedStationRecord* AppliedRecord = FindPlacementRecordByInstanceId(Record.StationInstanceId);
	if (!AppliedRecord)
	{
		if (!AddPlacementRecord(Record, OutReason))
		{
			return false;
		}

		AppliedRecord = FindPlacementRecordByInstanceId(Record.StationInstanceId);
	}

	if (!AppliedRecord)
	{
		OutReason = LOCTEXT("RegisterPlacedActorRecordLookupFailed", "Placed stationrecord kon na registratie niet teruggevonden worden.");
		return false;
	}

	ProcessingStation->SetPlacementContext(this, AppliedRecord->StationInstanceId, RoomId, AppliedRecord->AnchorCell, AppliedRecord->RotationQuarterTurns);
	return true;
}

AActor* AWorkspaceRoom::SpawnPlacedActor(const FWorkspacePlacedStationRecord& Record, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!HasAuthority())
	{
		OutReason = LOCTEXT("SpawnPlacedActorServerOnly", "Placed actors mogen alleen server-side worden gespawned.");
		return nullptr;
	}

	if (!Record.StationDefinition || !Record.StationDefinition->PlacedActorClass)
	{
		OutReason = LOCTEXT("SpawnPlacedActorMissingClass", "Geen geldige actor class gekoppeld aan deze stationdefinitie.");
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(
		Record.StationDefinition->PlacedActorClass,
		GetCellWorldLocation(Record.AnchorCell),
		GetPlacementWorldRotation(Record.RotationQuarterTurns),
		SpawnParameters);

	if (!SpawnedActor)
	{
		OutReason = LOCTEXT("SpawnPlacedActorFailed", "Kon geen placed station actor spawnen.");
		return nullptr;
	}

	if (ABloodProcessingStation* ProcessingStation = Cast<ABloodProcessingStation>(SpawnedActor))
	{
		if (!RegisterPlacedStationActor(ProcessingStation, Record, OutReason))
		{
			SpawnedActor->Destroy();
			return nullptr;
		}
	}

	return SpawnedActor;
}

bool AWorkspaceRoom::DestroyPlacedActorByInstanceId(const FGuid& StationInstanceId)
{
	if (!HasAuthority() || !StationInstanceId.IsValid() || !GetWorld())
	{
		return false;
	}

	for (TActorIterator<ABloodProcessingStation> It(GetWorld()); It; ++It)
	{
		ABloodProcessingStation* ProcessingStation = *It;
		if (!ProcessingStation || ProcessingStation->GetOwningWorkspaceRoom() != this)
		{
			continue;
		}

		if (ProcessingStation->GetPlacedStationInstanceId() != StationInstanceId)
		{
			continue;
		}

		ProcessingStation->Destroy();
		return true;
	}

	return false;
}

bool AWorkspaceRoom::ShouldDrawWorkspaceDebug() const
{
	if (!bShowWorkspaceDebug || !GetWorld())
	{
		return false;
	}

	const bool bGameWorld = GetWorld()->IsGameWorld();
	return bGameWorld ? bShowWorkspaceDebugInGame : true;
}

void AWorkspaceRoom::DrawWorkspaceDebug() const
{
	if (!GetWorld())
	{
		return;
	}

	const FTransform RoomTransform = GetActorTransform();
	const FVector RoomOrigin = RoomTransform.GetLocation();
	const FVector UpOffset = RoomTransform.GetUnitAxis(EAxis::Z) * DebugDrawHeight;
	const FVector LabelOffset = RoomTransform.GetUnitAxis(EAxis::Z) * DebugLabelHeight;
	const FVector XAxis = RoomTransform.GetUnitAxis(EAxis::X);
	const FVector YAxis = RoomTransform.GetUnitAxis(EAxis::Y);
	const float DebugLifetime = 0.0f;
	const float LineThickness = 1.5f;

	for (int32 X = 0; X <= GridWidth; ++X)
	{
		const FVector LineStart = RoomOrigin + UpOffset + (XAxis * (static_cast<float>(X) * CellSize));
		const FVector LineEnd = LineStart + (YAxis * (static_cast<float>(GridHeight) * CellSize));
		DrawDebugLine(GetWorld(), LineStart, LineEnd, GridLineColor, false, DebugLifetime, 0, LineThickness);
	}

	for (int32 Y = 0; Y <= GridHeight; ++Y)
	{
		const FVector LineStart = RoomOrigin + UpOffset + (YAxis * (static_cast<float>(Y) * CellSize));
		const FVector LineEnd = LineStart + (XAxis * (static_cast<float>(GridWidth) * CellSize));
		DrawDebugLine(GetWorld(), LineStart, LineEnd, GridLineColor, false, DebugLifetime, 0, LineThickness);
	}

	const FVector HalfExtent(
		(static_cast<float>(GridWidth) * CellSize) * 0.5f,
		(static_cast<float>(GridHeight) * CellSize) * 0.5f,
		2.0f);
	DrawDebugBox(
		GetWorld(),
		GetGridWorldCenter() + UpOffset,
		HalfExtent,
		RoomTransform.GetRotation(),
		BoundsColor,
		false,
		DebugLifetime,
		0,
		2.5f);

	for (const FIntPoint& DoorCell : DoorCells)
	{
		const FVector CellCenter = GetCellWorldLocation(DoorCell) + (XAxis + YAxis) * (CellSize * 0.5f) + UpOffset;
		DrawDebugBox(
			GetWorld(),
			CellCenter,
			FVector(CellSize * 0.5f, CellSize * 0.5f, 3.0f),
			RoomTransform.GetRotation(),
			DoorCellColor,
			false,
			DebugLifetime,
			0,
			2.0f);
	}

	for (const FWorkspacePlacedStationRecord& Record : PlacedStations)
	{
		if (!Record.StationDefinition)
		{
			continue;
		}

		for (const FIntPoint& OccupiedCell : BuildOccupiedCells(Record.StationDefinition, Record.AnchorCell, Record.RotationQuarterTurns))
		{
			const FVector CellCenter = GetCellWorldLocation(OccupiedCell) + (XAxis + YAxis) * (CellSize * 0.5f) + UpOffset;
			DrawDebugBox(
				GetWorld(),
				CellCenter,
				FVector(CellSize * 0.45f, CellSize * 0.45f, 2.0f),
				RoomTransform.GetRotation(),
				OccupiedCellColor,
				false,
				DebugLifetime,
				0,
				1.25f);
		}
	}

	const FString LabelText = FString::Printf(
		TEXT("Workspace: %s\nGrid: %dx%d\nCellSize: %.0f\nPlaced: %d"),
		RoomId.IsNone() ? TEXT("<No RoomId>") : *RoomId.ToString(),
		GridWidth,
		GridHeight,
		CellSize,
		PlacedStations.Num());
	DrawDebugString(
		GetWorld(),
		GetGridWorldCenter() + LabelOffset,
		LabelText,
		nullptr,
		FColor::White,
		0.0f,
		false,
		1.1f);
}

void AWorkspaceRoom::RegisterLevelAuthoredStations()
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ABloodProcessingStation> It(GetWorld()); It; ++It)
	{
		ABloodProcessingStation* ProcessingStation = *It;
		if (!ProcessingStation
			|| !ProcessingStation->bRegisterAsPlacedStationOnBeginPlay
			|| ProcessingStation->HasRegisteredPlacementContext())
		{
			continue;
		}

		if (ProcessingStation->WorkspaceRoomOverride && ProcessingStation->WorkspaceRoomOverride != this)
		{
			continue;
		}

		FWorkspacePlacedStationRecord PlacementRecord;
		FText RegistrationReason;
		if (!ProcessingStation->TryBuildPlacementRecordForWorkspace(this, PlacementRecord, RegistrationReason))
		{
			continue;
		}

		if (!RegisterPlacedStationActor(ProcessingStation, PlacementRecord, RegistrationReason))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("WorkspaceRoom: Failed to register level-authored station room=%s station=%s reason=%s"),
				*GetNameSafe(this),
				*GetNameSafe(ProcessingStation),
				*RegistrationReason.ToString());
		}
	}
}

FIntPoint AWorkspaceRoom::RotateLocalCell(FIntPoint LocalCell, int32 RotationQuarterTurns)
{
	const int32 NormalizedTurns = ((RotationQuarterTurns % 4) + 4) % 4;

	switch (NormalizedTurns)
	{
	case 1:
		return FIntPoint(-LocalCell.Y, LocalCell.X);
	case 2:
		return FIntPoint(-LocalCell.X, -LocalCell.Y);
	case 3:
		return FIntPoint(LocalCell.Y, -LocalCell.X);
	default:
		return LocalCell;
	}
}

TArray<FIntPoint> AWorkspaceRoom::BuildOccupiedCells(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns)
{
	TArray<FIntPoint> Result;
	if (!StationDefinition)
	{
		return Result;
	}

	Result.Reserve(StationDefinition->FootprintCells.Num());
	for (const FIntPoint& LocalCell : StationDefinition->FootprintCells)
	{
		Result.Add(AnchorCell + RotateLocalCell(LocalCell, RotationQuarterTurns));
	}

	return Result;
}

TArray<FIntPoint> AWorkspaceRoom::BuildFrontAccessCells(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns)
{
	TArray<FIntPoint> Result;
	if (!StationDefinition || StationDefinition->FootprintCells.IsEmpty())
	{
		return Result;
	}

	int32 MaxLocalY = TNumericLimits<int32>::Lowest();
	for (const FIntPoint& LocalCell : StationDefinition->FootprintCells)
	{
		MaxLocalY = FMath::Max(MaxLocalY, LocalCell.Y);
	}

	for (const FIntPoint& LocalCell : StationDefinition->FootprintCells)
	{
		if (LocalCell.Y == MaxLocalY)
		{
			Result.AddUnique(AnchorCell + RotateLocalCell(FIntPoint(LocalCell.X, LocalCell.Y + 1), RotationQuarterTurns));
		}
	}

	return Result;
}

TArray<FIntPoint> AWorkspaceRoom::BuildAdjacentAccessCells(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns)
{
	TArray<FIntPoint> Result;
	if (!StationDefinition)
	{
		return Result;
	}

	const TArray<FIntPoint> OccupiedCells = BuildOccupiedCells(StationDefinition, AnchorCell, RotationQuarterTurns);
	for (const FIntPoint& Cell : OccupiedCells)
	{
		Result.AddUnique(Cell + FIntPoint(1, 0));
		Result.AddUnique(Cell + FIntPoint(-1, 0));
		Result.AddUnique(Cell + FIntPoint(0, 1));
		Result.AddUnique(Cell + FIntPoint(0, -1));
	}

	for (const FIntPoint& OccupiedCell : OccupiedCells)
	{
		Result.Remove(OccupiedCell);
	}

	return Result;
}

bool AWorkspaceRoom::IsDoorCell(FIntPoint Cell) const
{
	return DoorCells.Contains(Cell);
}

bool AWorkspaceRoom::IsCellOccupied(FIntPoint Cell, const FGuid* IgnoredStationInstanceId) const
{
	for (const FWorkspacePlacedStationRecord& Record : PlacedStations)
	{
		if (IgnoredStationInstanceId && Record.StationInstanceId == *IgnoredStationInstanceId)
		{
			continue;
		}

		for (const FIntPoint& OccupiedCell : BuildOccupiedCells(Record.StationDefinition, Record.AnchorCell, Record.RotationQuarterTurns))
		{
			if (OccupiedCell == Cell)
			{
				return true;
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
