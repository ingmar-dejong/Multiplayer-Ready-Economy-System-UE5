#pragma once

#include "CoreMinimal.h"
#include "BloodTypes.generated.h"

class UBloodProductItem;

UENUM(BlueprintType)
enum class EBloodVatStationState : uint8
{
	Leeg UMETA(DisplayName = "Leeg"),
	Rijpt UMETA(DisplayName = "Rijpt"),
	Klaar UMETA(DisplayName = "Klaar")
};

UENUM(BlueprintType)
enum class EBloodSourceType : uint8
{
	Rat UMETA(DisplayName = "Rat"),
	Varken UMETA(DisplayName = "Varken"),
	Geit UMETA(DisplayName = "Geit"),
	Hert UMETA(DisplayName = "Hert"),
	Bedelaar UMETA(DisplayName = "Bedelaar"),
	Boer UMETA(DisplayName = "Boer"),
	Handelaar UMETA(DisplayName = "Handelaar"),
	Edelman UMETA(DisplayName = "Edelman"),
	Priester UMETA(DisplayName = "Priester")
};

UENUM(BlueprintType)
enum class EBloodQuality : uint8
{
	Gewoon UMETA(DisplayName = "Gewoon"),
	Goed UMETA(DisplayName = "Goed"),
	Premium UMETA(DisplayName = "Premium")
};

UENUM(BlueprintType)
enum class EBloodProcessingType : uint8
{
	Vers UMETA(DisplayName = "Vers"),
	GerijptOpHout UMETA(DisplayName = "Gerijpt Op Hout"),
	Handelsklaar UMETA(DisplayName = "Handelsklaar"),
	Gekruid UMETA(DisplayName = "Gekruid"),
	Gezuiverd UMETA(DisplayName = "Gezuiverd"),
	Verdund UMETA(DisplayName = "Verdund"),
	RitueelBehandeld UMETA(DisplayName = "Ritueel Behandeld"),
	GekoeldBewaard UMETA(DisplayName = "Gekoeld Bewaard")
};

USTRUCT(BlueprintType)
struct FHarvestedBloodData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blood")
	TSubclassOf<UBloodProductItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blood")
	EBloodSourceType SourceType = EBloodSourceType::Rat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blood")
	EBloodQuality BaseQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blood")
	EBloodProcessingType ProcessingType = EBloodProcessingType::Vers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blood", meta = (ClampMin = 1))
	int32 BloodQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blood", meta = (ClampMin = 0))
	int32 CreatedDay = 0;
};

USTRUCT(BlueprintType)
struct FStoredBloodBatchData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Blood")
	TSubclassOf<UBloodProductItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Blood")
	EBloodSourceType SourceType = EBloodSourceType::Rat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Blood")
	EBloodQuality BaseQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Blood")
	EBloodProcessingType ProcessingType = EBloodProcessingType::Vers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Blood", meta = (ClampMin = 1))
	int32 BloodQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Blood", meta = (ClampMin = 0))
	int32 CreatedDay = 0;
};

UENUM(BlueprintType)
enum class EThrallTier : uint8
{
	ThrallI UMETA(DisplayName = "Thrall I"),
	ThrallII UMETA(DisplayName = "Thrall II")
};

USTRUCT(BlueprintType)
struct FThrallUpkeepUnit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Thrall")
	FName ThrallId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Thrall")
	EThrallTier Tier = EThrallTier::ThrallI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Thrall")
	EBloodQuality RequiredQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Thrall", meta = (ClampMin = 1))
	int32 RequiredBloodUnitsPerDay = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Thrall", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Loyalty = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Thrall")
	bool bActive = true;
};
