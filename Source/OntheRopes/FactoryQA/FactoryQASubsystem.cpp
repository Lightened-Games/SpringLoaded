// FactoryQA subsystem (design guide C2 + C3). See header.

#include "FactoryQASubsystem.h"
#include "FactoryGaitProbeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "HAL/PlatformFileManager.h"

namespace FactoryQA
{
	static bool Flag(const TCHAR* Name) { return FParse::Param(FCommandLine::Get(), Name); }
	static bool All() { return Flag(TEXT("FactoryQA")); }
	static FString Esc(const FString& S) { return S.Replace(TEXT("\\"), TEXT("/")).Replace(TEXT("\""), TEXT("'")); }
}

bool UFactoryQASubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	if (!World || !(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		return false;
	}
	using namespace FactoryQA;
	return All() || Flag(TEXT("CollisionAudit")) || Flag(TEXT("PenetrationLog")) || Flag(TEXT("GaitLog")) || Flag(TEXT("GaitShots"));
}

void UFactoryQASubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	using namespace FactoryQA;
	bAudit = All() || Flag(TEXT("CollisionAudit"));
	bPen = All() || Flag(TEXT("PenetrationLog"));
	bShots = All() || Flag(TEXT("GaitShots"));
	bGait = bShots || Flag(TEXT("GaitLog"));
	if (bShots && !FApp::CanEverRender())
	{
		UE_LOG(LogTemp, Warning, TEXT("[FactoryQA] -GaitShots needs rendering: use -RenderOffscreen instead of -nullrhi. Shots disabled."));
		bShots = false;
	}
	// One folder per process, shared by every world the game creates (entry world, game world, restarts).
	static const FString ProcessStamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	RunStamp = ProcessStamp;
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Active: audit=%d penetration=%d gait=%d shots=%d -> %s"), bAudit, bPen, bGait, bShots, *OutDir());
}

void UFactoryQASubsystem::Deinitialize()
{
	if (Elapsed < 1.0f && !bAudited && PenEvents.Num() == 0 && Probes.Num() == 0)
	{
		Super::Deinitialize(); // a transient world (entry map, loading): nothing to report
		return;
	}
	WriteReport();
	for (UFactoryGaitProbeComponent* P : Probes)
	{
		if (IsValid(P)) { P->LogSummary(); }
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Report written to %s"), *OutDir());
	Super::Deinitialize();
}

TStatId UFactoryQASubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFactoryQASubsystem, STATGROUP_Tickables);
}

FString UFactoryQASubsystem::OutDir() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("QA"), TEXT("FactoryQA"), RunStamp));
}

bool UFactoryQASubsystem::IsExempt(const UPrimitiveComponent* C) const
{
	return C->ComponentHasTag(NoCollideTag) || (C->GetOwner() && C->GetOwner()->ActorHasTag(NoCollideTag));
}

float UFactoryQASubsystem::PlayerFeetZ() const
{
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (const ACharacter* C = Cast<ACharacter>(PC->GetPawn()))
		{
			return C->GetActorLocation().Z - C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		if (const APawn* P = PC->GetPawn())
		{
			return P->GetActorLocation().Z;
		}
	}
	return TNumericLimits<float>::Lowest();
}

void UFactoryQASubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Elapsed += DeltaTime;
	SinceReport += DeltaTime;

	if (bAudit && !bAudited && Elapsed >= AuditDelaySeconds)
	{
		bAudited = true;
		RunCollisionAudit();
	}
	if (bPen)
	{
		TickPenetration(DeltaTime);
	}
	if (bGait)
	{
		SinceProbeScan += DeltaTime;
		if (SinceProbeScan >= 0.5f) { SinceProbeScan = 0.0f; EnsureProbes(); }
	}
	if (SinceReport >= ReportInterval)
	{
		SinceReport = 0.0f;
		WriteReport();
	}
}

void UFactoryQASubsystem::EnsureProbes()
{
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* C = *It;
		if (!IsValid(C) || C->FindComponentByClass<UFactoryGaitProbeComponent>())
		{
			continue;
		}
		if (!bGaitAllCharacters && !C->IsPlayerControlled())
		{
			continue;
		}
		UFactoryGaitProbeComponent* P = NewObject<UFactoryGaitProbeComponent>(C, TEXT("FactoryGaitProbe"));
		P->bTakeShots = bShots; // ClaimShotSlot limits how many characters actually take shots
		P->RegisterComponent();
		Probes.Add(P);
	}
}

bool UFactoryQASubsystem::ClaimShotSlot(AActor* Subject)
{
	if (!bShots || !IsValid(Subject)) { return false; }
	if (ShotSubjects.Contains(Subject)) { return true; }
	const APawn* P = Cast<APawn>(Subject);
	const bool bPlayer = P && P->IsPlayerControlled();
	int32 Npcs = 0;
	for (const TWeakObjectPtr<AActor>& W : ShotSubjects)
	{
		const APawn* WP = Cast<APawn>(W.Get());
		if (!(WP && WP->IsPlayerControlled())) { ++Npcs; }
	}
	if (!bPlayer && Npcs >= FMath::Max(0, MaxShotCharacters - 1)) { return false; }
	ShotSubjects.Add(Subject);
	return true;
}

void UFactoryQASubsystem::RunCollisionAudit()
{
	PropsChecked = 0;
	TriggersSkipped = 0;
	FlatSkipped = 0;
	VfxSkipped = 0;
	TriggerVisualsSkipped.Reset();
	PropIssues.Reset();
	NonSolid.Reset();
	const float FeetZ = PlayerFeetZ();
	int32 NotBlocking = 0, NoSimple = 0, NoNav = 0;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (!IsValid(A) || A->IsHidden() || A->IsA<APawn>())
		{
			continue;
		}
		// Things carried by pawns (held loot, weapons) follow the pawn's own capsule.
		if (const AActor* Root = A->GetAttachParentActor(); Root && Root->IsA<APawn>())
		{
			continue;
		}
		bool bOwnerHasTrigger = false;
		if (bSkipTriggerOwnerVisuals)
		{
			TInlineComponentArray<UPrimitiveComponent*> Prims(A);
			for (const UPrimitiveComponent* Pc : Prims)
			{
				if (Pc && Pc->IsCollisionEnabled() && Pc->GetGenerateOverlapEvents() && Pc->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap) { bOwnerHasTrigger = true; break; }
			}
		}
		TInlineComponentArray<UStaticMeshComponent*> Comps(A);
		for (UStaticMeshComponent* C : Comps)
		{
			if (!C || !C->IsRegistered() || !C->IsVisible() || C->bHiddenInGame || !C->GetStaticMesh() || IsExempt(C))
			{
				continue;
			}
			const FBox Box = C->Bounds.GetBox();
			const FVector Size = Box.GetSize();
			if (Size.GetMax() < MinPropSizeCm)
			{
				continue;
			}
			if (FeetZ > TNumericLimits<float>::Lowest() && Box.Min.Z > FeetZ + ReachHeightCm)
			{
				continue; // out of reach overhead
			}
			if (Size.GetMin() < FlatThicknessCm)
			{
				++FlatSkipped; // quads, flat rings, rugs: walked over, never into
				continue;
			}
			if (!C->IsCollisionEnabled())
			{
				// Translucent / additive meshes with no collision are effects (dust puffs, light shafts, glows), not props.
				const UMaterialInterface* M = C->GetMaterial(0);
				const EBlendMode BM = M ? M->GetBlendMode() : BLEND_Opaque;
				if (BM != BLEND_Opaque && BM != BLEND_Masked) { ++VfxSkipped; continue; }
			}
			if (C->IsCollisionEnabled() && C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap && C->GetGenerateOverlapEvents())
			{
				++TriggersSkipped; // pickups and trigger volumes are walked into by design
				continue;
			}
			if (bOwnerHasTrigger && !(C->IsCollisionEnabled() && C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block))
			{
				TriggerVisualsSkipped.Add(A->GetName() + TEXT(".") + C->GetName()); // pickup body / marker / beacon of a trigger actor
				continue;
			}
			++PropsChecked;
			TArray<FString> Problems;
			const bool bQuery = C->IsCollisionEnabled() && C->GetCollisionEnabled() != ECollisionEnabled::PhysicsOnly;
			const bool bBlocksPawn = bQuery && C->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block;
			if (!bBlocksPawn)
			{
				static const UEnum* RespEnum = StaticEnum<ECollisionResponse>();
				static const UEnum* EnabledEnum = StaticEnum<ECollisionEnabled::Type>();
				Problems.Add(FString::Printf(TEXT("does not block Pawn (profile %s, collision %s, pawn %s, simulating %d)"),
					*C->GetCollisionProfileName().ToString(),
					*EnabledEnum->GetNameStringByValue(C->GetCollisionEnabled()),
					*RespEnum->GetNameStringByValue(C->GetCollisionResponseToChannel(ECC_Pawn)),
					C->IsSimulatingPhysics() ? 1 : 0));
				++NotBlocking;
			}
			const UBodySetup* Body = C->GetStaticMesh()->GetBodySetup();
			const bool bHasSimple = Body && (Body->AggGeom.GetElementCount() > 0 || Body->CollisionTraceFlag == CTF_UseComplexAsSimple);
			if (bBlocksPawn && !bHasSimple)
			{
				Problems.Add(TEXT("mesh has no simple collision (characters pass through)"));
				++NoSimple;
			}
			if (bBlocksPawn && !C->CanEverAffectNavigation())
			{
				Problems.Add(TEXT("does not affect navigation (NPCs path into it)"));
				++NoNav;
			}
			if (Problems.Num() > 0)
			{
				if (!bBlocksPawn || !bHasSimple) { NonSolid.Add(C); }
				PropIssues.Add({ A->GetName(), C->GetName(), C->GetStaticMesh()->GetPathName(), FString::Join(Problems, TEXT("; ")), Box.GetCenter(), Box.GetExtent() });
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] CollisionAudit: %d reachable visible props checked (%d pickups/triggers, %d trigger-owner visuals [%s], %d translucent effects and %d flat pieces skipped), %d not blocking Pawn, %d blocking but no simple collision, %d not affecting navigation -> %s"),
		PropsChecked, TriggersSkipped, TriggerVisualsSkipped.Num(), *FString::Join(TriggerVisualsSkipped, TEXT(", ")).Left(400), VfxSkipped, FlatSkipped, NotBlocking, NoSimple, NoNav, (NotBlocking + NoSimple) == 0 ? TEXT("PASS") : TEXT("FAIL"));
	int32 Shown = 0;
	for (const FPropIssue& I : PropIssues)
	{
		if (++Shown > 25) { UE_LOG(LogTemp, Display, TEXT("[FactoryQA]   ... %d more in report.json"), PropIssues.Num() - 25); break; }
		UE_LOG(LogTemp, Display, TEXT("[FactoryQA]   %s.%s (%s): %s"), *I.Owner, *I.Component, *FPaths::GetBaseFilename(I.Mesh), *I.Problems);
	}
	WriteReport();
}

void UFactoryQASubsystem::TickPenetration(float DeltaTime)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* C = *It;
		UCapsuleComponent* Cap = IsValid(C) ? C->GetCapsuleComponent() : nullptr;
		if (!Cap || !Cap->IsCollisionEnabled())
		{
			continue;
		}
		const float R = FMath::Max(1.0f, Cap->GetScaledCapsuleRadius() - PenetrationShrinkCm);
		const float HH = FMath::Max(R, Cap->GetScaledCapsuleHalfHeight() - PenetrationShrinkCm);
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryQAPen), false, C);
		TArray<AActor*> Attached;
		C->GetAttachedActors(Attached, true, true);
		Q.AddIgnoredActors(Attached);
		TArray<FOverlapResult> Hits;
		World->OverlapMultiByChannel(Hits, Cap->GetComponentLocation(), Cap->GetComponentQuat(), ECC_Pawn, FCollisionShape::MakeCapsule(R, HH), Q);
		for (const FOverlapResult& H : Hits)
		{
			UPrimitiveComponent* Other = H.GetComponent();
			if (!Other || Other->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block || IsExempt(Other))
			{
				continue;
			}
			const bool bChar = Other->GetOwner() && Other->GetOwner()->IsA<ACharacter>() && Other == Cast<ACharacter>(Other->GetOwner())->GetCapsuleComponent();
			const FString OtherName = GetNameSafe(Other->GetOwner()) + TEXT(".") + Other->GetName();
			const FString Key = C->GetName() + TEXT("|") + OtherName;
			FPenEvent& E = PenEvents.FindOrAdd(Key);
			if (E.Frames == 0)
			{
				E.Character = C->GetName(); E.Other = OtherName; E.Kind = bChar ? TEXT("character-character overlap") : TEXT("inside blocking geometry");
				E.FirstTime = Now; E.Where = C->GetActorLocation();
				UE_LOG(LogTemp, Warning, TEXT("[FactoryQA] Penetration: %s %s %s at %s (t=%.1fs)"), *E.Character, *E.Kind, *E.Other, *E.Where.ToCompactString(), Now);
			}
			++E.Frames;
		}
		// Standing inside a visible prop that has no blocking collision (found by the audit).
		const FBox CapBox = FBox::BuildAABB(Cap->GetComponentLocation(), FVector(R, R, HH));
		for (const TWeakObjectPtr<UPrimitiveComponent>& W : NonSolid)
		{
			const UPrimitiveComponent* P = W.Get();
			if (!P) { continue; }
			const FBox PB = P->Bounds.GetBox();
			if (PB.GetSize().X > ShellSizeCm && PB.GetSize().Y > ShellSizeCm) { continue; }
			if (CapBox.Intersect(PB.ExpandBy(-2.0f)))
			{
				const FString OtherName = GetNameSafe(P->GetOwner()) + TEXT(".") + P->GetName();
				FPenEvent& E = PenEvents.FindOrAdd(C->GetName() + TEXT("|ns|") + OtherName);
				if (E.Frames == 0)
				{
					E.Character = C->GetName(); E.Other = OtherName; E.Kind = TEXT("inside a NON-SOLID prop"); E.FirstTime = Now; E.Where = C->GetActorLocation();
					UE_LOG(LogTemp, Warning, TEXT("[FactoryQA] Penetration: %s is %s %s at %s (t=%.1fs)"), *E.Character, *E.Kind, *E.Other, *E.Where.ToCompactString(), Now);
				}
				++E.Frames;
			}
		}
	}
}

void UFactoryQASubsystem::CaptureLegViews(AActor* Subject, const FString& Label)
{
	if (!bShots || !IsValid(Subject))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!Capture)
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, SP);
		CaptureRT = NewObject<UTextureRenderTarget2D>(this);
		CaptureRT->RenderTargetFormat = RTF_RGBA8_SRGB;
		CaptureRT->InitAutoFormat(ShotWidth, ShotHeight);
		CaptureRT->UpdateResourceImmediate(true);
		USceneCaptureComponent2D* SC = Capture->GetCaptureComponent2D();
		SC->TextureTarget = CaptureRT;
		SC->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		SC->bCaptureEveryFrame = false;
		SC->bCaptureOnMovement = false;
		SC->bAlwaysPersistRenderingState = true;
		// Readable shots in any game: manual exposure, no fog, bloom or vignette; optionally unlit (base colour) or with a QA key light.
		FPostProcessSettings& PP = SC->PostProcessSettings;
		PP.bOverride_AutoExposureMethod = true; PP.AutoExposureMethod = AEM_Manual;
		PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true; PP.AutoExposureApplyPhysicalCameraExposure = false;
		PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = ShotExposureBias;
		PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.0f;
		PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.0f;
		SC->PostProcessBlendWeight = 1.0f;
		SC->ShowFlags.SetFog(false);
		SC->ShowFlags.SetVolumetricFog(false);
		SC->ShowFlags.SetMotionBlur(false);
		SC->ShowFlags.SetParticles(false); // footstep dust hides the feet
		if (ShotLightMode == 1)
		{
			SC->ShowFlags.SetLighting(false);
		}
		else if (ShotLightMode == 2)
		{
			UPointLightComponent* Key = NewObject<UPointLightComponent>(Capture, TEXT("FactoryQAKeyLight"));
			Key->SetIntensityUnits(ELightUnits::Candelas);
			Key->SetIntensity(ShotKeyLightCd);
			Key->SetAttenuationRadius(1200.0f);
			Key->SetCastShadows(false);
			Key->SetupAttachment(Capture->GetRootComponent());
			Key->RegisterComponent();
		}
	}
	USceneCaptureComponent2D* SC = Capture->GetCaptureComponent2D();
	SC->FOVAngle = ShotFOV;

	float HalfHeight = 90.0f;
	if (const ACharacter* C = Cast<ACharacter>(Subject)) { HalfHeight = C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); }
	const FVector Base = Subject->GetActorLocation() - FVector(0, 0, HalfHeight);           // feet
	const FVector Focus = Base + FVector(0, 0, HalfHeight * 0.5f);                          // knees / mid-leg
	const float Dist = (HalfHeight * 1.25f) / FMath::Tan(FMath::DegreesToRadians(ShotFOV * 0.5f)); // frame about the lower body
	const FVector Fwd = FVector(Subject->GetActorForwardVector().X, Subject->GetActorForwardVector().Y, 0).GetSafeNormal();
	const FVector Right = FVector(Subject->GetActorRightVector().X, Subject->GetActorRightVector().Y, 0).GetSafeNormal();

	// Isolated (default): only the character, what it carries and the floor under it, so beds, crates and walls never hide the legs.
	// Otherwise the full scene with occluders hidden by trace (meshes without collision can still block the view).
	SC->ShowOnlyActors.Reset();
	SC->ShowOnlyComponents.Reset();
	SC->HiddenActors.Reset();
	SC->HiddenComponents.Reset();
	if (bShotIsolateSubject)
	{
		SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		SC->ShowOnlyActors.Add(Subject);
		TArray<AActor*> Carried;
		Subject->GetAttachedActors(Carried, true, true);
		for (AActor* A : Carried) { SC->ShowOnlyActors.Add(A); }
		FCollisionQueryParams FQ(SCENE_QUERY_STAT(FactoryQAFloor), false, Subject);
		FHitResult FloorHit;
		if (World->LineTraceSingleByChannel(FloorHit, Base + FVector(0, 0, 30), Base - FVector(0, 0, 200), ECC_Visibility, FQ) && FloorHit.GetComponent())
		{
			SC->ShowOnlyComponents.Add(FloorHit.GetComponent());
		}
	}
	else
	{
		SC->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
	}

	const struct { const TCHAR* Name; FVector Dir; } Views[] = { { TEXT("front"), Fwd }, { TEXT("side"), Right }, { TEXT("back"), -Fwd } };
	const FString Dir = OutDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	for (const auto& V : Views)
	{
		const FVector Cam = Focus + V.Dir * Dist + FVector(0, 0, 8.0f);
		// Keep the framing; when not isolated, hide whatever stands between the camera and the legs (beds, suitcases, walls).
		SC->HiddenComponents.Reset();
		FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryQAShot), false, Subject);
		for (const float H : (bShotIsolateSubject ? TArray<float>() : TArray<float>{ 0.1f, 0.5f, 0.9f }))
		{
			const FVector Target = Base + FVector(0, 0, HalfHeight * 2.0f * H);
			TArray<FHitResult> Hits;
			World->LineTraceMultiByChannel(Hits, Cam, Target, ECC_Visibility, Q);
			TArray<FHitResult> Overlaps;
			World->SweepMultiByChannel(Overlaps, Cam, Target, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.0f), Q);
			Hits.Append(Overlaps);
			for (const FHitResult& Hh : Hits)
			{
				UPrimitiveComponent* Occ = Hh.GetComponent();
				if (!Occ || Occ->GetOwner() == Subject || (Occ->GetOwner() && Occ->GetOwner()->IsA<APawn>()) || FVector::Dist(Hh.ImpactPoint, Target) < 25.0f)
				{
					continue;
				}
				const FBox OB = Occ->Bounds.GetBox();
				if (OB.IsInsideXY(Base) && OB.Min.Z <= Base.Z + 5.0f)
				{
					continue; // the floor the subject stands on (or a level-wide mesh that includes it)
				}
				SC->HideComponent(Occ);
			}
		}
		Capture->SetActorLocationAndRotation(Cam, (Focus - Cam).Rotation());
		SC->CaptureScene();
		const FString File = FString::Printf(TEXT("leg_%s_%s_%s.png"), *Subject->GetName(), *Label, V.Name);
		UKismetRenderingLibrary::ExportRenderTarget(this, CaptureRT, Dir, File);
		++ShotCount;
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Gait shots %s (%s): front/side/back -> %s"), *Subject->GetName(), *Label, *Dir);
}

void UFactoryQASubsystem::WriteReport()
{
	const FString Dir = OutDir();
	IFileManager::Get().MakeDirectory(*Dir, true);
	using FactoryQA::Esc;

	int32 PenWorld = 0, PenChar = 0, PenNonSolid = 0;
	for (const TPair<FString, FPenEvent>& P : PenEvents)
	{
		if (P.Value.Kind.Contains(TEXT("NON-SOLID"))) { ++PenNonSolid; }
		else if (P.Value.Kind.Contains(TEXT("character"))) { ++PenChar; }
		else { ++PenWorld; }
	}
	bool bGaitPass = true;
	for (UFactoryGaitProbeComponent* P : Probes) { if (IsValid(P) && P->IsValidProbe()) { bGaitPass &= P->Passed(); } }

	int32 NotBlocking = 0, NoSimple = 0;
	for (const FPropIssue& I : PropIssues)
	{
		NotBlocking += I.Problems.Contains(TEXT("does not block")) ? 1 : 0;
		NoSimple += I.Problems.Contains(TEXT("no simple")) ? 1 : 0;
	}

	FString J = TEXT("{");
	J += FString::Printf(TEXT("\"time\":\"%s\",\"elapsedSeconds\":%.1f,"), *FDateTime::Now().ToIso8601(), Elapsed);
	J += FString::Printf(TEXT("\"collisionAudit\":{\"ran\":%s,\"propsChecked\":%d,\"notBlockingPawn\":%d,\"noSimpleCollision\":%d,\"pass\":%s,\"issues\":["),
		bAudited ? TEXT("true") : TEXT("false"), PropsChecked, NotBlocking, NoSimple, (bAudited && NotBlocking + NoSimple == 0) ? TEXT("true") : TEXT("false"));
	for (int32 i = 0; i < PropIssues.Num(); ++i)
	{
		const FPropIssue& I = PropIssues[i];
		J += FString::Printf(TEXT("%s{\"actor\":\"%s\",\"component\":\"%s\",\"mesh\":\"%s\",\"problems\":\"%s\",\"center\":\"%s\",\"extent\":\"%s\"}"),
			i ? TEXT(",") : TEXT(""), *Esc(I.Owner), *Esc(I.Component), *Esc(I.Mesh), *Esc(I.Problems), *I.Center.ToCompactString(), *I.Extent.ToCompactString());
	}
	J += FString::Printf(TEXT("]},\"penetration\":{\"ran\":%s,\"insideGeometry\":%d,\"characterOverlaps\":%d,\"insideNonSolid\":%d,\"pass\":%s,\"events\":["),
		bPen ? TEXT("true") : TEXT("false"), PenWorld, PenChar, PenNonSolid, (bPen && PenEvents.Num() == 0) ? TEXT("true") : TEXT("false"));
	int32 k = 0;
	for (const TPair<FString, FPenEvent>& P : PenEvents)
	{
		const FPenEvent& E = P.Value;
		J += FString::Printf(TEXT("%s{\"character\":\"%s\",\"other\":\"%s\",\"kind\":\"%s\",\"frames\":%d,\"firstTime\":%.2f,\"where\":\"%s\"}"),
			k++ ? TEXT(",") : TEXT(""), *Esc(E.Character), *Esc(E.Other), *E.Kind, E.Frames, E.FirstTime, *E.Where.ToCompactString());
	}
	J += FString::Printf(TEXT("]},\"gait\":{\"ran\":%s,\"pass\":%s,\"shots\":%d,\"characters\":["), bGait ? TEXT("true") : TEXT("false"), (bGait && bGaitPass && Probes.Num() > 0) ? TEXT("true") : TEXT("false"), ShotCount);
	k = 0;
	for (UFactoryGaitProbeComponent* P : Probes)
	{
		if (IsValid(P)) { J += (k++ ? TEXT(",") : TEXT("")) + P->BuildSummaryJson(); }
	}
	J += TEXT("]}}");
	FFileHelper::SaveStringToFile(J, *FPaths::Combine(Dir, TEXT("report.json")));

	FString T = FString::Printf(TEXT("FactoryQA report %s (elapsed %.1fs)\n"), *RunStamp, Elapsed);
	T += FString::Printf(TEXT("C2 collision audit: %s  props=%d notBlocking=%d noSimpleCollision=%d\n"), bAudited ? (NotBlocking + NoSimple == 0 ? TEXT("PASS") : TEXT("FAIL")) : TEXT("not run"), PropsChecked, NotBlocking, NoSimple);
	T += FString::Printf(TEXT("C2 penetration: %s  insideGeometry=%d characterOverlaps=%d insideNonSolid=%d\n"), bPen ? (PenEvents.Num() == 0 ? TEXT("PASS") : TEXT("FAIL")) : TEXT("not run"), PenWorld, PenChar, PenNonSolid);
	T += FString::Printf(TEXT("C3 gait: %s  probes=%d shots=%d\n"), bGait ? (bGaitPass && Probes.Num() > 0 ? TEXT("PASS") : TEXT("FAIL")) : TEXT("not run"), Probes.Num(), ShotCount);
	FFileHelper::SaveStringToFile(T, *FPaths::Combine(Dir, TEXT("report.txt")));
}
