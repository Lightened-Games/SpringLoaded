// Copyright On the Ropes. All Rights Reserved.

#include "Framework/OTRPlayerController.h"
#include "Framework/OTREventSubsystem.h"
#include "Interfaces/OTRInterfaces.h"
#include "UI/OTRHUDWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "OntheRopes.h"

const FName AOTRPlayerController::KeyMove(TEXT("Move"));
const FName AOTRPlayerController::KeyGuardHigh(TEXT("GuardHigh"));
const FName AOTRPlayerController::KeyGuardLow(TEXT("GuardLow"));
const FName AOTRPlayerController::KeyLean(TEXT("Lean"));
const FName AOTRPlayerController::KeyDodge(TEXT("Dodge"));
const FName AOTRPlayerController::KeyJab(TEXT("Jab"));
const FName AOTRPlayerController::KeyBreak(TEXT("Break"));
const FName AOTRPlayerController::KeyDuck(TEXT("Duck"));
const FName AOTRPlayerController::KeyRestart(TEXT("Restart"));
const FName AOTRPlayerController::KeyStart(TEXT("Start"));
const FName AOTRPlayerController::KeyQuit(TEXT("Quit"));

AOTRPlayerController::AOTRPlayerController()
{
	HUDWidgetClass = UOTRHUDWidget::StaticClass();
}

bool AOTRPlayerController::IsBotDriving() const
{
	const IOTRMatchInfo* M = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return M && M->OTR_IsAutoplay();
}

bool AOTRPlayerController::IsPlaying() const
{
	const IOTRMatchInfo* M = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return M && M->OTR_State() == EOTRMatchState::Active;
}

IOTRPilotable* AOTRPlayerController::Mech() const
{
	return Cast<IOTRPilotable>(GetPawn());
}

void AOTRPlayerController::Light(FName Key, bool bSimulated)
{
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnInput.Broadcast(Key, bSimulated);
	}
}

void AOTRPlayerController::BuildInput()
{
	if (Mapping)
	{
		return;
	}
	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_OntheRopes"));
	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	// WASD -> 2D axis (X = right, Y = forward).
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	{
		FEnhancedActionKeyMapping& W = Mapping->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* SwizzleW = NewObject<UInputModifierSwizzleAxis>(this);
		SwizzleW->Order = EInputAxisSwizzle::YXZ;
		W.Modifiers.Add(SwizzleW);
		FEnhancedActionKeyMapping& S = Mapping->MapKey(MoveAction, EKeys::S);
		UInputModifierSwizzleAxis* SwizzleS = NewObject<UInputModifierSwizzleAxis>(this);
		SwizzleS->Order = EInputAxisSwizzle::YXZ;
		S.Modifiers.Add(SwizzleS);
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		Mapping->MapKey(MoveAction, EKeys::D);
		FEnhancedActionKeyMapping& A = Mapping->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	HighAction = MakeAction(TEXT("IA_GuardHigh"), EInputActionValueType::Boolean);
	Mapping->MapKey(HighAction, EKeys::LeftMouseButton);
	LowAction = MakeAction(TEXT("IA_GuardLow"), EInputActionValueType::Boolean);
	Mapping->MapKey(LowAction, EKeys::RightMouseButton);
	LeanAction = MakeAction(TEXT("IA_Lean"), EInputActionValueType::Boolean);
	Mapping->MapKey(LeanAction, EKeys::SpaceBar);
	DodgeAction = MakeAction(TEXT("IA_Dodge"), EInputActionValueType::Boolean);
	Mapping->MapKey(DodgeAction, EKeys::LeftShift);
	JabAction = MakeAction(TEXT("IA_Jab"), EInputActionValueType::Boolean);
	Mapping->MapKey(JabAction, EKeys::E);
	BreakAction = MakeAction(TEXT("IA_Break"), EInputActionValueType::Boolean);
	Mapping->MapKey(BreakAction, EKeys::Q);
	DuckAction = MakeAction(TEXT("IA_Duck"), EInputActionValueType::Boolean);
	Mapping->MapKey(DuckAction, EKeys::C);
	RestartAction = MakeAction(TEXT("IA_Restart"), EInputActionValueType::Boolean);
	Mapping->MapKey(RestartAction, EKeys::R);
	StartAction = MakeAction(TEXT("IA_Start"), EInputActionValueType::Boolean);
	Mapping->MapKey(StartAction, EKeys::Enter);
	QuitAction = MakeAction(TEXT("IA_Quit"), EInputActionValueType::Boolean);
	Mapping->MapKey(QuitAction, EKeys::Escape);
}

void AOTRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Sub->AddMappingContext(Mapping, 0);
	}
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AOTRPlayerController::OnMove);
		EIC->BindAction(HighAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnHighPressed);
		EIC->BindAction(HighAction, ETriggerEvent::Completed, this, &AOTRPlayerController::OnHighReleased);
		EIC->BindAction(LowAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnLowPressed);
		EIC->BindAction(LowAction, ETriggerEvent::Completed, this, &AOTRPlayerController::OnLowReleased);
		EIC->BindAction(LeanAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnLeanPressed);
		EIC->BindAction(LeanAction, ETriggerEvent::Completed, this, &AOTRPlayerController::OnLeanReleased);
		EIC->BindAction(RestartAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnRestart);
		EIC->BindAction(StartAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnStart);
		EIC->BindAction(QuitAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnQuit);
		EIC->BindAction(DodgeAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnDodge);
		EIC->BindAction(JabAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnJab);
		EIC->BindAction(BreakAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnBreak);
		EIC->BindAction(DuckAction, ETriggerEvent::Started, this, &AOTRPlayerController::OnDuck);
	}
}

void AOTRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	SetControlRotation(FRotator(-20.0f, InPawn ? InPawn->GetActorRotation().Yaw : 0.0f, 0.0f));
	if (!HUDWidget && IsLocalController() && HUDWidgetClass)
	{
		HUDWidget = CreateWidget<UOTRHUDWidget>(this, HUDWidgetClass);
		if (HUDWidget)
		{
			HUDWidget->AddToViewport();
			UE_LOG(LogOntheRopes, Display, TEXT("[OTR] HUD created and added to the viewport."));
		}
	}
}

void AOTRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	MoveLightClock -= DeltaTime;
}

void AOTRPlayerController::ApplyGuard()
{
	EOTRGuard G = EOTRGuard::None;
	if (bHighHeld && bLowHeld)
	{
		G = LastPressed;
	}
	else if (bHighHeld)
	{
		G = EOTRGuard::High;
	}
	else if (bLowHeld)
	{
		G = EOTRGuard::Low;
	}
	if (IOTRPilotable* M = Mech())
	{
		M->OTR_SetGuard(G);
	}
}

void AOTRPlayerController::OnMove(const FInputActionValue& Value)
{
	if (IsBotDriving() || !IsPlaying())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	LastMoveX = Axis.X;
	if (MoveLightClock <= 0.0f && !Axis.IsNearlyZero())
	{
		MoveLightClock = 0.2f;
		Light(KeyMove, false);
	}
	if (IOTRPilotable* M = Mech())
	{
		const FRotator Yaw(0.0f, GetControlRotation().Yaw, 0.0f);
		const FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X) * Axis.Y + FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y) * Axis.X;
		M->OTR_MoveInput(Dir, FMath::Min(Dir.Size(), 1.0f));
	}
}

void AOTRPlayerController::OnHighPressed(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	bHighHeld = true;
	LastPressed = EOTRGuard::High;
	Light(KeyGuardHigh, false);
	ApplyGuard();
}

void AOTRPlayerController::OnHighReleased(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	bHighHeld = false;
	ApplyGuard();
}

void AOTRPlayerController::OnLowPressed(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	bLowHeld = true;
	LastPressed = EOTRGuard::Low;
	Light(KeyGuardLow, false);
	ApplyGuard();
}

void AOTRPlayerController::OnLowReleased(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	bLowHeld = false;
	ApplyGuard();
}

void AOTRPlayerController::OnLeanPressed(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	Light(KeyLean, false);
	if (IOTRPilotable* M = Mech())
	{
		M->OTR_SetLeanHeld(true);
	}
}

void AOTRPlayerController::OnLeanReleased(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return;
	}
	Light(KeyLean, false);
	if (IOTRPilotable* M = Mech())
	{
		M->OTR_SetLeanHeld(false);
	}
}

void AOTRPlayerController::OnRestart(const FInputActionValue& Value)
{
	if (IsBotDriving())
	{
		return; // unattended runs restart on their own; stray keys must not interrupt them
	}
	IOTRMatchInfo* Match = Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode());
	if (Match && Match->OTR_State() != EOTRMatchState::Active && Match->OTR_State() != EOTRMatchState::Ready)
	{
		Light(KeyRestart, false);
		Match->OTR_RequestRestart();
	}
}

void AOTRPlayerController::OnStart(const FInputActionValue& Value)
{
	// The fight card: ENTER steps into the ring.
	IOTRMatchInfo* Match = Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode());
	if (!IsBotDriving() && Match && Match->OTR_IsTitle())
	{
		Light(KeyStart, false);
		Match->OTR_StartFight();
	}
}

void AOTRPlayerController::OnQuit(const FInputActionValue& Value)
{
	// ESC leaves the hall from the fight card or the result card (never mid-bout).
	const IOTRMatchInfo* Match = Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode());
	if (Match && (Match->OTR_IsTitle() || Match->OTR_State() == EOTRMatchState::Victory || Match->OTR_State() == EOTRMatchState::Defeat))
	{
		Light(KeyQuit, false);
		ConsoleCommand(TEXT("quit"));
	}
}

void AOTRPlayerController::OnDodge(const FInputActionValue& Value)
{
	if (IsBotDriving() || !IsPlaying())
	{
		return;
	}
	// Dodge toward the held A / D side (right by default).
	if (IOTRPilotable* M = Mech())
	{
		if (M->OTR_Dodge(LastMoveX < -0.1f ? -1.0f : 1.0f))
		{
			Light(KeyDodge, false);
		}
	}
	LastMoveX = 0.0f;
}

void AOTRPlayerController::BotDodge(float Side)
{
	if (IOTRPilotable* M = Mech())
	{
		if (IsPlaying() && M->OTR_Dodge(Side))
		{
			Light(KeyDodge, true);
		}
	}
}

void AOTRPlayerController::BotMove(const FVector& WorldDirection, float Scale)
{
	IOTRPilotable* M = Mech();
	if (!M || !IsPlaying() || Scale <= 0.01f)
	{
		return;
	}
	M->OTR_MoveInput(WorldDirection, Scale);
	if (MoveLightClock <= 0.0f && M->OTR_CanMove())
	{
		MoveLightClock = 0.2f;
		Light(KeyMove, true);
	}
}

void AOTRPlayerController::BotGuard(EOTRGuard Guard)
{
	if (Guard != BotGuardState)
	{
		if (Guard == EOTRGuard::High) { Light(KeyGuardHigh, true); }
		if (Guard == EOTRGuard::Low) { Light(KeyGuardLow, true); }
		BotGuardState = Guard;
	}
	if (IOTRPilotable* M = Mech())
	{
		M->OTR_SetGuard(Guard);
	}
}

void AOTRPlayerController::BotLean(bool bHeld)
{
	if (bHeld != bBotLean)
	{
		Light(KeyLean, true);
		bBotLean = bHeld;
	}
	if (IOTRPilotable* M = Mech())
	{
		M->OTR_SetLeanHeld(bHeld);
	}
}

void AOTRPlayerController::OnJab(const FInputActionValue& Value)
{
	if (IsBotDriving() || !IsPlaying())
	{
		return;
	}
	if (IOTRPilotable* M = Mech())
	{
		if (M->OTR_Jab())
		{
			Light(KeyJab, false);
		}
	}
}

void AOTRPlayerController::OnBreak(const FInputActionValue& Value)
{
	if (IsBotDriving() || !IsPlaying())
	{
		return;
	}
	if (IOTRPilotable* M = Mech())
	{
		if (M->OTR_Break())
		{
			Light(KeyBreak, false);
		}
	}
}

void AOTRPlayerController::OnDuck(const FInputActionValue& Value)
{
	if (IsBotDriving() || !IsPlaying())
	{
		return;
	}
	if (IOTRPilotable* M = Mech())
	{
		if (M->OTR_Duck())
		{
			Light(KeyDuck, false);
		}
	}
}

void AOTRPlayerController::BotJab()
{
	IOTRPilotable* M = Mech();
	if (M && IsPlaying() && M->OTR_Jab())
	{
		Light(KeyJab, true);
	}
}

void AOTRPlayerController::BotBreak()
{
	IOTRPilotable* M = Mech();
	if (M && IsPlaying() && M->OTR_Break())
	{
		Light(KeyBreak, true);
	}
}

void AOTRPlayerController::BotDuck()
{
	IOTRPilotable* M = Mech();
	if (M && IsPlaying() && M->OTR_Duck())
	{
		Light(KeyDuck, true);
	}
}
