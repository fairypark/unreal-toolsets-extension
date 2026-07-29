#if WITH_DEV_AUTOMATION_TESTS

#include "PCGExtensionToolset.h"

#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Misc/AutomationTest.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExtensionWeightedEntriesTest,
	"AI.UnrealToolsetsExtension.PCG.WeightedMeshEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExtensionWeightedEntriesTest::RunTest(const FString& Parameters)
{
	UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage());
	UPCGStaticMeshSpawnerSettings* Settings = nullptr;
	UPCGNode* Node = Graph->AddNodeOfType(Settings);
	TestNotNull(TEXT("Transient PCG graph was created"), Graph);
	TestNotNull(TEXT("Static Mesh Spawner node was created"), Node);
	TestNotNull(TEXT("Static Mesh Spawner settings were created"), Settings);
	if (!IsValid(Graph) || !IsValid(Node) || !IsValid(Settings))
	{
		return false;
	}

	Settings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
	TestTrue(
		TEXT("Node uses the weighted mesh selector"),
		Settings->MeshSelectorType == UPCGMeshSelectorWeighted::StaticClass());

	UStaticMesh* Cube = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(
		nullptr,
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TestNotNull(TEXT("Cube mesh is available"), Cube);
	TestNotNull(TEXT("Sphere mesh is available"), Sphere);
	if (!IsValid(Cube) || !IsValid(Sphere))
	{
		return false;
	}

	FPCGExtensionWeightedMeshEntryRequest CubeEntry;
	CubeEntry.Mesh = Cube;
	CubeEntry.Weight = 2;
	CubeEntry.CollisionProfileName = TEXT("NoCollision");
	CubeEntry.CullStartDistanceCentimeters = 1000;
	CubeEntry.CullEndDistanceCentimeters = 5000;

	FPCGExtensionWeightedMeshEntryRequest SphereEntry;
	SphereEntry.Mesh = Sphere;
	SphereEntry.Weight = 3;
	SphereEntry.CollisionProfileName = TEXT("NoCollision");
	SphereEntry.CullStartDistanceCentimeters = 2000;
	SphereEntry.CullEndDistanceCentimeters = 6000;

	const FPCGExtensionWeightedMeshMutation Replaced =
		UPCGExtensionToolset::ReplaceWeightedMeshEntries(
			Node,
			{CubeEntry, SphereEntry});
	TestEqual(TEXT("Replace writes two entries"), Replaced.Entries.Num(), 2);
	TestEqual(TEXT("First replacement is the cube"), Replaced.Entries[0].Mesh.Get(), Cube);
	TestEqual(TEXT("Second replacement is the sphere"), Replaced.Entries[1].Mesh.Get(), Sphere);

	CubeEntry.Weight = 5;
	const FPCGExtensionWeightedMeshMutation Updated =
		UPCGExtensionToolset::UpdateWeightedMeshEntry(Node, 0, CubeEntry);
	TestEqual(TEXT("Update changes the selected weight"), Updated.Entries[0].Weight, 5);

	const FPCGExtensionWeightedMeshMutation Reordered =
		UPCGExtensionToolset::ReorderWeightedMeshEntry(Node, 0, 1);
	TestEqual(TEXT("Reorder moves the sphere first"), Reordered.Entries[0].Mesh.Get(), Sphere);
	TestEqual(TEXT("Reorder moves the cube second"), Reordered.Entries[1].Mesh.Get(), Cube);

	const FPCGExtensionWeightedMeshMutation Removed =
		UPCGExtensionToolset::RemoveWeightedMeshEntry(Node, 0);
	TestEqual(TEXT("Remove leaves one entry"), Removed.Entries.Num(), 1);
	TestEqual(TEXT("The cube remains after removal"), Removed.Entries[0].Mesh.Get(), Cube);

	const FPCGExtensionWeightedMeshMutation Added =
		UPCGExtensionToolset::AddWeightedMeshEntry(Node, SphereEntry, 0);
	TestEqual(TEXT("Add restores two entries"), Added.Entries.Num(), 2);
	TestEqual(TEXT("InsertIndex places the sphere first"), Added.Entries[0].Mesh.Get(), Sphere);
	TestEqual(TEXT("The cube remains second"), Added.Entries[1].Mesh.Get(), Cube);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExtensionEmptyAuditTest,
	"AI.UnrealToolsetsExtension.PCG.EmptyGraphAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPCGExtensionEmptyAuditTest::RunTest(const FString& Parameters)
{
	UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
	TestNotNull(TEXT("Disposable Editor world was created"), World);
	if (!IsValid(World))
	{
		return false;
	}

	AActor* Owner = World->SpawnActor<AActor>();
	TestNotNull(TEXT("PCG owner Actor was created"), Owner);
	if (!IsValid(Owner))
	{
		return false;
	}

	UPCGComponent* Component = NewObject<UPCGComponent>(
		Owner,
		TEXT("PCGAuditComponent"));
	Owner->AddInstanceComponent(Component);

	FPCGExtensionGraphAuditRequest Request;
	const FPCGExtensionGraphAudit Audit =
		UPCGExtensionToolset::AuditGraphInstance(Component, Request);
	TestEqual(TEXT("Empty component has no generated instances"), Audit.TotalInstanceCount, 0);
	TestEqual(TEXT("Empty component has no managed resources"), Audit.ManagedResourceCount, 0);
	TestTrue(TEXT("Empty audit explains the missing graph"), Audit.Findings.Contains(TEXT("The PCG component has no graph.")));

	World->DestroyActor(Owner);
	return true;
}

#endif
