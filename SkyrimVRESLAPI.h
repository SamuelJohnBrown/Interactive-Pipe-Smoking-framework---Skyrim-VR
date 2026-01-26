#pragma once
#include "skse64/PluginAPI.h"
#include "skse64/GameReferences.h"
#include <skse64/GameData.h>
#include <vector>
// Interface code based on https://github.com/adamhynek/higgs

namespace SkyrimVRESLPluginAPI
{
	constexpr const auto SkyrimVRESLPluginName = "SkyrimVRESL";
	// A message used to fetch SkyrimVRESL's interface
	struct SkyrimVRESLMessage
	{
		enum : uint32_t
		{
			kMessage_GetInterface = 0xeacb2bef
		};  // Randomly generated
		void* (*GetApiFunction)(unsigned int revisionNumber) = nullptr;
	};

	struct TESFileCollection
	{
	public:
		// members
		tArray<ModInfo*> files;       // 00
		tArray<ModInfo*> smallFiles;  // 18
	};
	STATIC_ASSERT(sizeof(TESFileCollection) == 0x30);

	// Returns an ISkyrimVRESLInterface001 object compatible with the API shown below
	// This should only be called after SKSE sends kMessage_PostLoad to your plugin
	struct ISkyrimVRESLInterface001;
	ISkyrimVRESLInterface001* GetSkyrimVRESLInterface001(const PluginHandle& pluginHandle, SKSEMessagingInterface* messagingInterface);

	// This object provides access to SkyrimVRESL's mod support API
	struct ISkyrimVRESLInterface001
	{
		// Gets the SkyrimVRESL build number
		virtual unsigned int GetBuildNumber() = 0;

		/// @brief Get the SSE compatible TESFileCollection for SkyrimVR.
		/// This should be called after kDataLoaded to ensure it's been populated.
		/// @return Pointer to TESFileCollection CompiledFileCollection.
		const virtual TESFileCollection* GetCompiledFileCollection() = 0;
	};

}  // namespace SkyrimVRESLPluginAPI
extern SkyrimVRESLPluginAPI::ISkyrimVRESLInterface001* g_SkyrimVRESLInterface;


// get base formID (without mod index)
static inline UInt32 GetLightBaseFormID(UInt32 formId)
{
	return formId & 0x00000FFF;
}

struct ModIndex
{
	UInt8 modIndex = 0xFF;
	UInt16 lightIndex = 0;

	// Define the equality operator
	bool operator==(const ModIndex& other) const {
		return modIndex == other.modIndex && lightIndex == other.lightIndex;
	}

	// Less-than operator (used for ordering in the map)
	bool operator<(const ModIndex& other) const {
		return modIndex < other.modIndex || (modIndex == other.modIndex && lightIndex < other.lightIndex);
	}
};

namespace std {
	template <>
	struct hash<ModIndex> {
		std::size_t operator()(const ModIndex& key) const {
			// Combine the hash values of modIndex and lightIndex
			return hash<UInt8>()(key.modIndex) ^ (hash<UInt16>()(key.lightIndex) << 1);
		}
	};
}

// get mod index from a normal form ID 32 bit unsigned
static inline ModIndex GetModIndexFromFormId(UInt32 formId)
{
	ModIndex result;

	result.modIndex = static_cast<UInt8>(formId >> 24);

	if (result.modIndex != 0xFE)
	{
		result.lightIndex = 0;
	}
	else
	{
		result.lightIndex = static_cast<UInt16>((formId & 0x00FFFFFF) >> 12);
	}

	return result;
}

// Converts the lower bits of a FormID to a full FormID depending on plugin type
static inline UInt32 GetFullFormID(const ModInfo* modInfo, UInt32 formLower)
{
	return !modInfo->IsLight() ? UInt32(modInfo->modIndex) << 24 | (formLower & 0xFFFFFF) : 0xFE000000 | (UInt32(modInfo->lightIndex) << 12) | (formLower & 0xFFF);
}

//Useful functions
const ModInfo* NEWLookupAllLoadedModByName(const char* modName);
const ModInfo* NEWLookupLoadedLightModByName(const char* modName);
TESForm* ParseFormFromSplitted(std::vector<std::string>& splittedByPlugin);

UInt32 GetFullFormIdFromEspAndFormId(const char* espName, UInt32 baseFormId);
