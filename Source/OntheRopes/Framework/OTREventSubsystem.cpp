// Copyright On the Ropes. All Rights Reserved.

#include "Framework/OTREventSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "OntheRopes.h"

UOTREventSubsystem* UOTREventSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UOTREventSubsystem>() : nullptr;
}

bool UOTREventSubsystem::PromptOnce(FName Id, const FText& Text, AActor* Anchor, const FVector& Offset, bool bDanger)
{
	if (Prompted.Contains(Id))
	{
		return false;
	}
	Prompted.Add(Id);
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Prompt %s: %s"), *Id.ToString(), *Text.ToString());
	OnPrompt.Broadcast(Text, Anchor, Offset, bDanger);
	return true;
}
