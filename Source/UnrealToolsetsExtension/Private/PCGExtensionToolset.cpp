#include "PCGExtensionToolset.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "EngineUtils.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LandscapeProxy.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "PCGCommon.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGManagedResource.h"
#include "PCGNode.h"
#include "ScopedTransaction.h"
#include "Elements/PCGStaticMeshSpawner.h"

namespace PCGExtensionToolsetPrivate
{
	void RaiseError(const TCHAR* Code, const FString& Detail)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("[%s] %s"), Code, *Detail));
	}

	bool ResolveWeightedSelector(
		UPCGNode* Node,
		UPCGStaticMeshSpawnerSettings*& OutSettings,
		UPCGMeshSelectorWeighted*& OutSelector,
		UPCGGraph*& OutGraph,
		FString& OutError)
	{
		if (!IsValid(Node))
		{
			OutError = TEXT("StaticMeshSpawnerNode must reference a loaded PCG node.");
			return false;
		}

		OutGraph = Node->GetGraph();
		OutSettings = Cast<UPCGStaticMeshSpawnerSettings>(Node->GetSettings());
		if (!IsValid(OutGraph) || !IsValid(OutSettings))
		{
			OutError = TEXT("Node must belong to a PCG graph and use StaticMeshSpawner settings.");
			return false;
		}

		OutSelector = Cast<UPCGMeshSelectorWeighted>(OutSettings->MeshSelectorParameters);
		if (!IsValid(OutSelector)
			|| OutSettings->MeshSelectorType != UPCGMeshSelectorWeighted::StaticClass())
		{
			OutError = TEXT("StaticMeshSpawner node must already use PCGMeshSelectorWeighted.");
			return false;
		}
		return true;
	}

	bool ValidateEntry(
		const FPCGExtensionWeightedMeshEntryRequest& Entry,
		FString& OutError)
	{
		if (Entry.Mesh.IsNull())
		{
			OutError = TEXT("Mesh must reference a static mesh asset.");
			return false;
		}
		if (Entry.Weight < 0)
		{
			OutError = TEXT("Weight must be zero or greater.");
			return false;
		}
		if (Entry.CullStartDistanceCentimeters < 0
			|| Entry.CullEndDistanceCentimeters < 0
			|| (Entry.CullEndDistanceCentimeters > 0
				&& Entry.CullEndDistanceCentimeters < Entry.CullStartDistanceCentimeters))
		{
			OutError = TEXT("Cull distances must be non-negative, and a non-zero end distance must not precede the start distance.");
			return false;
		}

		FCollisionResponseTemplate CollisionTemplate;
		if (Entry.CollisionProfileName.IsNone()
			|| !UCollisionProfile::Get()->GetProfileTemplate(
				Entry.CollisionProfileName,
				CollisionTemplate))
		{
			OutError = FString::Printf(
				TEXT("Unknown collision profile: %s"),
				*Entry.CollisionProfileName.ToString());
			return false;
		}
		return true;
	}

	FPCGMeshSelectorWeightedEntry BuildNativeEntry(
		const FPCGExtensionWeightedMeshEntryRequest& Entry)
	{
		FPCGMeshSelectorWeightedEntry NativeEntry;
		NativeEntry.Descriptor.StaticMesh = Entry.Mesh;
		NativeEntry.Descriptor.BodyInstance.SetCollisionProfileName(
			Entry.CollisionProfileName);
		NativeEntry.Descriptor.bUseDefaultCollision = false;
		NativeEntry.Descriptor.bCanEverAffectNavigation =
			Entry.bCanEverAffectNavigation;
		NativeEntry.Descriptor.InstanceStartCullDistance =
			Entry.CullStartDistanceCentimeters;
		NativeEntry.Descriptor.InstanceEndCullDistance =
			Entry.CullEndDistanceCentimeters;
		NativeEntry.Descriptor.Hash = 0;
		NativeEntry.Weight = Entry.Weight;
		return NativeEntry;
	}

	FPCGExtensionWeightedMeshEntry BuildPublicEntry(
		const FPCGMeshSelectorWeightedEntry& NativeEntry,
		int32 Index)
	{
		FPCGExtensionWeightedMeshEntry Entry;
		Entry.Index = Index;
		Entry.Mesh = NativeEntry.Descriptor.StaticMesh;
		Entry.Weight = NativeEntry.Weight;
		Entry.CollisionProfileName =
			NativeEntry.Descriptor.BodyInstance.GetCollisionProfileName();
		Entry.CullStartDistanceCentimeters =
			NativeEntry.Descriptor.InstanceStartCullDistance;
		Entry.CullEndDistanceCentimeters =
			NativeEntry.Descriptor.InstanceEndCullDistance;
		Entry.bCanEverAffectNavigation =
			NativeEntry.Descriptor.bCanEverAffectNavigation;
		return Entry;
	}

	TArray<FPCGExtensionWeightedMeshEntry> BuildPublicEntries(
		const UPCGMeshSelectorWeighted* Selector)
	{
		TArray<FPCGExtensionWeightedMeshEntry> Entries;
		if (!IsValid(Selector))
		{
			return Entries;
		}
		Entries.Reserve(Selector->MeshEntries.Num());
		for (int32 Index = 0; Index < Selector->MeshEntries.Num(); ++Index)
		{
			Entries.Add(BuildPublicEntry(Selector->MeshEntries[Index], Index));
		}
		return Entries;
	}

	FPCGExtensionWeightedMeshMutation FinishMutation(
		UPCGNode* Node,
		UPCGMeshSelectorWeighted* Selector,
		UPCGGraph* Graph)
	{
		Selector->RefreshDisplayNames();
		Graph->MarkPackageDirty();
		Graph->ForceNotificationForEditor(
			EPCGChangeType::Settings | EPCGChangeType::ExternalModification);

		FPCGExtensionWeightedMeshMutation Result;
		Result.Node = Node;
		Result.ModifiedPackagePath = Graph->GetOutermost()->GetName();
		Result.Entries = BuildPublicEntries(Selector);
		return Result;
	}

	FPCGExtensionWeightedMeshMutation MutateEntries(
		UPCGNode* Node,
		TFunctionRef<bool(TArray<FPCGMeshSelectorWeightedEntry>&, FString&)> Mutation)
	{
		UPCGStaticMeshSpawnerSettings* Settings = nullptr;
		UPCGMeshSelectorWeighted* Selector = nullptr;
		UPCGGraph* Graph = nullptr;
		FString Error;
		if (!ResolveWeightedSelector(
			Node,
			Settings,
			Selector,
			Graph,
			Error))
		{
			RaiseError(TEXT("PCG_EXTENSION_WEIGHTED_SELECTOR_INVALID"), Error);
			return {};
		}

		TArray<FPCGMeshSelectorWeightedEntry> NewEntries = Selector->MeshEntries;
		if (!Mutation(NewEntries, Error))
		{
			RaiseError(TEXT("PCG_EXTENSION_WEIGHTED_ENTRY_INVALID"), Error);
			return {};
		}

		const FScopedTransaction Transaction(
			FText::FromString(TEXT("Unreal Toolsets Extension Edit PCG Weighted Mesh Entries")));
		Node->Modify();
		Settings->Modify();
		Selector->Modify();
		Graph->Modify();
		Selector->MeshEntries = MoveTemp(NewEntries);
		return FinishMutation(Node, Selector, Graph);
	}
}

TArray<FPCGExtensionWeightedMeshEntry>
UPCGExtensionToolset::GetWeightedMeshEntries(UPCGNode* StaticMeshSpawnerNode)
{
	using namespace PCGExtensionToolsetPrivate;
	UPCGStaticMeshSpawnerSettings* Settings = nullptr;
	UPCGMeshSelectorWeighted* Selector = nullptr;
	UPCGGraph* Graph = nullptr;
	FString Error;
	if (!ResolveWeightedSelector(
		StaticMeshSpawnerNode,
		Settings,
		Selector,
		Graph,
		Error))
	{
		RaiseError(TEXT("PCG_EXTENSION_WEIGHTED_SELECTOR_INVALID"), Error);
		return {};
	}
	return BuildPublicEntries(Selector);
}

FPCGExtensionWeightedMeshMutation
UPCGExtensionToolset::ReplaceWeightedMeshEntries(
	UPCGNode* StaticMeshSpawnerNode,
	const TArray<FPCGExtensionWeightedMeshEntryRequest>& Entries)
{
	using namespace PCGExtensionToolsetPrivate;
	return MutateEntries(
		StaticMeshSpawnerNode,
		[&Entries](TArray<FPCGMeshSelectorWeightedEntry>& NativeEntries, FString& Error)
		{
			NativeEntries.Reset();
			NativeEntries.Reserve(Entries.Num());
			for (const FPCGExtensionWeightedMeshEntryRequest& Entry : Entries)
			{
				if (!ValidateEntry(Entry, Error))
				{
					return false;
				}
				NativeEntries.Add(BuildNativeEntry(Entry));
			}
			return true;
		});
}

FPCGExtensionWeightedMeshMutation UPCGExtensionToolset::AddWeightedMeshEntry(
	UPCGNode* StaticMeshSpawnerNode,
	const FPCGExtensionWeightedMeshEntryRequest& Entry,
	int32 InsertIndex)
{
	using namespace PCGExtensionToolsetPrivate;
	return MutateEntries(
		StaticMeshSpawnerNode,
		[&Entry, InsertIndex](TArray<FPCGMeshSelectorWeightedEntry>& Entries, FString& Error)
		{
			if (!ValidateEntry(Entry, Error))
			{
				return false;
			}
			const int32 ResolvedIndex =
				InsertIndex == INDEX_NONE ? Entries.Num() : InsertIndex;
			if (ResolvedIndex < 0 || ResolvedIndex > Entries.Num())
			{
				Error = FString::Printf(
					TEXT("InsertIndex %d is outside the valid range 0..%d."),
					ResolvedIndex,
					Entries.Num());
				return false;
			}
			Entries.Insert(BuildNativeEntry(Entry), ResolvedIndex);
			return true;
		});
}

FPCGExtensionWeightedMeshMutation UPCGExtensionToolset::UpdateWeightedMeshEntry(
	UPCGNode* StaticMeshSpawnerNode,
	int32 Index,
	const FPCGExtensionWeightedMeshEntryRequest& Entry)
{
	using namespace PCGExtensionToolsetPrivate;
	return MutateEntries(
		StaticMeshSpawnerNode,
		[Index, &Entry](TArray<FPCGMeshSelectorWeightedEntry>& Entries, FString& Error)
		{
			if (!Entries.IsValidIndex(Index))
			{
				Error = FString::Printf(
					TEXT("Index %d is outside the weighted entry array."),
					Index);
				return false;
			}
			if (!ValidateEntry(Entry, Error))
			{
				return false;
			}
			Entries[Index] = BuildNativeEntry(Entry);
			return true;
		});
}

FPCGExtensionWeightedMeshMutation UPCGExtensionToolset::RemoveWeightedMeshEntry(
	UPCGNode* StaticMeshSpawnerNode,
	int32 Index)
{
	using namespace PCGExtensionToolsetPrivate;
	return MutateEntries(
		StaticMeshSpawnerNode,
		[Index](TArray<FPCGMeshSelectorWeightedEntry>& Entries, FString& Error)
		{
			if (!Entries.IsValidIndex(Index))
			{
				Error = FString::Printf(
					TEXT("Index %d is outside the weighted entry array."),
					Index);
				return false;
			}
			Entries.RemoveAt(Index);
			return true;
		});
}

FPCGExtensionWeightedMeshMutation UPCGExtensionToolset::ReorderWeightedMeshEntry(
	UPCGNode* StaticMeshSpawnerNode,
	int32 FromIndex,
	int32 ToIndex)
{
	using namespace PCGExtensionToolsetPrivate;
	return MutateEntries(
		StaticMeshSpawnerNode,
		[FromIndex, ToIndex](TArray<FPCGMeshSelectorWeightedEntry>& Entries, FString& Error)
		{
			if (!Entries.IsValidIndex(FromIndex)
				|| !Entries.IsValidIndex(ToIndex))
			{
				Error = FString::Printf(
					TEXT("FromIndex %d and ToIndex %d must both reference existing entries."),
					FromIndex,
					ToIndex);
				return false;
			}
			if (FromIndex != ToIndex)
			{
				FPCGMeshSelectorWeightedEntry MovedEntry = Entries[FromIndex];
				Entries.RemoveAt(FromIndex);
				Entries.Insert(MoveTemp(MovedEntry), ToIndex);
			}
			return true;
		});
}

FPCGExtensionGraphAudit UPCGExtensionToolset::AuditGraphInstance(
	UPCGComponent* Component,
	const FPCGExtensionGraphAuditRequest& Request)
{
	using namespace PCGExtensionToolsetPrivate;
	if (!IsValid(Component) || !IsValid(Component->GetWorld()))
	{
		RaiseError(
			TEXT("PCG_EXTENSION_COMPONENT_INVALID"),
			TEXT("Component must reference a loaded PCG component in a valid world."));
		return {};
	}
	if (Request.MaxInstancesPerComponentToSample < 1
		|| Request.MaxInstancesPerComponentToSample > 10000
		|| Request.GroundToleranceCentimeters < 0.0)
	{
		RaiseError(
			TEXT("PCG_EXTENSION_AUDIT_REQUEST_INVALID"),
			TEXT("Sampling count must be 1..10000 and ground tolerance must be non-negative."));
		return {};
	}

	FPCGExtensionGraphAudit Result;
	Result.Component = Component;
	Result.OwnerActor = Component->GetOwner();
	Result.bIsGenerating = Component->IsGenerating();
	Result.bWasGeneratedThisSession = Component->WasGeneratedThisSession();
	if (UPCGGraph* Graph = Component->GetGraph())
	{
		Result.GraphPath = Graph->GetPathName();
	}

	TArray<ALandscapeProxy*> Landscapes;
	for (TActorIterator<ALandscapeProxy> It(Component->GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			Landscapes.Add(*It);
		}
	}

	Component->ForEachConstManagedResource(
		[&](const UPCGManagedResource* Resource)
		{
			++Result.ManagedResourceCount;
			const UPCGManagedISMComponent* ManagedISM =
				Cast<UPCGManagedISMComponent>(Resource);
			if (!IsValid(ManagedISM))
			{
				return;
			}

			UInstancedStaticMeshComponent* ISM = ManagedISM->GetComponent();
			if (!IsValid(ISM))
			{
				Result.Findings.Add(
					FString::Printf(
						TEXT("Managed ISM resource %s has no loaded component."),
						*ManagedISM->GetPathName()));
				return;
			}

			FPCGExtensionInstanceComponentAudit Audit;
			Audit.ComponentPath = ISM->GetPathName();
			Audit.Mesh = ISM->GetStaticMesh();
			Audit.InstanceCount = ISM->GetInstanceCount();
			Audit.CollisionProfileName = ISM->GetCollisionProfileName();
			Audit.bCanEverAffectNavigation = ISM->CanEverAffectNavigation();
			Audit.CullStartDistanceCentimeters = ISM->InstanceStartCullDistance;
			Audit.CullEndDistanceCentimeters = ISM->InstanceEndCullDistance;
			Result.TotalInstanceCount += Audit.InstanceCount;

			const int32 SampleCount = FMath::Min(
				Audit.InstanceCount,
				Request.MaxInstancesPerComponentToSample);
			int32 LandscapeCoveredCount = 0;
			int32 GroundedCount = 0;
			for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
			{
				const int32 InstanceIndex =
					SampleCount == Audit.InstanceCount
						? SampleIndex
						: FMath::FloorToInt(
							static_cast<double>(SampleIndex)
							* Audit.InstanceCount
							/ SampleCount);
				FTransform InstanceTransform;
				if (!ISM->GetInstanceTransform(
					InstanceIndex,
					InstanceTransform,
					true))
				{
					continue;
				}

				TOptional<float> LandscapeHeight;
				for (ALandscapeProxy* Landscape : Landscapes)
				{
					LandscapeHeight = Landscape->GetHeightAtLocation(
						InstanceTransform.GetLocation());
					if (LandscapeHeight.IsSet())
					{
						break;
					}
				}
				if (!LandscapeHeight.IsSet())
				{
					continue;
				}
				++LandscapeCoveredCount;
				if (FMath::Abs(
					InstanceTransform.GetLocation().Z
					- LandscapeHeight.GetValue())
					<= Request.GroundToleranceCentimeters)
				{
					++GroundedCount;
				}
			}

			Audit.SampledInstanceCount = SampleCount;
			if (SampleCount > 0)
			{
				Audit.LandscapeCoverageRatio =
					static_cast<double>(LandscapeCoveredCount) / SampleCount;
				Audit.LandscapeGroundedRatio =
					static_cast<double>(GroundedCount) / SampleCount;
			}
			Result.InstanceComponents.Add(Audit);
		});

	if (Result.TotalInstanceCount == 0)
	{
		if (Result.GraphPath.IsEmpty())
		{
			Result.Findings.Add(TEXT("The PCG component has no graph."));
		}
		if (Result.bIsGenerating)
		{
			Result.Findings.Add(TEXT("Generation is still in progress; audit again after completion."));
		}
		if (Result.ManagedResourceCount == 0)
		{
			Result.Findings.Add(TEXT("No managed resources exist. The graph may not have executed, may have produced no data, or may have been cleaned."));
		}
		else if (Result.InstanceComponents.IsEmpty())
		{
			Result.Findings.Add(TEXT("Managed resources exist but none are ISM/HISM output. Inspect the graph's output node types."));
		}
		else
		{
			Result.Findings.Add(TEXT("ISM/HISM components exist with zero instances. Inspect input bounds, filters, density, mesh selection, and Landscape intersection."));
		}
	}
	else
	{
		Result.Findings.Add(FString::Printf(
			TEXT("Audited %d generated instances across %d ISM/HISM components without regenerating or cleaning PCG output."),
			Result.TotalInstanceCount,
			Result.InstanceComponents.Num()));
	}
	return Result;
}
