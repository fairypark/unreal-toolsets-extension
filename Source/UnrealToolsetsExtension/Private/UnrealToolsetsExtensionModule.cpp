#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "LandscapeExtensionToolset.h"
#include "LandscapeGrassExtensionToolset.h"
#include "FabExtensionToolset.h"
#include "ObservabilityExtensionToolset.h"
#include "PCGExtensionToolset.h"
#include "PlacementExtensionToolset.h"

class FUnrealToolsetsExtensionModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(ULandscapeExtensionToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(ULandscapeGrassExtensionToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UFabExtensionToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UPlacementExtensionToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UPCGExtensionToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UObservabilityExtensionToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(UObservabilityExtensionToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UPCGExtensionToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UPlacementExtensionToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UFabExtensionToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(ULandscapeGrassExtensionToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(ULandscapeExtensionToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FUnrealToolsetsExtensionModule, UnrealToolsetsExtension)
