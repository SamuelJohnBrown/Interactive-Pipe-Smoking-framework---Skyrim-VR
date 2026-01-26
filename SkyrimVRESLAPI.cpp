#include "SkyrimVRESLAPI.h"
#include <random>
// Interface code based on https://github.com/adamhynek/higgs

// Stores the API after it has already been fetched
SkyrimVRESLPluginAPI::ISkyrimVRESLInterface001* g_SkyrimVRESLInterface = nullptr;

// Fetches the interface to use from SkyrimVRESL
SkyrimVRESLPluginAPI::ISkyrimVRESLInterface001* SkyrimVRESLPluginAPI::GetSkyrimVRESLInterface001(const PluginHandle& pluginHandle, SKSEMessagingInterface* messagingInterface)
{
	// If the interface has already been fetched, rturn the same object
	if (g_SkyrimVRESLInterface) {
		return g_SkyrimVRESLInterface;
	}

	// Dispatch a message to get the plugin interface from SkyrimVRESL
	SkyrimVRESLMessage message;
	messagingInterface->Dispatch(pluginHandle, SkyrimVRESLMessage::kMessage_GetInterface, (void*)&message, sizeof(SkyrimVRESLMessage*), SkyrimVRESLPluginName);
	if (!message.GetApiFunction) {
		return nullptr;
	}

	// Fetch the API for this version of the SkyrimVRESL interface
	g_SkyrimVRESLInterface = static_cast<ISkyrimVRESLInterface001*>(message.GetApiFunction(1));
	return g_SkyrimVRESLInterface;
}

static inline UInt32 getHex(std::string hexstr)
{
	return (UInt32)strtoul(hexstr.c_str(), 0, 16);
}

template <typename I> static inline std::string num2hex(I w, size_t hex_len = sizeof(I) << 1) {
	static const char* digits = "0123456789ABCDEF";
	std::string rc(hex_len, '0');
	for (size_t i = 0, j = (hex_len - 1) * 4; i < hex_len; ++i, j -= 4)
		rc[i] = digits[(w >> j) & 0x0f];
	return rc;
}

const ModInfo* NEWLookupAllLoadedModByName(const char* modName)
{
	DataHandler* dataHandler = DataHandler::GetSingleton();
	if (dataHandler)
	{
		if (!g_SkyrimVRESLInterface)
		{
			return dataHandler->LookupLoadedModByName(modName);
		}
		else
		{
			const ModInfo* modInfo = dataHandler->LookupLoadedModByName(modName);
			if (modInfo == nullptr)
			{
				modInfo = NEWLookupLoadedLightModByName(modName);
			}
			return modInfo;
		}
	}
	return nullptr;
}

const ModInfo* NEWLookupLoadedLightModByName(const char* modName)
{
	if (!g_SkyrimVRESLInterface)
	{
		DataHandler* dataHandler = DataHandler::GetSingleton();
		if (dataHandler)
		{
			return dataHandler->LookupLoadedModByName(modName);
		}
		else
		{
			return nullptr;
		}
	}
	else
	{
		const SkyrimVRESLPluginAPI::TESFileCollection* fileCollection = g_SkyrimVRESLInterface->GetCompiledFileCollection();
		if (fileCollection != nullptr)
		{
			for (int i = 0; i < fileCollection->smallFiles.count; i++)
			{
				ModInfo* smallFile = nullptr;
				fileCollection->smallFiles.GetNthItem(i, smallFile);
				if (smallFile != nullptr)
				{
					int modNameLength = strlen(modName);
					if (modNameLength == strlen(smallFile->name) && _strnicmp(smallFile->name, modName, modNameLength) == 0)
					{
						return smallFile;
					}
				}
			}
		}
		return nullptr;
	}
}

TESForm* ParseFormFromSplitted(std::vector<std::string>& splittedByPlugin)
{
	if (splittedByPlugin.size() > 1)
	{
		DataHandler* dataHandler = DataHandler::GetSingleton();
		if (dataHandler)
		{
			const ModInfo* modInfo = dataHandler->LookupLoadedModByName(splittedByPlugin[0].c_str());
			if (modInfo == nullptr)
			{
				modInfo = NEWLookupLoadedLightModByName(splittedByPlugin[0].c_str());
			}
			if (modInfo != nullptr && modInfo->IsActive())
			{
				UInt32 formLower = getHex(splittedByPlugin[1].c_str());

				if (modInfo->modIndex != 0xFE) //Is not light mod
				{
					return LookupFormByID(UInt32(modInfo->modIndex) << 24 | (formLower & 0xFFFFFF));
				}
				else //light mod
				{
					return LookupFormByID(UInt32(modInfo->modIndex) << 24 | UInt32(modInfo->lightIndex) << 12 | (formLower & 0xFFF));
				}
			}
		}
	}
	else if (splittedByPlugin.size() > 0)
	{
		UInt32 formLower = getHex(splittedByPlugin[0].c_str());
		if (formLower != 0)
		{
			return LookupFormByID(formLower);
		}
	}

	return nullptr;
}

// check if mod index is valid (mod index is the upper 8 bits of form ID)
static inline bool IsValidModIndex(UInt32 modIndex)
{
	return modIndex > 0 && modIndex != 0xFF;
}

// get base formID (without mod index)
static inline UInt32 GetBaseFormID(UInt32 formId)
{
	return formId & 0x00FFFFFF;
}

UInt32 GetFullFormIdFromEspAndFormId(const char* espName, UInt32 baseFormId)
{
	UInt32 fullFormID = 0;

	std::string espNameStr = espName;
	std::transform(espNameStr.begin(), espNameStr.end(), espNameStr.begin(), ::tolower);

	if (espNameStr == "skyrim.esm")
	{
		fullFormID = baseFormId;
	}
	else
	{
		DataHandler* dataHandler = DataHandler::GetSingleton();

		if (dataHandler)
		{
			const ModInfo* modInfo = NEWLookupAllLoadedModByName(espName);
			if (modInfo)
			{
				_MESSAGE("Modinfo %x - %x - %s", modInfo->modIndex, modInfo->lightIndex, modInfo->name);
				if (IsValidModIndex(modInfo->modIndex)) //If plugin is in the load order.
				{
					fullFormID = GetFullFormID(modInfo, GetBaseFormID(baseFormId));
				}
			}
		}
	}
	return fullFormID;
}
