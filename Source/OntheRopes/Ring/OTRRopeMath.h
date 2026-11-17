// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Pure rope maths (no engine state), shared by the ring, the lean component and the automation test.
 * Ropes are analytic, not simulated (rope physics is on the design guide high-risk list): a taut string pushed at one
 * point takes a V shape between its posts, driven by a damped spring toward the load-dependent deflection.
 */
namespace OTRRope
{
	/** Shape of a rope pushed at Contact (0..1), with a flat patch of PatchHalf (the mech's back) around it. */
	inline float Shape(float T, float Contact, float PatchHalf = 0.06f)
	{
		const float C = FMath::Clamp(Contact, 0.12f, 0.88f);
		const float A = FMath::Max(C - PatchHalf, 0.02f);
		const float B = FMath::Min(C + PatchHalf, 0.98f);
		if (T <= A) { return FMath::Clamp(T / A, 0.0f, 1.0f); }
		if (T >= B) { return FMath::Clamp((1.0f - T) / (1.0f - B), 0.0f, 1.0f); }
		return 1.0f;
	}

	/** Target deflection for a lean with this stored load (0..1). */
	inline float TargetDeflect(bool bPressed, float Load, float MaxDeflect, float RestFraction)
	{
		return bPressed ? MaxDeflect * (RestFraction + (1.0f - RestFraction) * FMath::Clamp(Load, 0.0f, 1.0f)) : 0.0f;
	}

	/** One damped-spring step. */
	inline void Spring(float& Value, float& Velocity, float TargetValue, float Stiffness, float Damping, float Dt)
	{
		Velocity += (Stiffness * (TargetValue - Value) - Damping * Velocity) * Dt;
		Value += Velocity * Dt;
	}

	/** Store energy from one absorbed hit. Returns the new load, capped at MaxLoad. */
	inline float AddLoad(float Load, float Gain, bool bCorner, float CornerMultiplier, float MaxLoad)
	{
		return FMath::Min(MaxLoad, Load + Gain * (bCorner ? CornerMultiplier : 1.0f));
	}

	/** Fray a rope. Returns true the moment it reaches 1 (snaps). */
	inline bool AddFray(float& Fray, bool& bSnapped, float Amount)
	{
		if (bSnapped)
		{
			return false;
		}
		Fray = FMath::Min(1.0f, Fray + Amount);
		if (Fray >= 1.0f)
		{
			bSnapped = true;
			return true;
		}
		return false;
	}
}
