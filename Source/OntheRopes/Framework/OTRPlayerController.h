// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/OTRTypes.h"
#include "OTRPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UOTRHUDWidget;
class IOTRPilotable;
struct FInputActionValue;

/**
 * Runtime Enhanced Input (no template Input folder): WASD step around the champion, LMB guard high, RMB guard low,
 * Space hold to lean on the ropes / release to launch, Shift dodge, E jab, Q guard-break, C duck, R restart on the
 * end screens. Creates the HUD in OnPossess.
 * The autoplay bot drives the same calls (Bot*), and every input (human or bot) lights its legend row.
 */
UCLASS()
class ONTHEROPES_API AOTRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AOTRPlayerController();

	static const FName KeyMove;
	static const FName KeyGuardHigh;
	static const FName KeyGuardLow;
	static const FName KeyLean;
	static const FName KeyDodge;
	static const FName KeyJab;
	static const FName KeyBreak;
	static const FName KeyDuck;
	static const FName KeyRestart;
	static const FName KeyStart;
	static const FName KeyQuit;

	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void PlayerTick(float DeltaTime) override;

	// Bot entry points (same effect as the keys).
	void BotMove(const FVector& WorldDirection, float Scale);
	void BotGuard(EOTRGuard Guard);
	void BotLean(bool bHeld);
	void BotDodge(float Side);
	void BotJab();
	void BotBreak();
	void BotDuck();

	UPROPERTY(EditAnywhere, Category = "UI") TSubclassOf<UOTRHUDWidget> HUDWidgetClass;

private:
	void BuildInput();
	bool IsBotDriving() const;
	bool IsPlaying() const;
	IOTRPilotable* Mech() const;
	void Light(FName Key, bool bSimulated);
	void ApplyGuard();

	void OnMove(const FInputActionValue& Value);
	void OnHighPressed(const FInputActionValue& Value);
	void OnHighReleased(const FInputActionValue& Value);
	void OnLowPressed(const FInputActionValue& Value);
	void OnLowReleased(const FInputActionValue& Value);
	void OnLeanPressed(const FInputActionValue& Value);
	void OnLeanReleased(const FInputActionValue& Value);
	void OnRestart(const FInputActionValue& Value);
	void OnStart(const FInputActionValue& Value);
	void OnQuit(const FInputActionValue& Value);
	void OnDodge(const FInputActionValue& Value);
	void OnJab(const FInputActionValue& Value);
	void OnBreak(const FInputActionValue& Value);
	void OnDuck(const FInputActionValue& Value);

	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> HighAction;
	UPROPERTY() TObjectPtr<UInputAction> LowAction;
	UPROPERTY() TObjectPtr<UInputAction> LeanAction;
	UPROPERTY() TObjectPtr<UInputAction> RestartAction;
	UPROPERTY() TObjectPtr<UInputAction> StartAction;
	UPROPERTY() TObjectPtr<UInputAction> QuitAction;
	UPROPERTY() TObjectPtr<UInputAction> DodgeAction;
	UPROPERTY() TObjectPtr<UInputAction> JabAction;
	UPROPERTY() TObjectPtr<UInputAction> BreakAction;
	UPROPERTY() TObjectPtr<UInputAction> DuckAction;
	float LastMoveX = 0.0f;
	UPROPERTY() TObjectPtr<UOTRHUDWidget> HUDWidget;
	float MoveLightClock = 0.0f;
	bool bHighHeld = false;
	bool bLowHeld = false;
	/** The most recently pressed guard wins when both are held. */
	EOTRGuard LastPressed = EOTRGuard::None;
	EOTRGuard BotGuardState = EOTRGuard::None;
	bool bBotLean = false;
};
