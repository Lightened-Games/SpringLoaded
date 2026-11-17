// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Data/OTRTypes.h"
#include "OTRCrowdComponent.generated.h"

class USkeletalMeshComponent;
class UAnimSequence;

/**
 * The fight crowd (review 1: "looks empty, add an audience"). Standing spectators (Microsoft Rocketbox, MIT) placed
 * on every bleacher the arena builds, each on a seeded spot along the rows, facing the ring. They idle on Rocketbox
 * mocap (neutral, look-around, chatting) and react through the event bus only: a clean hit, a snapped rope, a
 * parry or the knockout brings them up cheering; a heavy hit on the player gets the champion's fans going.
 */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRCrowdComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOTRCrowdComponent();

	/** Fill these bleacher sections (their world transforms; local +X faces the ring). */
	void Populate(const TArray<FTransform>& Bleachers);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") FString CrowdRoot = TEXT("/Game/OntheRopes/Crowd");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TArray<FString> Avatars = {
		TEXT("Male_Adult_01"), TEXT("Male_Adult_02"), TEXT("Male_Adult_03"), TEXT("Male_Adult_05"), TEXT("Male_Adult_06"), TEXT("Male_Adult_08"),
		TEXT("Male_Adult_11"), TEXT("Female_Adult_01"), TEXT("Female_Adult_02"), TEXT("Female_Adult_04"), TEXT("Female_Adult_06"), TEXT("Sports_Male_02") };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TArray<FString> IdleClips = {
		TEXT("m_idle_neutral_01"), TEXT("m_idle_look_around_01"), TEXT("m_gestic_talk_neutral_01") };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TArray<FString> CheerClips = { TEXT("m_cheer_01"), TEXT("m_cheer_03") };

	/** Bleacher geometry (matches SM_OTR_Bleacher): rows, row depth / rise, half width, deck top above the row base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") int32 Rows = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float RowDepth = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float RowRise = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float DeckTop = 25.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float StandForward = 24.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float HalfWidth = 370.0f;
	/** Spacing between standing spots along a row, cm; each spot is filled with this chance (front row first). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float Spacing = 78.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float FrontDensity = 0.62f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float BackDensity = 0.32f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") int32 MaxSpectators = 150;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") int32 Seed = 7;
	/** Yaw that turns the Rocketbox mesh (forward +Y) to face local +X; random yaw jitter toward the ring, deg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float MeshYaw = -90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") float YawJitter = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") FVector2D ScaleRange = FVector2D(0.95f, 1.05f);
	/** Only the front row casts shadows (cost). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Layout") int32 ShadowRows = 1;

	/** Reactions: share of the crowd that cheers, for how long, with a random start delay up to ReactSpread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float CleanHitShare = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float SnapShare = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float BigMoveShare = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float PlayerHurtShare = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float PlayerHurtMinDamage = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float CheerSeconds = 3.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float VictoryCheerSeconds = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float ReactSpread = 0.45f;
	/** Volume of the crowd cheer on a parry, interrupt, guard-break or slip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Reactions") float BigMoveCheerVolume = 0.45f;
	/** Press camera flashes from the stands: a small pool of point lights popped at random spectators. Ambient rate
	 *  per second, a burst count on big moments, flash length (s) and peak candela. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flashes") int32 FlashPool = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flashes") float FlashAmbientRate = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flashes") int32 FlashBurst = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flashes") float FlashSeconds = 0.07f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flashes") float FlashCandela = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Flashes") float FlashRadius = 420.0f;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FFan
	{
		TObjectPtr<USkeletalMeshComponent> Mesh;
		int32 Idle = 0;
		float CheerStart = -1.0f;
		float CheerEnd = -1.0f;
		bool bCheering = false;
	};

	void Cheer(float Share, float Seconds);
	void QueueFlashes(int32 Count, float OverSeconds);
	void TickFlashes(float Dt);
	UAnimSequence* LoadClip(const FString& Name) const;
	UFUNCTION() void HandleCleanHit(float Load, FVector Location);
	UFUNCTION() void HandleSnap(int32 Side, FVector Location);
	UFUNCTION() void HandlePopup(FText Text, FVector WorldLocation, FLinearColor Colour);
	UFUNCTION() void HandlePlayerHit(float Damage, bool bGuarded, EOTRPunch Punch, FVector Location);
	UFUNCTION() void HandleMatchState(EOTRMatchState State, EOTRDefeat Reason);

	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Idles;
	UPROPERTY() TArray<TObjectPtr<UAnimSequence>> Cheers;
	UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> Meshes;
	TArray<FFan> Fans;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Flashes;
	TArray<float> FlashLeft;
	TArray<float> PendingFlashAt;
	float Clock = 0.0f;
	FRandomStream Rng;
};
