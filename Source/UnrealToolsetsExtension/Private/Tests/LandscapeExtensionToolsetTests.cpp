#if WITH_DEV_AUTOMATION_TESTS

#include "LandscapeExtensionToolset.h"
#include "LandscapeGrassExtensionToolset.h"
#include "PCGExtensionToolset.h"
#include "PlacementExtensionToolset.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUnrealToolsetsExtensionContractTest,
	"AI.UnrealToolsetsExtension.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealToolsetsExtensionContractTest::RunTest(const FString& Parameters)
{
	const UClass* ToolsetClass = ULandscapeExtensionToolset::StaticClass();
	TestNotNull(TEXT("LandscapeExtensionToolset class is reflected"), ToolsetClass);

	const TArray<FName> RequiredFunctions = {
		TEXT("GetLandscapeInfo"),
		TEXT("CreateLandscape"),
		TEXT("ImportHeightmap"),
		TEXT("ReimportHeightmap"),
		TEXT("ImportWeightmap"),
		TEXT("SetLandscapeMaterial"),
		TEXT("GetHeightStatistics"),
		TEXT("ValidateBuildingPad"),
		TEXT("PreflightReimportLandscapeData"),
		TEXT("ReimportLandscapeData"),
		TEXT("AuditStreamingProxies"),
		TEXT("RebuildStreamingProxies"),
		TEXT("SaveLandscape")
	};

	for (const FName FunctionName : RequiredFunctions)
	{
		const UFunction* Function = ToolsetClass->FindFunctionByName(FunctionName);
		TestNotNull(*FString::Printf(TEXT("%s is reflected"), *FunctionName.ToString()), Function);
		if (Function != nullptr)
		{
			TestTrue(
				*FString::Printf(TEXT("%s is static"), *FunctionName.ToString()),
				Function->HasAnyFunctionFlags(FUNC_Static));
			TestTrue(
				*FString::Printf(TEXT("%s is AICallable"), *FunctionName.ToString()),
				Function->HasMetaData(TEXT("AICallable")));
		}
	}

	const UClass* PlacementClass = UPlacementExtensionToolset::StaticClass();
	TestNotNull(TEXT("PlacementExtensionToolset class is reflected"), PlacementClass);
	const UFunction* SnapFunction = PlacementClass != nullptr
		? PlacementClass->FindFunctionByName(TEXT("SnapActorToGround"))
		: nullptr;
	TestNotNull(TEXT("SnapActorToGround is reflected"), SnapFunction);
	if (SnapFunction != nullptr)
	{
		TestTrue(TEXT("SnapActorToGround is static"), SnapFunction->HasAnyFunctionFlags(FUNC_Static));
		TestTrue(TEXT("SnapActorToGround is AICallable"), SnapFunction->HasMetaData(TEXT("AICallable")));
	}

	const UClass* PCGClass = UPCGExtensionToolset::StaticClass();
	TestNotNull(TEXT("PCGExtensionToolset class is reflected"), PCGClass);
	const UClass* LandscapeGrassClass =
		ULandscapeGrassExtensionToolset::StaticClass();
	TestNotNull(
		TEXT("LandscapeGrassExtensionToolset class is reflected"),
		LandscapeGrassClass);
	const TArray<FName> PCGFunctions = {
		TEXT("GetWeightedMeshEntries"),
		TEXT("ReplaceWeightedMeshEntries"),
		TEXT("AddWeightedMeshEntry"),
		TEXT("UpdateWeightedMeshEntry"),
		TEXT("RemoveWeightedMeshEntry"),
		TEXT("ReorderWeightedMeshEntry"),
		TEXT("AuditGraphInstance")
	};
	for (const FName FunctionName : PCGFunctions)
	{
		const UFunction* Function = PCGClass != nullptr
			? PCGClass->FindFunctionByName(FunctionName)
			: nullptr;
		TestNotNull(
			*FString::Printf(TEXT("%s is reflected"), *FunctionName.ToString()),
			Function);
		if (Function != nullptr)
		{
			TestTrue(
				*FString::Printf(TEXT("%s is static"), *FunctionName.ToString()),
				Function->HasAnyFunctionFlags(FUNC_Static));
			TestTrue(
				*FString::Printf(TEXT("%s is AICallable"), *FunctionName.ToString()),
				Function->HasMetaData(TEXT("AICallable")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeExtensionCreateAndAuditTest,
	"AI.UnrealToolsetsExtension.Landscape.CreateAndAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLandscapeExtensionCreateAndAuditTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Disposable Editor world was created"), World);
	if (!IsValid(World))
	{
		return false;
	}

	FLandscapeExtensionCreateRequest Request;
	Request.ActorLabel = TEXT("AutomationLandscape");
	Request.QuadsPerSection = 7;
	Request.SectionsPerComponent = 1;
	Request.ComponentCountX = 1;
	Request.ComponentCountY = 1;
	Request.FlatRawHeight = 32768;
	Request.bSaveAfterCreate = false;

	const FLandscapeExtensionMutationResult Mutation =
		ULandscapeExtensionToolset::CreateLandscape(World, Request);
	ALandscape* Landscape = Mutation.Landscape.Get();
	TestNotNull(TEXT("CreateLandscape returned a Landscape"), Landscape);
	if (!IsValid(Landscape))
	{
		return false;
	}

	const FLandscapeExtensionInfoResult Info =
		ULandscapeExtensionToolset::GetLandscapeInfo(Landscape);
	TestEqual(TEXT("Created width is exact"), Info.VertexResolution.X, 8);
	TestEqual(TEXT("Created height is exact"), Info.VertexResolution.Y, 8);
	TestFalse(TEXT("Disposable map is not World Partition"), Info.bWorldPartition);

	FLandscapeExtensionRegion Region;
	const FLandscapeExtensionHeightStatistics Statistics =
		ULandscapeExtensionToolset::GetHeightStatistics(Landscape, Region);
	TestEqual(TEXT("All vertices were read"), Statistics.SampleCount, 64);
	TestEqual(TEXT("Flat minimum is preserved"), Statistics.MinRawHeight, 32768);
	TestEqual(TEXT("Flat maximum is preserved"), Statistics.MaxRawHeight, 32768);
	TestTrue(TEXT("The requested extent was fully covered"), Statistics.bFullRequestedRegionCovered);

	FLandscapeExtensionProxyAuditRequest AuditRequest;
	const FLandscapeExtensionProxyAudit Audit =
		ULandscapeExtensionToolset::AuditStreamingProxies(Landscape, AuditRequest);
	TestFalse(TEXT("Non-partitioned map is reported accurately"), Audit.bWorldPartition);
	TestTrue(TEXT("Audit returns a non-partitioned finding"), !Audit.Findings.IsEmpty());

	World->DestroyActor(Landscape);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlacementExtensionNegativeHeightSnapTest,
	"AI.UnrealToolsetsExtension.Placement.NegativeHeightAndSelfIgnore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlacementExtensionNegativeHeightSnapTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Disposable Editor world was created"), World);
	if (!IsValid(World))
	{
		return false;
	}

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	TestNotNull(TEXT("Engine cube mesh is available"), CubeMesh);
	if (!IsValid(CubeMesh))
	{
		return false;
	}

	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(
		FVector(0.0, 0.0, -1000.0),
		FRotator::ZeroRotator);
	AStaticMeshActor* Target = World->SpawnActor<AStaticMeshActor>(
		FVector(0.0, 0.0, 200.0),
		FRotator::ZeroRotator);
	TestNotNull(TEXT("Floor Actor was spawned"), Floor);
	TestNotNull(TEXT("Target Actor was spawned"), Target);
	if (!IsValid(Floor) || !IsValid(Target))
	{
		return false;
	}

	Floor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	Floor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Floor->SetActorScale3D(FVector(10.0, 10.0, 1.0));
	Target->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	Target->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Target->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	World->UpdateWorldComponents(false, false);

	FPlacementExtensionSnapRequest Request;
	Request.TraceUpCentimeters = 2000.0;
	Request.TraceDownCentimeters = 5000.0;
	Request.bPreferLandscape = false;
	Request.bLandscapeOnly = false;
	Request.bUseActorBoundsBottom = true;

	const FPlacementExtensionSnapResult Result =
		UPlacementExtensionToolset::SnapActorToGround(Target, Request);
	TestTrue(
		TEXT("The floor, not the target itself, answered the trace"),
		Result.HitActor.Get() == Floor);
	TestTrue(TEXT("The chosen ground surface is below world zero"), Result.HitLocation.Z < 0.0);
	TestTrue(TEXT("The Actor moved downward"), Result.AppliedDeltaZCentimeters < 0.0);

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	Target->GetActorBounds(false, BoundsOrigin, BoundsExtent, true);
	TestEqual(
		TEXT("Actor bounds bottom aligns to the negative-height hit"),
		BoundsOrigin.Z - BoundsExtent.Z,
		Result.HitLocation.Z,
		0.1);

	World->DestroyActor(Floor);
	const FVector LocationBeforeNoHit = Target->GetActorLocation();
	const FPlacementExtensionSnapResult NoHitResult =
		UPlacementExtensionToolset::SnapActorToGround(Target, Request);
	TestTrue(
		TEXT("A self-only trace reports no hit Actor"),
		NoHitResult.HitActor.IsNull());
	TestEqual(
		TEXT("A self-only trace does not move the Actor"),
		Target->GetActorLocation(),
		LocationBeforeNoHit);

	World->DestroyActor(Target);
	return true;
}

#endif
