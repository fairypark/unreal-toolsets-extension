#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include "LandscapeExtensionToolset.h"

class FCommunityToolsetsExtensionModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(ULandscapeExtensionToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		UToolsetRegistry::UnregisterToolsetClass(ULandscapeExtensionToolset::StaticClass());
	}
};

IMPLEMENT_MODULE(FCommunityToolsetsExtensionModule, CommunityToolsetsExtension)
