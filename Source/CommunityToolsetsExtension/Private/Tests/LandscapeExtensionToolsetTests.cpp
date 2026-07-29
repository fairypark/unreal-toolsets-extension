#if WITH_DEV_AUTOMATION_TESTS

#include "LandscapeExtensionToolset.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeExtensionToolsetContractTest,
	"AI.LandscapeExtensionToolset.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLandscapeExtensionToolsetContractTest::RunTest(const FString& Parameters)
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

	return true;
}

#endif
