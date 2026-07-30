#if WITH_DEV_AUTOMATION_TESTS

#include "ObservabilityExtensionToolset.h"

#include "FabExtensionToolset.h"
#include "LandscapeExtensionToolset.h"
#include "LandscapeGrassExtensionToolset.h"
#include "Misc/AutomationTest.h"
#include "PCGExtensionToolset.h"
#include "PlacementExtensionToolset.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FObservabilityExtensionCatalogTest,
	"AI.UnrealToolsetsExtension.Observability.CatalogMatchesReflection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObservabilityExtensionCatalogTest::RunTest(const FString& Parameters)
{
	const FUnrealToolsetsExtensionObservabilityInfo Info =
		UObservabilityExtensionToolset::GetExtensionObservabilityInfo();
	TestEqual(TEXT("Extension identity is stable"), Info.ExtensionName, TEXT("UnrealToolsetsExtension"));
	TestFalse(TEXT("Extension version is reported"), Info.ExtensionVersion.IsEmpty());
	TestFalse(TEXT("Engine version is reported"), Info.EngineVersion.IsEmpty());
	TestEqual(TEXT("Observability contract version"), Info.ObservabilityContractVersion, 1);

	const TArray<UClass*> ToolsetClasses = {
		UObservabilityExtensionToolset::StaticClass(),
		ULandscapeExtensionToolset::StaticClass(),
		UPlacementExtensionToolset::StaticClass(),
		UPCGExtensionToolset::StaticClass(),
		ULandscapeGrassExtensionToolset::StaticClass(),
		UFabExtensionToolset::StaticClass()
	};

	TSet<FString> ReflectedOperations;
	for (const UClass* ToolsetClass : ToolsetClasses)
	{
		for (TFieldIterator<UFunction> FunctionIt(
			ToolsetClass,
			EFieldIteratorFlags::ExcludeSuper);
			FunctionIt;
			++FunctionIt)
		{
			const UFunction* Function = *FunctionIt;
			if (Function->HasMetaData(TEXT("AICallable")))
			{
				ReflectedOperations.Add(
					FString::Printf(
						TEXT("%s.%s"),
						*ToolsetClass->GetName(),
						*Function->GetName()));
			}
		}
	}

	TSet<FString> CatalogOperations;
	for (const FUnrealToolsetsExtensionOperationDescriptor& Descriptor : Info.Operations)
	{
		const FString Key = FString::Printf(
			TEXT("%s.%s"),
			*Descriptor.ToolsetName,
			*Descriptor.OperationName);
		TestTrue(*FString::Printf(TEXT("Catalog operation is unique: %s"), *Key), !CatalogOperations.Contains(Key));
		CatalogOperations.Add(Key);
	}
	TestEqual(TEXT("Catalog operation count"), CatalogOperations.Num(), 34);
	bool bCatalogMatchesReflection =
		CatalogOperations.Num() == ReflectedOperations.Num();
	for (const FString& ReflectedOperation : ReflectedOperations)
	{
		bCatalogMatchesReflection &=
			CatalogOperations.Contains(ReflectedOperation);
	}
	TestTrue(
		TEXT("Catalog exactly matches reflected AICallable operations"),
		bCatalogMatchesReflection);

	TSet<FString> ErrorCodes;
	for (const FUnrealToolsetsExtensionErrorDescriptor& Descriptor : Info.Errors)
	{
		TestFalse(TEXT("Catalog error code is non-empty"), Descriptor.ErrorCode.IsEmpty());
		TestTrue(
			*FString::Printf(TEXT("Catalog error is unique: %s"), *Descriptor.ErrorCode),
			!ErrorCodes.Contains(Descriptor.ErrorCode));
		ErrorCodes.Add(Descriptor.ErrorCode);
	}
	TestEqual(TEXT("Catalog error-code count"), ErrorCodes.Num(), 55);
	TestTrue(
		TEXT("Verified rollback code is cataloged"),
		ErrorCodes.Contains(TEXT("LANDSCAPE_EXTENSION_BATCH_VERIFY_FAILED_ROLLED_BACK")));
	return true;
}

#endif
