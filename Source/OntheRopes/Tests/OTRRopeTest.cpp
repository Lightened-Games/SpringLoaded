// Copyright On the Ropes. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Ring/OTRRopeMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOTRRopeMathTest, "OntheRopes.Rope.Math", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOTRRopeMathTest::RunTest(const FString& Parameters)
{
	// Five hooks (0.22) fill a side rope; four fill it in a corner, capped at one full hit.
	float Load = 0.0f;
	for (int32 I = 0; I < 5; ++I)
	{
		Load = OTRRope::AddLoad(Load, 0.22f, false, 2.0f, 1.0f);
	}
	TestEqual(TEXT("side load after 5 hooks"), Load, 1.0f, 0.001f);
	Load = 0.0f;
	for (int32 I = 0; I < 3; ++I)
	{
		Load = OTRRope::AddLoad(Load, 0.22f, true, 2.0f, 1.0f);
	}
	TestEqual(TEXT("corner load after 3 hooks (capped)"), Load, 1.0f, 0.001f);

	// Fray: 0.09 per loading hit snaps on the 12th hit, exactly once.
	float Fray = 0.0f;
	bool bSnapped = false;
	int32 Snaps = 0;
	int32 SnapHit = -1;
	for (int32 I = 1; I <= 20; ++I)
	{
		if (OTRRope::AddFray(Fray, bSnapped, 0.09f))
		{
			++Snaps;
			SnapHit = I;
		}
	}
	TestEqual(TEXT("snaps once"), Snaps, 1);
	TestEqual(TEXT("snaps on hit 12"), SnapHit, 12);

	// Shape: 0 at the posts, 1 under the contact patch.
	TestEqual(TEXT("shape at post A"), OTRRope::Shape(0.0f, 0.5f), 0.0f, 0.001f);
	TestEqual(TEXT("shape at post B"), OTRRope::Shape(1.0f, 0.5f), 0.0f, 0.001f);
	TestEqual(TEXT("shape at contact"), OTRRope::Shape(0.5f, 0.5f), 1.0f, 0.001f);

	// Spring settles on its target.
	float D = 0.0f;
	float V = 0.0f;
	for (int32 I = 0; I < 600; ++I)
	{
		OTRRope::Spring(D, V, 48.0f, 70.0f, 7.0f, 1.0f / 60.0f);
	}
	TestEqual(TEXT("spring settles"), D, 48.0f, 0.5f);
	return true;
}

#endif
