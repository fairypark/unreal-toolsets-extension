#if WITH_DEV_AUTOMATION_TESTS

#include "LandscapeGrassExtensionToolset.h"

#include "Engine/StaticMesh.h"
#include "LandscapeGrassType.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeGrassExtensionVarietiesTest,
	"AI.UnrealToolsetsExtension.LandscapeGrass.Varieties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLandscapeGrassExtensionVarietiesTest::RunTest(
	const FString& Parameters)
{
	const FString PackageName =
		FString(TEXT("/")) + TEXT("Game/UnrealToolsetsExtensionTests/TransientLandscapeGrassType");
	UPackage* Package = CreatePackage(*PackageName);
	ULandscapeGrassType* GrassType = NewObject<ULandscapeGrassType>(
		Package,
		TEXT("TransientLandscapeGrassType"),
		RF_Public | RF_Standalone);
	UStaticMesh* Cube = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TestNotNull(TEXT("Landscape Grass Type fixture was created"), GrassType);
	TestNotNull(TEXT("Cube mesh is available"), Cube);
	TestNotNull(TEXT("Sphere mesh is available"), Sphere);
	if (!IsValid(GrassType) || !IsValid(Cube) || !IsValid(Sphere))
	{
		return false;
	}

	FLandscapeGrassVarietyRequest CubeVariety;
	CubeVariety.Mesh = Cube;
	CubeVariety.GrassDensity = 200.0f;
	CubeVariety.StartCullDistanceCentimeters = 1000;
	CubeVariety.EndCullDistanceCentimeters = 5000;
	CubeVariety.ScaleX = FVector2D(0.8, 1.2);
	CubeVariety.ScaleY = FVector2D(0.8, 1.2);
	CubeVariety.ScaleZ = FVector2D(0.9, 1.5);

	FLandscapeGrassVarietyRequest SphereVariety;
	SphereVariety.Mesh = Sphere;
	SphereVariety.GrassDensity = 50.0f;
	SphereVariety.StartCullDistanceCentimeters = 2000;
	SphereVariety.EndCullDistanceCentimeters = 6000;

	const FLandscapeGrassMutation Replaced =
		ULandscapeGrassExtensionToolset::ReplaceGrassVarieties(
			GrassType,
			{CubeVariety, SphereVariety});
	TestEqual(TEXT("Replace writes two varieties"), Replaced.Varieties.Num(), 2);
	TestEqual(TEXT("First replacement is the cube"), Replaced.Varieties[0].Mesh.Get(), Cube);
	TestEqual(TEXT("Second replacement is the sphere"), Replaced.Varieties[1].Mesh.Get(), Sphere);

	CubeVariety.GrassDensity = 300.0f;
	const FLandscapeGrassMutation Updated =
		ULandscapeGrassExtensionToolset::UpdateGrassVariety(
			GrassType,
			0,
			CubeVariety);
	TestEqual(TEXT("Update changes density"), Updated.Varieties[0].GrassDensity, 300.0f);

	const FLandscapeGrassMutation Reordered =
		ULandscapeGrassExtensionToolset::ReorderGrassVariety(GrassType, 0, 1);
	TestEqual(TEXT("Reorder moves the sphere first"), Reordered.Varieties[0].Mesh.Get(), Sphere);
	TestEqual(TEXT("Reorder moves the cube second"), Reordered.Varieties[1].Mesh.Get(), Cube);

	const FLandscapeGrassMutation Removed =
		ULandscapeGrassExtensionToolset::RemoveGrassVariety(GrassType, 0);
	TestEqual(TEXT("Remove leaves one variety"), Removed.Varieties.Num(), 1);
	TestEqual(TEXT("The cube remains after removal"), Removed.Varieties[0].Mesh.Get(), Cube);

	const FLandscapeGrassMutation Added =
		ULandscapeGrassExtensionToolset::AddGrassVariety(
			GrassType,
			SphereVariety,
			0);
	TestEqual(TEXT("Add restores two varieties"), Added.Varieties.Num(), 2);
	TestEqual(TEXT("InsertIndex places the sphere first"), Added.Varieties[0].Mesh.Get(), Sphere);

	const FLandscapeGrassMutation ScalingDisabled =
		ULandscapeGrassExtensionToolset::SetGrassDensityScaling(
			GrassType,
			false);
	TestFalse(TEXT("Density scaling can be disabled"), ScalingDisabled.bEnableDensityScaling);
	TestTrue(TEXT("Mutations mark the package dirty"), Package->IsDirty());
	return true;
}

#endif
