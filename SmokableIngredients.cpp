#include "SmokableIngredients.h"
#include "skse64_common/skse_version.h"
#include "Helper.h"
#include "skse64/GameForms.h"
#include "skse64/GameRTTI.h"
#include "skse64/GameObjects.h"
#include <vector>

namespace InteractivePipeSmokingVR
{
	// Static member initialization
	std::unordered_set<UInt32> SmokableIngredients::s_smokableFormIds;
	std::unordered_set<UInt32> SmokableIngredients::s_cannabisFormIds;
	std::unordered_map<UInt32, SmokableCategory> SmokableIngredients::s_smokableCategories;
	bool SmokableIngredients::s_initialized = false;

	void SmokableIngredients::Initialize()
	{
		if (s_initialized)
			return;

		using namespace SmokableFormIDs;

		// === HEALING ===
		s_smokableFormIds.insert(RedMountainFlower);
		s_smokableCategories[RedMountainFlower] = SmokableCategory::Healing;

		s_smokableFormIds.insert(Blisterwort);
		s_smokableCategories[Blisterwort] = SmokableCategory::Healing;

		s_smokableFormIds.insert(ImpStool);
		s_smokableCategories[ImpStool] = SmokableCategory::Healing;

		s_smokableFormIds.insert(Nirnroot);
		s_smokableCategories[Nirnroot] = SmokableCategory::Healing;

		s_smokableFormIds.insert(GlowingMushroom);
		s_smokableCategories[GlowingMushroom] = SmokableCategory::Healing;

		s_smokableFormIds.insert(SwampFungalPod);
		s_smokableCategories[SwampFungalPod] = SmokableCategory::Healing;

		s_smokableFormIds.insert(DragonsTongue);
		s_smokableCategories[DragonsTongue] = SmokableCategory::Healing;

		// === MAGIC REGEN ===
		s_smokableFormIds.insert(BlueMountainFlower);
		s_smokableCategories[BlueMountainFlower] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(MoraTapinella);
		s_smokableCategories[MoraTapinella] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(CreepCluster);
		s_smokableCategories[CreepCluster] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(Taproot);
		s_smokableCategories[Taproot] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(ElvesEar);
		s_smokableCategories[ElvesEar] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(GiantLichen);
		s_smokableCategories[GiantLichen] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(FrostSalts);
	 s_smokableCategories[FrostSalts] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(FrostMirriam);
	 s_smokableCategories[FrostMirriam] = SmokableCategory::MagicRegen;

		s_smokableFormIds.insert(Snowberries);
	 s_smokableCategories[Snowberries] = SmokableCategory::MagicRegen;

		// === STAMINA REGEN ===
		s_smokableFormIds.insert(PurpleMountainFlower);
		s_smokableCategories[PurpleMountainFlower] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(HangingMoss);
		s_smokableCategories[HangingMoss] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(JuniperBerries);
	 s_smokableCategories[JuniperBerries] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(Lavender);
		s_smokableCategories[Lavender] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(TundraCotton);
		s_smokableCategories[TundraCotton] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(GrassPod);
		s_smokableCategories[GrassPod] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(FireSalts);
	 s_smokableCategories[FireSalts] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(ScalyPholiota);
	 s_smokableCategories[ScalyPholiota] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(CanisRoot);
	 s_smokableCategories[CanisRoot] = SmokableCategory::StaminaRegen;

		s_smokableFormIds.insert(ThistleBranch);
	 s_smokableCategories[ThistleBranch] = SmokableCategory::StaminaRegen;

		// === RECREATIONAL (Visual effects only - small list) ===
		s_smokableFormIds.insert(FlyAmanita);
		s_smokableCategories[FlyAmanita] = SmokableCategory::Recreational;

		s_smokableFormIds.insert(NamirasRot);
		s_smokableCategories[NamirasRot] = SmokableCategory::Recreational;

		s_smokableFormIds.insert(WhiteCap);
	 s_smokableCategories[WhiteCap] = SmokableCategory::Recreational;

		s_smokableFormIds.insert(BleedingCrown);
	 s_smokableCategories[BleedingCrown] = SmokableCategory::Recreational;

		// === SPECIAL (Save game, slow time, unique effects) ===
		s_smokableFormIds.insert(CrimsonNirnroot);
		s_smokableCategories[CrimsonNirnroot] = SmokableCategory::Special;

		s_smokableFormIds.insert(Nightshade);
		s_smokableCategories[Nightshade] = SmokableCategory::Special;

		s_smokableFormIds.insert(SprigganSap);
		s_smokableCategories[SprigganSap] = SmokableCategory::Special;

		s_smokableFormIds.insert(Deathbell);
		s_smokableCategories[Deathbell] = SmokableCategory::Special;

		// === CANNABIS MOD SUPPORT ===
		// Check if Cannabis.esp is currently loaded
		const char* cannabisEspName = "Cannabis.esp";
		// IDs from Cannabis.esp (assuming load order 05 in provided list, but we use GetFullFormIdMine to resolve).
		// The provided list shows FormID: 05xxxxxx. The base ID is the lower 24 bits (last 6 digits).
		struct CannabisIngredientInfo {
			UInt32 baseId;
			const char* name;
		};

		std::vector<CannabisIngredientInfo> cannabisIngredients = {
			{ 0x01296A, "CannabisDJTreeIngredient" },    // 0501296A
			{ 0x00673A, "CannabisSDMaleTreeIngredient" },// 0500673A
			{ 0x006738, "CannabisPKMaleTreeIngredient" },// 05006738
			{ 0x006736, "CannabisNLMaleTreeIngredient" },// 05006736
			{ 0x006734, "CannabisJHMaleTreeIngredient" },// 05006734
			{ 0x006732, "CannabisDPMaleTreeIngredient" },// 05006732
			{ 0x006731, "CannabisAZMaleTreeIngredient" },// 05006731
			{ 0x006729, "CannabisSDTreeIngredient" },    // 05006729
			{ 0x006727, "CannabisDPTreeIngredient" },    // 05006727
			{ 0x006725, "CannabisAZTreeIngredient" },    // 05006725
			{ 0x006723, "CannabisPKTreeIngredient" },    // 05006723
			{ 0x006722, "CannabisNLTreeIngredient" },    // 05006722
			{ 0x00671C, "CannabisJHTreeIngredient" },    // 0500671C
			{ 0x0013D1, "CannabisElvesEarFermented" }    // 050013D1
		};

		// Try to resolve and add each ingredient
		int addedCannabisCount = 0;
		for (const auto& info : cannabisIngredients)
		{
			UInt32 fullFormId = GetFullFormIdMine(cannabisEspName, info.baseId);
			if (fullFormId != 0)
			{
				s_smokableFormIds.insert(fullFormId);
			 s_cannabisFormIds.insert(fullFormId);
				s_smokableCategories[fullFormId] = SmokableCategory::Recreational;
				addedCannabisCount++;
			}
		}

		if (addedCannabisCount > 0)
		{
			_MESSAGE("[SmokableIngredients] Added %d ingredients from %s", addedCannabisCount, cannabisEspName);
		}

		s_initialized = true;
		_MESSAGE("[SmokableIngredients] Initialized with %d smokable ingredients", s_smokableFormIds.size());
	}

	bool SmokableIngredients::IsSmokable(UInt32 formId)
	{
		if (!s_initialized)
			Initialize();

		return s_smokableFormIds.find(formId) != s_smokableFormIds.end();
	}

	bool SmokableIngredients::IsCannabis(UInt32 formId)
	{
		if (!s_initialized)
			Initialize();

		return s_cannabisFormIds.find(formId) != s_cannabisFormIds.end();
	}

	SmokableCategory SmokableIngredients::GetCategory(UInt32 formId)
	{
		if (!s_initialized)
			Initialize();

		auto it = s_smokableCategories.find(formId);
		if (it != s_smokableCategories.end())
			return it->second;
		
		return SmokableCategory::None;
	}

	const char* SmokableIngredients::GetCategoryName(SmokableCategory category)
	{
		switch (category)
		{
			case SmokableCategory::Healing: return "HEALING";
			case SmokableCategory::MagicRegen: return "MAGIC_REGEN";
			case SmokableCategory::StaminaRegen: return "STAMINA_REGEN";
			case SmokableCategory::Recreational: return "RECREATIONAL";
			case SmokableCategory::Special: return "SPECIAL";
			default: return "NONE";
		}
	}

	const char* SmokableIngredients::GetSmokableName(UInt32 formId)
	{
		using namespace SmokableFormIDs;

		switch (formId)
		{
			// HEALING
			case RedMountainFlower: return "Red Mountain Flower";
			case Blisterwort: return "Blisterwort";
			case ImpStool: return "Imp Stool";
			case Nirnroot: return "Nirnroot";
			case GlowingMushroom: return "Glowing Mushroom";
			case SwampFungalPod: return "Swamp Fungal Pod";
			case DragonsTongue: return "Dragon's Tongue";

			// MAGIC REGEN
			case BlueMountainFlower: return "Blue Mountain Flower";
			case MoraTapinella: return "Mora Tapinella";
			case CreepCluster: return "Creep Cluster";
			case Taproot: return "Taproot";
			case ElvesEar: return "Elves Ear";
			case GiantLichen: return "Giant Lichen";
			case FrostSalts: return "Frost Salts";
			case FrostMirriam: return "Frost Mirriam";
			case Snowberries: return "Snowberries";

			// STAMINA REGEN
			case PurpleMountainFlower: return "Purple Mountain Flower";
			case HangingMoss: return "Hanging Moss";
			case JuniperBerries: return "Juniper Berries";
			case Lavender: return "Lavender";
			case TundraCotton: return "Tundra Cotton";
			case GrassPod: return "Grass Pod";
			case FireSalts: return "Fire Salts";
			case ScalyPholiota: return "Scaly Pholiota";
			case CanisRoot: return "Canis Root";
			case ThistleBranch: return "Thistle Branch";

			// RECREATIONAL
			case FlyAmanita: return "Fly Amanita";
			case NamirasRot: return "Namira's Rot";
			case WhiteCap: return "White Cap";
			case BleedingCrown: return "Bleeding Crown";

			// SPECIAL
			case CrimsonNirnroot: return "Crimson Nirnroot";
			case Nightshade: return "Nightshade";
			case SprigganSap: return "Spriggan Sap";
			case Deathbell: return "Deathbell";

			default: 
			{
				// Check if it's one of the dynamically added Cannabis ingredients
				// Since we can't switch on dynamic IDs easily, we just check category
				auto it = s_smokableCategories.find(formId);
				if (it != s_smokableCategories.end())
				{
					if (it->second == SmokableCategory::Recreational)
					{
						// Return a generic name, or could store names in a map if precision needed
						return "Cannabis (Modded)";
					}
				}
				return "Unknown";
			}
		}
	}
}
