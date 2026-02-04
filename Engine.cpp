#include "Engine.h"
#include "EquipState.h"
#include "VRInputTracker.h"
#include "SmokableIngredients.h"
#include "SmokingMechanics.h"
#include "PipeCrafting.h"
#include "Haptics.h"
#include "config.h"

#include <skse64/PapyrusActor.cpp>
#include <skse64/GameMenus.h>
#include <skse64/GameObjects.h>
#include <skse64/PapyrusVM.h>
#include <skse64/GameInput.h>
#include <cstdlib>
#include <ctime>

namespace InteractivePipeSmokingVR
{
	SKSETrampolineInterface* g_trampolineInterface = nullptr;

	HiggsPluginAPI::IHiggsInterface001* higgsInterface;
	vrikPluginApi::IVrikInterface001* vrikInterface;

	SkyrimVRESLPluginAPI::ISkyrimVRESLInterface001* skyrimVRESLInterface;

	// Near clip distance - will be set from config
	float nearClipDistance = 15.0f;
	
	// Cached original VRIK nearClipDistance for restoration
	float g_originalVrikNearClipDistance = 0.0f;
	bool g_vrikNearClipDistanceCached = false;

	// ESP loaded flag
	bool espLoaded = false;
	
	// Resolved full form IDs - Weapons UNLIT
	UInt32 g_rolledSmokeWeaponFullFormId = 0;
	UInt32 g_herbWoodenPipeWeaponFullFormId = 0; // Was g_emptyPipeWeaponFullFormId
	UInt32 g_herbBonePipeWeaponFullFormId = 0;   // Was g_bonePipeWeaponFullFormId
	UInt32 g_emptyWoodenPipeWeaponFullFormId = 0;
	UInt32 g_emptyBonePipeWeaponFullFormId = 0;

	// Resolved full form IDs - Weapons LIT
	UInt32 g_rolledSmokeLitWeaponFullFormId = 0;
	UInt32 g_woodenPipeLitWeaponFullFormId = 0;  // Was g_pipeLitWeaponFullFormId
	UInt32 g_bonePipeLitWeaponFullFormId = 0;

	// Resolved full form IDs - Rolled Smoke Armor (UNLIT/LIT)
	UInt32 g_smokeUnlitVisualLeftArmorFullFormId = 0;
	UInt32 g_smokeUnlitVisualRightArmorFullFormId = 0;
	UInt32 g_smokeLitVisualLeftArmorFullFormId = 0;
	UInt32 g_smokeLitVisualRightArmorFullFormId = 0;

	// Resolved full form IDs - Wooden Pipe Armor (UNLIT/LIT)
	UInt32 g_herbWoodenPipeUnlitVisualLeftArmorFullFormId = 0;  // Was g_pipeUnlitVisualLeftArmorFullFormId
	UInt32 g_herbWoodenPipeUnlitVisualRightArmorFullFormId = 0; // Was g_pipeUnlitVisualRightArmorFullFormId
	UInt32 g_woodenPipeLitVisualLeftArmorFullFormId = 0;        // Was g_pipeLitVisualLeftArmorFullFormId
	UInt32 g_woodenPipeLitVisualRightArmorFullFormId = 0;       // Was g_pipeLitVisualRightArmorFullFormId

	// Resolved full form IDs - Bone Pipe Armor (UNLIT/LIT)
	UInt32 g_herbBonePipeUnlitVisualLeftArmorFullFormId = 0; // Was g_bonePipeUnlitVisualLeftArmorFullFormId
	UInt32 g_herbBonePipeUnlitVisualRightArmorFullFormId = 0;// Was g_bonePipeUnlitVisualRightArmorFullFormId
	UInt32 g_bonePipeLitVisualLeftArmorFullFormId = 0;
	UInt32 g_bonePipeLitVisualRightArmorFullFormId = 0;

	// Resolved full form IDs - Empty Wooden Pipe Armor (UNLIT only - empty pipes can't be lit)
	UInt32 g_emptyWoodenPipeUnlitVisualLeftArmorFullFormId = 0;
	UInt32 g_emptyWoodenPipeUnlitVisualRightArmorFullFormId = 0;

	// Resolved full form IDs - Empty Bone Pipe Armor (UNLIT only - empty pipes can't be lit)
	UInt32 g_emptyBonePipeUnlitVisualLeftArmorFullFormId = 0;
	UInt32 g_emptyBonePipeUnlitVisualRightArmorFullFormId = 0;

	// Resolved full form IDs - Sound records
	UInt32 g_burningSound1FullFormId = 0;
	UInt32 g_burningSound2FullFormId = 0;

	// Resolved full form IDs - Visual Effect records
	UInt32 g_smokeExhaleFxFullFormId = 0;

	// Track if empty pipe is equipped (for HIGGS grab detection)
	bool g_emptyPipeEquippedLeft = false;
	bool g_emptyPipeEquippedRight = false;
	bool g_emptyBonePipeEquippedLeft = false;
	bool g_emptyBonePipeEquippedRight = false;
	bool g_emptyWoodenPipeEquippedLeft = false;
	bool g_emptyWoodenPipeEquippedRight = false;

	// Track if Roll of Paper is held (for smoke rolling - treated like empty pipe)
	bool g_rollOfPaperHeldLeft = false;
	bool g_rollOfPaperHeldRight = false;

	// Track left-handed mode (for VR controller inversion)
	bool g_lastKnownLeftHandedMode = false;

	// Track if controllers have been touching long enough (for pipe filling)
	bool g_controllersTouchingLongEnough = false;

	// Track if herb pipe hand has been flipped (facing down) long enough
	bool g_herbPipeFlippedLongEnough = false;

	// Track if pipe filling condition was already logged (to avoid spam)
	bool g_pipeFillingConditionLogged = false;

	// Track currently held smokable ingredients (need continuous scale update)
	TESObjectREFR* g_heldSmokableLeft = nullptr;
	TESObjectREFR* g_heldSmokableRight = nullptr;

	// Track the smokable ingredient that was used to fill the pipe (for effects)
	// Separate caches for each pipe type so they can have independent effects
	UInt32 g_filledWoodenPipeSmokableFormId = 0;
	SmokableCategory g_filledWoodenPipeSmokableCategory = SmokableCategory::None;

	UInt32 g_filledBonePipeSmokableFormId = 0;
	SmokableCategory g_filledBonePipeSmokableCategory = SmokableCategory::None;

	UInt32 g_filledRolledSmokeSmokableFormId = 0;
	SmokableCategory g_filledRolledSmokeSmokableCategory = SmokableCategory::None;

	// Active smokable (set when a lit item is equipped, used by smoking mechanics)
	UInt32 g_activeSmokableFormId = 0;
	SmokableCategory g_activeSmokableCategory = SmokableCategory::None;

	// HIGGS MouthRadius tracking
	double g_originalHiggsMouthRadius = 0.0;
	bool g_higgsMouthRadiusCached = false;
	bool g_higgsMouthRadiusModified = false;

	// Left-handed mode tracking
	bool g_leftHandedMode = false;

	void SetNearClipDistance(float distance)
	{
		if (vrikInterface)
		{
			vrikInterface->setSettingDouble("nearClipDistance", static_cast<double>(distance));
			nearClipDistance = distance;
			LOG("Set VRIK nearClipDistance to: %f", distance);
		}
		else
		{
			LOG_ERR("Cannot set nearClipDistance - VRIK interface not available");
		}
	}

	void RestoreVrikSettings()
	{
		if (vrikInterface)
		{
			vrikInterface->restoreSettings();
			LOG("Restored VRIK settings to defaults");
		}
		else
		{
			LOG_ERR("Cannot restore VRIK settings - VRIK interface not available");
		}
	}

	void SetHiggsMouthRadius(double radius)
	{
		if (!higgsInterface)
		{
			_MESSAGE("[HIGGS] Cannot set MouthRadius - interface not available");
			return;
		}

		// Check HIGGS build number - GetSettingDouble/SetSettingDouble requires build 80+
		unsigned int buildNumber = higgsInterface->GetBuildNumber();
		_MESSAGE("[HIGGS] HIGGS build number: %u", buildNumber);
		
		// If build number is too low, these methods may not exist
		if (buildNumber < 80)
		{
			_MESSAGE("[HIGGS] WARNING: HIGGS build %u may not support GetSettingDouble/SetSettingDouble", buildNumber);
			return;
		}

		// Cache the original value if not already cached
		if (!g_higgsMouthRadiusCached)
		{
			double originalRadius = 0.0;
			if (higgsInterface->GetSettingDouble("MouthRadius", originalRadius))
			{
				g_originalHiggsMouthRadius = originalRadius;
				g_higgsMouthRadiusCached = true;
				_MESSAGE("[HIGGS] Cached original MouthRadius: %.2f", g_originalHiggsMouthRadius);
			}
			else
			{
				_MESSAGE("[HIGGS] WARNING: Could not get original MouthRadius");
				return;  // Don't try to set if we can't get the original
			}
		}

		// Set the new radius
		if (higgsInterface->SetSettingDouble("MouthRadius", radius))
		{
			g_higgsMouthRadiusModified = true;
			_MESSAGE("[HIGGS] Set MouthRadius to: %.2f", radius);
		}
		else
		{
			_MESSAGE("[HIGGS] WARNING: Failed to set MouthRadius to: %.2f", radius);
		}
	}

	void RestoreHiggsMouthRadius()
	{
		if (!higgsInterface)
			return;

		if (g_higgsMouthRadiusModified && g_higgsMouthRadiusCached)
		{
			if (higgsInterface->SetSettingDouble("MouthRadius", g_originalHiggsMouthRadius))
			{
				g_higgsMouthRadiusModified = false;
				_MESSAGE("[HIGGS] Restored MouthRadius to original: %.2f", g_originalHiggsMouthRadius);
			}
			else
			{
				_MESSAGE("[HIGGS] WARNING: Failed to restore MouthRadius");
			}
		}
	}

	// ============================================
	// PLAY SOUND AT PLAYER LOCATION
	// Uses TESSound and PlaySoundEffect (Papyrus native function)
	// ============================================

	// PlaySoundEffect function signature - based on Papyrus Sound.Play native
	typedef void(*_PlaySoundEffect)(VMClassRegistry* VMinternal, UInt32 stackId, TESSound* sound, TESObjectREFR* source);
	static RelocAddr<_PlaySoundEffect> PlaySoundEffect(0x009EF150);

	// VisualEffect_Play function signature - based on Papyrus VisualEffect.Play native
	// Address from WeaponThrowVR: 0x9A4E00
	RelocAddr<_VisualEffect_Play> VisualEffect_Play(0x9A4E00);

	// VisualEffect_Stop function signature - based on Papyrus VisualEffect.Stop native
	typedef void(*_VisualEffect_Stop)(VMClassRegistry* registry, UInt32 stackId, BGSReferenceEffect* effect, TESObjectREFR* target);
	static RelocAddr<_VisualEffect_Stop> VisualEffect_Stop(0x9A4F80);

	// Track if burning sound is currently playing
	static bool g_burningSoundPlaying = false;

	void PlaySoundAtPlayer(UInt32 soundFormID)
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		// Look up the sound form (SOUN type)
		TESForm* form = LookupFormByID(soundFormID);
		if (!form)
			return;

		// Cast to TESSound (SOUN record)
		TESSound* sound = DYNAMIC_CAST(form, TESForm, TESSound);
		if (!sound)
			return;

		// Play the sound using the Papyrus native function
		PlaySoundEffect((*g_skyrimVM)->GetClassRegistry(), 0, sound, player);
	}

	void PlaySmokeExhaleEffect()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		if (g_smokeExhaleFxFullFormId == 0)
			return;

		// Look up the visual effect form (RFCT type)
		TESForm* form = LookupFormByID(g_smokeExhaleFxFullFormId);
		if (!form)
			return;

		// Cast to BGSReferenceEffect (RFCT record)
		BGSReferenceEffect* effect = DYNAMIC_CAST(form, TESForm, BGSReferenceEffect);
		if (!effect)
			return;

		// Dynamically set the "Attach to Camera" flag so the effect follows VR HMD rotation
		// Flag bits: kFaceTarget = 1, kAttachToCamera = 2, kInheritRotation = 4
		effect->data.flags = static_cast<BGSReferenceEffect::Flag>(

			static_cast<UInt32>(effect->data.flags) | 0x02  // Set kAttachToCamera bit
		);

		// Play the visual effect using the Papyrus native function
		VisualEffect_Play((*g_skyrimVM)->GetClassRegistry(), 0, effect, player, -1.0f, player);
		_MESSAGE("[SmokeExhale] Playing smoke exhale effect");
	}

	void StopSmokeExhaleEffect()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		if (g_smokeExhaleFxFullFormId == 0)
			return;

		// Look up the visual effect form (RFCT type)
		TESForm* form = LookupFormByID(g_smokeExhaleFxFullFormId);
		if (!form)
			return;

		// Cast to BGSReferenceEffect (RFCT record)
		BGSReferenceEffect* effect = DYNAMIC_CAST(form, TESForm, BGSReferenceEffect);
		if (!effect)
			return;

		// Stop the visual effect using the Papyrus native function
		VisualEffect_Stop((*g_skyrimVM)->GetClassRegistry(), 0, effect, player);
		_MESSAGE("[SmokeExhale] Stopped smoke exhale effect");
	}

	void PlayRandomBurningSound()
	{
		if (g_burningSoundPlaying)
			return; // Already playing

		// Randomly select one of the two burning sounds
		UInt32 soundFormId = 0;
		if (g_burningSound1FullFormId != 0 && g_burningSound2FullFormId != 0)
		{
			// Use simple rand() to pick one (0 or 1)
			static bool seeded = false;
			if (!seeded)
			{
				srand(static_cast<unsigned int>(time(nullptr)));
				seeded = true;
			}
			int selection = rand() % 2;
			soundFormId = (selection == 0) ? g_burningSound1FullFormId : g_burningSound2FullFormId;
			_MESSAGE("[Sound] Randomly selected burning sound %d (FormID: %08X)", selection + 1, soundFormId);
		}
		else if (g_burningSound1FullFormId != 0)
		{
			soundFormId = g_burningSound1FullFormId;
		}
		else if (g_burningSound2FullFormId != 0)
		{
			soundFormId = g_burningSound2FullFormId;
		}

		if (soundFormId != 0)
		{
			PlaySoundAtPlayer(soundFormId);
			g_burningSoundPlaying = true;
		}
		else
		{
			_MESSAGE("[Sound] WARNING: No burning sound form IDs resolved!");
		}
	}

	void StopBurningSound()
	{
		// Note: The PlaySoundEffect Papyrus native doesn't return a handle to stop the sound.
		// The sound will naturally stop when it finishes playing.
		// For looping sounds, we just reset our tracking flag here.
		// A proper implementation would use BSSoundHandle to stop the sound.
		if (g_burningSoundPlaying)
		{
			g_burningSoundPlaying = false;
			_MESSAGE("[Sound] Burning sound tracking reset");
		}
	}

	bool LoadAndLogESP()
	{
		const ModInfo* modInfo = NEWLookupAllLoadedModByName(ESP_NAME);
		
		if (modInfo)
		{
			_MESSAGE("ESP loaded: %s (index %02X)", ESP_NAME, modInfo->modIndex);
			espLoaded = true;
			return true;
		}
		else
		{
			_MESSAGE("ESP NOT FOUND: %s", ESP_NAME);
			espLoaded = false;
			return false;
		}
	}

	void LogAllESPRecords()
	{
		if (!espLoaded)
		{
			_MESSAGE("Cannot resolve ESP records - ESP not loaded");
			return;
		}

		int resolved = 0;
		int failed = 0;

		// Silent resolve helper - only counts, no logging per item
		auto Resolve = [&resolved, &failed](UInt32 baseId, UInt32& outFullId)
		{
			if (baseId == 0)
				return;

			outFullId = GetFullFormIdFromEspAndFormId(ESP_NAME, baseId);
			if (outFullId == 0 || !LookupFormByID(outFullId))
			{
				failed++;
				return;
			}
			resolved++;
		};

		// Weapons UNLIT
		Resolve(ROLLED_SMOKE_WEAPON_BASE_FORMID, g_rolledSmokeWeaponFullFormId);
		Resolve(HERB_WOODEN_PIPE_WEAPON_BASE_FORMID, g_herbWoodenPipeWeaponFullFormId);
		Resolve(HERB_BONE_PIPE_WEAPON_BASE_FORMID, g_herbBonePipeWeaponFullFormId);
		Resolve(EMPTY_WOODEN_PIPE_WEAPON_BASE_FORMID, g_emptyWoodenPipeWeaponFullFormId);
		Resolve(EMPTY_BONE_PIPE_WEAPON_BASE_FORMID, g_emptyBonePipeWeaponFullFormId);

		// Weapons LIT
		Resolve(ROLLED_SMOKE_LIT_WEAPON_BASE_FORMID, g_rolledSmokeLitWeaponFullFormId);
		Resolve(WOODEN_PIPE_LIT_WEAPON_BASE_FORMID, g_woodenPipeLitWeaponFullFormId);
		Resolve(BONE_PIPE_LIT_WEAPON_BASE_FORMID, g_bonePipeLitWeaponFullFormId);

		// Rolled Smoke Armor
		Resolve(SMOKE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_smokeUnlitVisualLeftArmorFullFormId);
		Resolve(SMOKE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_smokeUnlitVisualRightArmorFullFormId);
		Resolve(SMOKE_LIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_smokeLitVisualLeftArmorFullFormId);
		Resolve(SMOKE_LIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_smokeLitVisualRightArmorFullFormId);

		// Wooden Pipe / Herb Pipe Armor
		Resolve(HERB_WOODEN_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_herbWoodenPipeUnlitVisualLeftArmorFullFormId);
		Resolve(HERB_WOODEN_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_herbWoodenPipeUnlitVisualRightArmorFullFormId);
		Resolve(WOODEN_PIPE_LIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_woodenPipeLitVisualLeftArmorFullFormId);
		Resolve(WOODEN_PIPE_LIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_woodenPipeLitVisualRightArmorFullFormId);

		// Bone Pipe / Herb Bone Pipe Armor
		Resolve(HERB_BONE_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_herbBonePipeUnlitVisualLeftArmorFullFormId);
		Resolve(HERB_BONE_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_herbBonePipeUnlitVisualRightArmorFullFormId);
		Resolve(BONE_PIPE_LIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_bonePipeLitVisualLeftArmorFullFormId);
		Resolve(BONE_PIPE_LIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_bonePipeLitVisualRightArmorFullFormId);

		// Empty Wooden Pipe Armor
		Resolve(EMPTY_WOODEN_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_emptyWoodenPipeUnlitVisualLeftArmorFullFormId);
		Resolve(EMPTY_WOODEN_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_emptyWoodenPipeUnlitVisualRightArmorFullFormId);

		// Empty Bone Pipe Armor
		Resolve(EMPTY_BONE_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID, g_emptyBonePipeUnlitVisualLeftArmorFullFormId);
		Resolve(EMPTY_BONE_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID, g_emptyBonePipeUnlitVisualRightArmorFullFormId);

		// Sound Records
		Resolve(BURNING_SOUND_1_BASE_FORMID, g_burningSound1FullFormId);
		Resolve(BURNING_SOUND_2_BASE_FORMID, g_burningSound2FullFormId);

		// Visual Effect Records
		Resolve(SMOKE_EXHALE_FX_BASE_FORMID, g_smokeExhaleFxFullFormId);

		// Log summary
		_MESSAGE("ESP forms resolved: %d OK, %d failed", resolved, failed);
	}

	// UNLIT weapon checks
	bool IsRolledSmokeWeapon(UInt32 formId)
	{
		return (formId == ROLLED_SMOKE_WEAPON_BASE_FORMID) ||
			(g_rolledSmokeWeaponFullFormId != 0 && formId == g_rolledSmokeWeaponFullFormId);
	}

	bool IsHerbWoodenPipeWeapon(UInt32 formId)
	{
		return (formId == HERB_WOODEN_PIPE_WEAPON_BASE_FORMID) ||
			(g_herbWoodenPipeWeaponFullFormId != 0 && formId == g_herbWoodenPipeWeaponFullFormId);
	}

	bool IsHerbBonePipeWeapon(UInt32 formId)
	{
		return (formId == HERB_BONE_PIPE_WEAPON_BASE_FORMID) ||
			(g_herbBonePipeWeaponFullFormId != 0 && formId == g_herbBonePipeWeaponFullFormId);
	}

	bool IsEmptyWoodenPipeWeapon(UInt32 formId)
	{
		return (formId == EMPTY_WOODEN_PIPE_WEAPON_BASE_FORMID) ||
			(g_emptyWoodenPipeWeaponFullFormId != 0 && formId == g_emptyWoodenPipeWeaponFullFormId);
	}

	bool IsEmptyBonePipeWeapon(UInt32 formId)
	{
		return (formId == EMPTY_BONE_PIPE_WEAPON_BASE_FORMID) ||
			(g_emptyBonePipeWeaponFullFormId != 0 && formId == g_emptyBonePipeWeaponFullFormId);
	}

	// LIT weapon checks
	bool IsRolledSmokeLitWeapon(UInt32 formId)
	{
		return (formId == ROLLED_SMOKE_LIT_WEAPON_BASE_FORMID) ||
			(g_rolledSmokeLitWeaponFullFormId != 0 && formId == g_rolledSmokeLitWeaponFullFormId);
	}

	bool IsWoodenPipeLitWeapon(UInt32 formId)
	{
		return (formId == WOODEN_PIPE_LIT_WEAPON_BASE_FORMID) ||
			(g_woodenPipeLitWeaponFullFormId != 0 && formId == g_woodenPipeLitWeaponFullFormId);
	}

	bool IsBonePipeLitWeapon(UInt32 formId)
	{
		return (formId == BONE_PIPE_LIT_WEAPON_BASE_FORMID) ||
			(g_bonePipeLitWeaponFullFormId != 0 && formId == g_bonePipeLitWeaponFullFormId);
	}

	// ============================================
	// Equip Event Handling
	// ============================================
	
	PipeEquipEventSink* PipeEquipEventSink::GetSingleton()
	{
		static PipeEquipEventSink singleton;
		return &singleton;
	}

	EventResult PipeEquipEventSink::ReceiveEvent(TESEquipEvent* evn, EventDispatcher<TESEquipEvent>* dispatcher)
	{
		if (!evn || !evn->actor)
			return kEvent_Continue;

		// Only care about player equip events
		if (evn->actor != *g_thePlayer)
			return kEvent_Continue;

		// Always log player equip events for debugging
		_MESSAGE("[EquipEvent] baseObject=%08X equipped=%d uniqueID=%u", evn->baseObject, evn->equipped ? 1 : 0, evn->uniqueID);

		// Note: Knife/dagger tracking has been removed - knives are now tracked via HIGGS grab callbacks
		// The OnWeaponEquipStateChanged function is now a stub

		// Debug: Log our expected form IDs for comparison
		_MESSAGE("[EquipEvent DEBUG] Expected HerbWoodenPipe: base=%08X full=%08X", HERB_WOODEN_PIPE_WEAPON_BASE_FORMID, g_herbWoodenPipeWeaponFullFormId);
		_MESSAGE("[EquipEvent DEBUG] Expected HerbBonePipe: base=%08X full=%08X", HERB_BONE_PIPE_WEAPON_BASE_FORMID, g_herbBonePipeWeaponFullFormId);

		// Check UNLIT weapons
		const bool isRolledSmoke = IsRolledSmokeWeapon(evn->baseObject);
		const bool isHerbWoodenPipe = IsHerbWoodenPipeWeapon(evn->baseObject);
		const bool isHerbBonePipe = IsHerbBonePipeWeapon(evn->baseObject);
		const bool isEmptyWoodenPipe = IsEmptyWoodenPipeWeapon(evn->baseObject);
		const bool isEmptyBonePipe = IsEmptyBonePipeWeapon(evn->baseObject);

		// Check LIT weapons
		const bool isRolledSmokeLit = IsRolledSmokeLitWeapon(evn->baseObject);
		const bool isWoodenPipeLit = IsWoodenPipeLitWeapon(evn->baseObject);
		const bool isBonePipeLit = IsBonePipeLitWeapon(evn->baseObject);

		// Debug: Log which checks passed
		_MESSAGE("[EquipEvent DEBUG] isHerbWoodenPipe=%d isHerbBonePipe=%d", isHerbWoodenPipe ? 1 : 0, isHerbBonePipe ? 1 : 0);

		// If not any of our items, skip
		if (!isRolledSmoke && !isHerbWoodenPipe && !isHerbBonePipe && !isEmptyWoodenPipe && !isEmptyBonePipe &&
			!isRolledSmokeLit && !isWoodenPipeLit && !isBonePipeLit)
			return kEvent_Continue;

		Actor* player = *g_thePlayer;
		if (!player)
			return kEvent_Continue;

		// Determine which hand the item is equipped to
		bool inLeft = false;
		bool inRight = false;
		if (TESForm* left = player->GetEquippedObject(true))
		{
			inLeft = (left->formID == evn->baseObject) || 
				(left->formID == g_rolledSmokeWeaponFullFormId) || 
				(left->formID == g_herbWoodenPipeWeaponFullFormId) ||
				(left->formID == g_herbBonePipeWeaponFullFormId) ||
				(left->formID == g_emptyWoodenPipeWeaponFullFormId) ||
				(left->formID == g_emptyBonePipeWeaponFullFormId) ||
				(left->formID == g_rolledSmokeLitWeaponFullFormId) ||
				(left->formID == g_woodenPipeLitWeaponFullFormId) ||
				(left->formID == g_bonePipeLitWeaponFullFormId);
		}
		if (TESForm* right = player->GetEquippedObject(false))
		{
			inRight = (right->formID == evn->baseObject) || 
				(right->formID == g_rolledSmokeWeaponFullFormId) || 
				(right->formID == g_herbWoodenPipeWeaponFullFormId) ||
				(right->formID == g_herbBonePipeWeaponFullFormId) ||
				(right->formID == g_emptyWoodenPipeWeaponFullFormId) ||
				(right->formID == g_emptyBonePipeWeaponFullFormId) ||
				(right->formID == g_rolledSmokeLitWeaponFullFormId) ||
				(right->formID == g_woodenPipeLitWeaponFullFormId) ||
				(right->formID == g_bonePipeLitWeaponFullFormId);
		}

		// Determine product type for logging
		const char* productType = "Unknown";
		if (isRolledSmoke) productType = "RolledSmoke";
		else if (isHerbWoodenPipe) productType = "HerbWoodenPipe";
		else if (isHerbBonePipe) productType = "HerbBonePipe";
		else if (isEmptyWoodenPipe) productType = "EmptyWoodenPipe";
		else if (isEmptyBonePipe) productType = "EmptyBonePipe";
		else if (isRolledSmokeLit) productType = "RolledSmokeLit";
		else if (isWoodenPipeLit) productType = "WoodenPipeLit";
		else if (isBonePipeLit) productType = "BonePipeLit";
		
		
		

		_MESSAGE("[EquipEvent] matched=%s handL=%d handR=%d", productType, inLeft ? 1 : 0, inRight ? 1 : 0);

		if (g_equipStateManager)
		{
			g_equipStateManager->OnDummyWeaponEquipEvent(*evn, inLeft, inRight);
		}
		else
		{
			_MESSAGE("[EquipEvent] EquipStateManager is null");
		}

		return kEvent_Continue;
	}

	void RegisterEquipEventSink()
	{
		EventDispatcherList* eventDispatcherList = GetEventDispatcherList();
		if (eventDispatcherList)
		{
			eventDispatcherList->unk4D0.AddEventSink(PipeEquipEventSink::GetSingleton());
			_MESSAGE("Registered PipeEquipEventSink");
		}
		else
		{
			_MESSAGE("ERROR: Could not get EventDispatcherList");
		}
	}

	void UnregisterEquipEventSink()
	{
		EventDispatcherList* eventDispatcherList = GetEventDispatcherList();
		if (eventDispatcherList)
		{
			eventDispatcherList->unk4D0.RemoveEventSink(PipeEquipEventSink::GetSingleton());
			_MESSAGE("Unregistered PipeEquipEventSink");
		}
	}

	// ============================================
	// Menu Open/Close Event Handling
	// ============================================

	MenuOpenCloseEventSink* MenuOpenCloseEventSink::GetSingleton()
	{
		static MenuOpenCloseEventSink singleton;
		return &singleton;
	}

	// Check if the menu is a pause-type menu that should pause VR tracking
	static bool IsPauseMenu(const char* menuName)
	{
		if (!menuName)
			return false;

		// List of menus that should pause VR input tracking
		// These are menus that pause the game or where the player isn't actively playing
		static const char* pauseMenus[] = {
			"Journal Menu",         // Main pause menu (ESC)
			"InventoryMenu",     // Inventory
			"MagicMenu",   // Magic/Powers menu
			"MapMenu",        // Map
			"StatsMenu",   // Skills/Perks
			"ContainerMenu",        // Looting containers
			"BarterMenu",           // Trading
			"GiftMenu",   // Gifting
			"Book Menu",   // Reading books
			"Lockpicking Menu",     // Lockpicking
			"Sleep/Wait Menu",      // Sleep/Wait
			"LevelUp Menu",         // Level up
			"Training Menu",        // Training
			"RaceSex Menu",  // Character creation
			"Crafting Menu",        // Smithing/Alchemy/Enchanting
			"FavoritesMenu", // Favorites
			"Loading Menu",         // Loading screen
			"Main Menu",       // Main menu
			"Console",   // Console
			"TweenMenu",// Tween menu (world-space menu background)
			"MessageBoxMenu",  // Message boxes
			nullptr
		};

		for (int i = 0; pauseMenus[i] != nullptr; ++i)
		{
			if (_stricmp(menuName, pauseMenus[i]) == 0)
				return true;
		}

		return false;
	}

	EventResult MenuOpenCloseEventSink::ReceiveEvent(MenuOpenCloseEvent* evn, EventDispatcher<MenuOpenCloseEvent>* dispatcher)
	{
		if (!evn)
			return kEvent_Continue;

		const char* menuName = evn->menuName.data;
		bool isOpening = evn->opening;

		// Check if this is a pause-type menu
		if (IsPauseMenu(menuName))
		{
			if (isOpening)
			{
				// Menu opening - pause VR input tracking
				if (g_vrInputTracker && g_vrInputTracker->IsTracking())
				{
					g_vrInputTracker->PauseTracking();
					_MESSAGE("[Menu] Pause menu '%s' opened - VR input tracking PAUSED", menuName);
				}
			}
			else
			{
				// Menu closing - check if any other pause menus are still open
				// Only resume tracking if no pause menus remain open
				MenuManager* mm = MenuManager::GetSingleton();
				bool anyPauseMenuOpen = false;

				if (mm)
				{
					// Check all known pause menus
					static const char* pauseMenusToCheck[] = {
						"Journal Menu", "InventoryMenu", "MagicMenu", "MapMenu",
						"StatsMenu", "ContainerMenu", "BarterMenu", "GiftMenu",
						"Book Menu", "Lockpicking Menu", "Sleep/Wait Menu",
						"LevelUp Menu", "Training Menu", "RaceSex Menu",
						"Crafting Menu", "FavoritesMenu", "Loading Menu",
						"Main Menu", "Console", "TweenMenu", "MessageBoxMenu",
						nullptr
					};

					for (int i = 0; pauseMenusToCheck[i] != nullptr; ++i)
					{
						// Skip the menu that's currently closing
						if (_stricmp(pauseMenusToCheck[i], menuName) == 0)
							continue;

						BSFixedString checkMenuName(pauseMenusToCheck[i]);
						if (mm->IsMenuOpen(&checkMenuName))
						{
							anyPauseMenuOpen = true;
							break;
						}
					}
				}

				if (!anyPauseMenuOpen)
				{
					// No pause menus open - resume VR input tracking
					if (g_vrInputTracker && g_vrInputTracker->IsPaused())
					{
						g_vrInputTracker->ResumeTracking();
						_MESSAGE("[Menu] Pause menu '%s' closed (no other pause menus open) - VR input tracking RESUMED", menuName);
					}
				}
				else
				{
					_MESSAGE("[Menu] Pause menu '%s' closed but other pause menus still open - tracking remains paused", menuName);
				}
			}
		}

		// Reload config on any menu close (silently)
		if (!isOpening)
		{
			loadConfig();
		}

		return kEvent_Continue;
	}

	void RegisterMenuEventSink()
	{
		MenuManager* menuManager = MenuManager::GetSingleton();
		if (menuManager)
		{
			menuManager->MenuOpenCloseEventDispatcher()->AddEventSink(MenuOpenCloseEventSink::GetSingleton());
			_MESSAGE("Registered MenuOpenCloseEventSink");
		}
		else
		{
			_MESSAGE("ERROR: Could not get MenuManager");
		}
	}

	void UnregisterMenuEventSink()
	{
		MenuManager* menuManager = MenuManager::GetSingleton();
		if (menuManager)
		{
			menuManager->MenuOpenCloseEventDispatcher()->RemoveEventSink(MenuOpenCloseEventSink::GetSingleton());
			_MESSAGE("Unregistered MenuOpenCloseEventSink");
		}
	}

	// ============================================
	// Actor Death Event Handling
	// ============================================

	ActorDeathEventSink* ActorDeathEventSink::GetSingleton()
	{
		static ActorDeathEventSink singleton;
		return &singleton;
	}

	EventResult ActorDeathEventSink::ReceiveEvent(TESDeathEvent* evn, EventDispatcher<TESDeathEvent>* dispatcher)
	{
		if (!evn)
			return kEvent_Continue;

		// Check if this is the player dying
		if (evn->source && evn->source == *g_thePlayer)
		{
			_MESSAGE("[DeathEvent] Player died - resetting mod state");
			ResetModState();
		}

		return kEvent_Continue;
	}

	void RegisterDeathEventSink()
	{
		EventDispatcherList* eventDispatcherList = GetEventDispatcherList();
		if (eventDispatcherList)
		{
			eventDispatcherList->deathDispatcher.AddEventSink(ActorDeathEventSink::GetSingleton());
			_MESSAGE("Registered ActorDeathEventSink");
		}
		else
		{
			_MESSAGE("ERROR: Could not get EventDispatcherList for death events");
		}
	}

	void UnregisterDeathEventSink()
	{
		EventDispatcherList* eventDispatcherList = GetEventDispatcherList();
		if (eventDispatcherList)
		{
			eventDispatcherList->deathDispatcher.RemoveEventSink(ActorDeathEventSink::GetSingleton());
			_MESSAGE("Unregistered ActorDeathEventSink");
		}
	}

	// ============================================
	// HIGGS Grab Event Handling
	// ============================================

	// Static callback function for HIGGS grabbed event
	void HiggsGrabbedCallback(bool isLeft, TESObjectREFR* grabbedRefr)
	{
		OnHiggsGrabbed(isLeft, grabbedRefr);
	}

	// Static callback function for HIGGS dropped event
	void HiggsDroppedCallback(bool isLeft, TESObjectREFR* droppedRefr)
	{
		OnHiggsDropped(isLeft, droppedRefr);
	}

	// Static callback function for HIGGS consumed event (item dropped at mouth)
	void HiggsConsumedCallback(bool isLeft, TESForm* consumedForm)
	{
		OnHiggsConsumed(isLeft, consumedForm);
	}

	// Static callback for post-VRIK post-HIGGS update (runs after HIGGS processes)
	void PostVrikPostHiggsCallback()
	{
		// Check if left-handed mode changed
		CheckAndLogLeftHandedMode(false);

		// Apply scale AFTER HIGGS has done its updates
		UpdateHeldSmokableScale();

		// Apply scale for crafting items (when knife is equipped)
		UpdateHeldCraftingItemScale();

		// Continuously check pipe filling condition while holding a smokable
		CheckPipeFillingCondition();
	}

	void CheckPipeFillingCondition()
	{
		// Only check if we're holding a smokable ingredient
		bool holdingSmokableLeft = (g_heldSmokableLeft != nullptr);
		bool holdingSmokableRight = (g_heldSmokableRight != nullptr);
		bool holdingSmokable = holdingSmokableLeft || holdingSmokableRight;
		
		if (!holdingSmokable)
		{
			g_pipeFillingConditionLogged = false;
			return;
		}

		// Check if any empty pipe is equipped and determine which hand
		bool emptyPipeInLeft = g_emptyPipeEquippedLeft || g_emptyBonePipeEquippedLeft || g_emptyWoodenPipeEquippedLeft;
		bool emptyPipeInRight = g_emptyPipeEquippedRight || g_emptyBonePipeEquippedRight || g_emptyWoodenPipeEquippedRight;
		bool anyEmptyPipeEquipped = emptyPipeInLeft || emptyPipeInRight;

		if (!anyEmptyPipeEquipped)
		{
			g_pipeFillingConditionLogged = false;
			return;
		}

		// Check if controllers are near enough for pipe filling (uses separate radius)
		bool controllersNearForFilling = g_vrInputTracker && g_vrInputTracker->AreControllersNearForPipeFilling();
		
		if (controllersNearForFilling)
		{
			// All conditions met - trigger continuous weak haptic feedback on pipe hand
			// This signals "ready to fill" - player must DROP the smokable to actually fill
			TriggerHapticPulse(emptyPipeInLeft, emptyPipeInRight, 0.08f); // Weak continuous feedback

			if (!g_pipeFillingConditionLogged)
			{
				g_pipeFillingConditionLogged = true;

				// Get smokable info for logging
				TESObjectREFR* heldSmokable = g_heldSmokableLeft ? g_heldSmokableLeft : g_heldSmokableRight;
				const char* smokableName = "Unknown";
				UInt32 smokableFormId = 0;
				SmokableCategory smokableCategory = SmokableCategory::None;

				if (heldSmokable && heldSmokable->baseForm)
				{
					smokableFormId = heldSmokable->baseForm->formID;
					smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
					smokableCategory = SmokableIngredients::GetCategory(smokableFormId);
				}

				_MESSAGE("[PipeFill] *** READY TO FILL - DROP SMOKABLE TO FILL PIPE ***");
				_MESSAGE("[PipeFill]   -> Empty pipe equipped: %s hand", emptyPipeInLeft ? "LEFT" : "RIGHT");
				_MESSAGE("[PipeFill]   -> Smokable held: '%s' (FormID: %08X)", smokableName, smokableFormId);
				_MESSAGE("[PipeFill]   -> Smokable category: %s", SmokableIngredients::GetCategoryName(smokableCategory));
				_MESSAGE("[PipeFill]   -> Controllers near for filling: YES");
				_MESSAGE("[PipeFill]   -> Continuous haptic feedback started - DROP to fill!");
			}
		}
		else
		{
			// Conditions no longer met - reset so we can log again when they re-enter the zone
			if (g_pipeFillingConditionLogged)
			{
				_MESSAGE("[PipeFill] Left ready-to-fill zone - haptic feedback stopped");
			}
			g_pipeFillingConditionLogged = false;
		}
	}

	void RegisterHiggsGrabCallback()
	{
		if (higgsInterface)
		{
			higgsInterface->AddGrabbedCallback(HiggsGrabbedCallback);
			higgsInterface->AddDroppedCallback(HiggsDroppedCallback);
			higgsInterface->AddConsumedCallback(HiggsConsumedCallback);
			higgsInterface->AddPostVrikPostHiggsCallback(PostVrikPostHiggsCallback);
			_MESSAGE("Registered HIGGS grabbed, dropped, consumed, and post-update callbacks");
		}
		else
		{
			_MESSAGE("WARNING: Cannot register HIGGS callbacks - interface not available");
		}
	}

	// Helper to check if a reference is still valid
	static bool IsRefrValid(TESObjectREFR* refr)
	{
		if (!refr)
			return false;
		
		// Check for deleted flag
		if (refr->flags & TESForm::kFlagIsDeleted)
			return false;
		
		// Check formID is valid
		if (refr->formID == 0)
			return false;

		return true;
	}

	void ShrinkSmokableIngredient(TESObjectREFR* refr, float targetScale)
	{
		if (!IsRefrValid(refr))
			return;

		// Get the 3D root node for the reference
		NiNode* rootNode = refr->GetNiNode();
		if (!rootNode)
		{
			return;
		}

		// Set the scale on the root node's local transform
		rootNode->m_localTransform.scale = targetScale;

		// Also set world transform scale
		rootNode->m_worldTransform.scale = targetScale;

		// Recursively set scale on all child nodes as well
		for (UInt32 i = 0; i < rootNode->m_children.m_size; ++i)
		{
			NiAVObject* child = rootNode->m_children.m_data[i];
			if (child)
			{
				child->m_localTransform.scale = targetScale;
				child->m_worldTransform.scale = targetScale;
			}
		}

		// Force update of world transforms
		NiAVObject::ControllerUpdateContext ctx;
		ctx.flags = 0;
		ctx.delta = 0;
		rootNode->UpdateWorldData(&ctx);
	}

	void UpdateHeldSmokableScale()
	{
		// Validate and apply scale to held smokables using config value
		if (g_heldSmokableLeft)
		{
			// Check if reference is still valid before accessing
			if (!IsRefrValid(g_heldSmokableLeft))
			{
				_MESSAGE("[UpdateHeldSmokableScale] Left held smokable is invalid, clearing");
				g_heldSmokableLeft = nullptr;
			}
			else
			{
				float targetScale = configSmokableGrabbedScale;
				if (g_heldSmokableLeft->baseForm && SmokableIngredients::IsCannabis(g_heldSmokableLeft->baseForm->formID))
				{
					targetScale = 1.0f;
				}
				ShrinkSmokableIngredient(g_heldSmokableLeft, targetScale);
			}
		}
		if (g_heldSmokableRight)
		{
			// Check if reference is still valid before accessing
			if (!IsRefrValid(g_heldSmokableRight))
			{
				_MESSAGE("[UpdateHeldSmokableScale] Right held smokable is invalid, clearing");
				g_heldSmokableRight = nullptr;
			}
			else
			{
				float targetScale = configSmokableGrabbedScale;
				if (g_heldSmokableRight->baseForm && SmokableIngredients::IsCannabis(g_heldSmokableRight->baseForm->formID))
				{
					targetScale = 1.0f;
				}
				ShrinkSmokableIngredient(g_heldSmokableRight, targetScale);
			}
		}
	}

	void OnHiggsDropped(bool isLeft, TESObjectREFR* droppedRefr)
	{
		// Check if we're dropping a smokable while in the "ready to fill" state
		bool wasHoldingSmokable = false;
		TESObjectREFR* droppedSmokable = nullptr;

		if (isLeft && g_heldSmokableLeft != nullptr)
		{
			droppedSmokable = g_heldSmokableLeft;
			wasHoldingSmokable = true;
		}
		else if (!isLeft && g_heldSmokableRight != nullptr)
		{
			droppedSmokable = g_heldSmokableRight;
			wasHoldingSmokable = true;
		}

		// Check pipe fill condition: dropping smokable while controllers touching + empty pipe equipped
		if (wasHoldingSmokable && droppedSmokable)
		{
			bool emptyPipeInLeft = g_emptyPipeEquippedLeft || g_emptyBonePipeEquippedLeft || g_emptyWoodenPipeEquippedLeft;
			bool emptyPipeInRight = g_emptyPipeEquippedRight || g_emptyBonePipeEquippedRight || g_emptyWoodenPipeEquippedRight;
			bool anyEmptyPipeEquipped = emptyPipeInLeft || emptyPipeInRight;

			bool hasRollOfPaperLeft = (g_heldRollOfPaperLeft != nullptr);
			bool hasRollOfPaperRight = (g_heldRollOfPaperRight != nullptr);
			bool holdingRollOfPaper = hasRollOfPaperLeft || hasRollOfPaperRight;

			bool controllersNearForFilling = g_vrInputTracker && g_vrInputTracker->AreControllersNearForPipeFilling();
			bool controllersNearForSmokeRolling = g_vrInputTracker && g_vrInputTracker->AreControllersNearForSmokeRolling();

			// Debug logging
			_MESSAGE("[HIGGS Drop DEBUG] emptyPipeInLeft=%d emptyPipeInRight=%d anyEmptyPipeEquipped=%d controllersNearForFilling=%d",
				emptyPipeInLeft ? 1 : 0, emptyPipeInRight ? 1 : 0, anyEmptyPipeEquipped ? 1 : 0, 
				controllersNearForFilling ? 1 : 0);
			_MESSAGE("[HIGGS Drop DEBUG] holdingRollOfPaper=%d controllersNearForSmokeRolling=%d",
				holdingRollOfPaper ? 1 : 0, controllersNearForSmokeRolling ? 1 : 0);

			// Pipe fills when: empty pipe equipped AND controllers are near enough (uses pipe filling radius)
			if (anyEmptyPipeEquipped && controllersNearForFilling)
			{
				// PIPE FILL - scale to 0 to hide the ingredient
				const char* smokableName = "Unknown";
				UInt32 smokableFormId = 0;
				SmokableCategory smokableCategory = SmokableCategory::None;

				if (droppedSmokable->baseForm)
				{
					smokableFormId = droppedSmokable->baseForm->formID;
					smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
					smokableCategory = SmokableIngredients::GetCategory(smokableFormId);
				}

				_MESSAGE("[PipeFill] *** PIPE FILLED! ***");
				_MESSAGE("[PipeFill]   -> Empty pipe equipped: %s hand", emptyPipeInLeft ? "LEFT" : "RIGHT");
				_MESSAGE("[PipeFill]   -> Smokable used: '%s' (FormID: %08X)", smokableName, smokableFormId);
				_MESSAGE("[PipeFill]   -> Smokable category: %s", SmokableIngredients::GetCategoryName(smokableCategory));

				// Store the smokable ingredient info in the correct type-specific cache
				// Wooden pipe and bone pipe have separate caches so they can have different effects
				if (g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight)
				{
					g_filledWoodenPipeSmokableFormId = smokableFormId;
					g_filledWoodenPipeSmokableCategory = smokableCategory;
					_MESSAGE("[PipeFill]   -> Stored in WOODEN PIPE cache: '%s' (%s)", smokableName, SmokableIngredients::GetCategoryName(smokableCategory));
				}
				else if (g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight)
				{
					g_filledBonePipeSmokableFormId = smokableFormId;
					g_filledBonePipeSmokableCategory = smokableCategory;
					_MESSAGE("[PipeFill]   -> Stored in BONE PIPE cache: '%s' (%s)", smokableName, SmokableIngredients::GetCategoryName(smokableCategory));
				}

				// Scale the dropped ingredient to 0 (makes it disappear visually)
				ShrinkSmokableIngredient(droppedSmokable, 0.0f);
				_MESSAGE("[PipeFill]   -> Scaled ingredient to 0 (hidden)");
				
				// Delete the world object to clean it up properly
				DeleteWorldObject(droppedSmokable);

				// Trigger stronger haptic feedback on the hand with the empty pipe to confirm fill
				TriggerHapticFeedback(emptyPipeInLeft, emptyPipeInRight, 0.5f, 0.3f);
				_MESSAGE("[PipeFill] -> Haptic feedback triggered on %s hand!", emptyPipeInLeft ? "LEFT" : "RIGHT");

				// Unequip and remove the empty pipe dummy weapon, equip herb-filled pipe
				if (g_equipStateManager)
				{
					g_equipStateManager->UnequipAndRemoveEmptyPipe(emptyPipeInLeft, emptyPipeInRight);
				}
			}
			// Smoke rolling: dropping smokable while holding Roll of Paper + controllers near enough
			else if (holdingRollOfPaper && controllersNearForSmokeRolling)
			{
				// SMOKE ROLLING - create rolled smoke
				const char* smokableName = "Unknown";
				UInt32 smokableFormId = 0;
				SmokableCategory smokableCategory = SmokableCategory::None;

				if (droppedSmokable->baseForm)
				{
					smokableFormId = droppedSmokable->baseForm->formID;
					smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
					smokableCategory = SmokableIngredients::GetCategory(smokableFormId);
				}

				_MESSAGE("[SmokeRolling] *** SMOKE ROLLED! ***");
				_MESSAGE("[SmokeRolling]   -> Roll of Paper in %s hand", hasRollOfPaperLeft ? "LEFT" : "RIGHT");
				_MESSAGE("[SmokeRolling]   -> Smokable used: '%s' (FormID: %08X)", smokableName, smokableFormId);
				_MESSAGE("[SmokeRolling]   -> Smokable category: %s", SmokableIngredients::GetCategoryName(smokableCategory));

				// Store the smokable ingredient info in the ROLLED SMOKE specific cache
				g_filledRolledSmokeSmokableFormId = smokableFormId;
				g_filledRolledSmokeSmokableCategory = smokableCategory;
				_MESSAGE("[SmokeRolling] -> Stored in ROLLED SMOKE cache: '%s' (%s)", smokableName, SmokableIngredients::GetCategoryName(smokableCategory));

				// Scale the dropped smokable ingredient to 0 (makes it disappear visually)
				ShrinkSmokableIngredient(droppedSmokable, 0.0f);
				_MESSAGE("[SmokeRolling]   -> Scaled smokable ingredient to 0 (hidden)");
				
				// Delete the smokable world object to clean it up properly
				DeleteWorldObject(droppedSmokable);

				// Scale the Roll of Paper to 0 (makes it disappear visually)
				TESObjectREFR* rollOfPaper = hasRollOfPaperLeft ? g_heldRollOfPaperLeft : g_heldRollOfPaperRight;
				if (rollOfPaper)
				{
					ShrinkSmokableIngredient(rollOfPaper, 0.0f);
					_MESSAGE("[SmokeRolling]   -> Scaled Roll of Paper to 0 (hidden)");
					
					// Delete the roll of paper world object to clean it up properly
					DeleteWorldObject(rollOfPaper);
				}

				// Trigger haptic feedback on the hand with the Roll of Paper to confirm
				TriggerHapticFeedback(hasRollOfPaperLeft, hasRollOfPaperRight, 0.5f, 0.3f);
				_MESSAGE("[SmokeRolling]   -> Haptic feedback triggered on %s hand!", hasRollOfPaperLeft ? "LEFT" : "RIGHT");

				// Clear Roll of Paper tracking
				if (hasRollOfPaperLeft)
				{
					g_heldRollOfPaperLeft = nullptr;
					g_rollOfPaperHeldLeft = false;
				}
				if (hasRollOfPaperRight)
				{
					g_heldRollOfPaperRight = nullptr;
					g_rollOfPaperHeldRight = false;
				}

				// Equip the unlit rolled smoke to the hand that was holding the Roll of Paper
				if (g_equipStateManager)
				{
					// hasRollOfPaperLeft refers to VR controller (HIGGS), need to convert to game hand
					bool equipToGameLeftHand;
					if (IsLeftHandedMode())
					{
						// In left-handed mode: left VR = right game, right VR = left game
						equipToGameLeftHand = hasRollOfPaperRight;
					}
					else
					{
						// Normal mode: left VR = left game
						equipToGameLeftHand = hasRollOfPaperLeft;
					}
					g_equipStateManager->EquipUnlitRolledSmoke(equipToGameLeftHand);
					_MESSAGE("[SmokeRolling]   -> Equipping Unlit Rolled Smoke to %s game hand", equipToGameLeftHand ? "LEFT" : "RIGHT");
				}
			}
			else
			{
				// NEITHER PIPE FILL NOR SMOKE ROLLING - restore ingredient to default scale
				ShrinkSmokableIngredient(droppedSmokable, 1.0f);
				_MESSAGE("[HIGGS Drop] Restored smokable to default scale (conditions not met)");
			}
		}

		// Clear the held smokable when dropped
		if (isLeft)
		{
			if (g_heldSmokableLeft == droppedRefr || g_heldSmokableLeft != nullptr)
			{
				_MESSAGE("[HIGGS Drop] LEFT hand dropped smokable refr %08X", droppedRefr ? droppedRefr->formID : 0);
				g_heldSmokableLeft = nullptr;
			}
		}
		else
		{
			if (g_heldSmokableRight == droppedRefr || g_heldSmokableRight != nullptr)
			{
				_MESSAGE("[HIGGS Drop] RIGHT hand dropped smokable refr %08X", droppedRefr ? droppedRefr->formID : 0);
				g_heldSmokableRight = nullptr;
			}
		}

		// Restore HIGGS MouthRadius if no more smokables are held
		if (wasHoldingSmokable && g_heldSmokableLeft == nullptr && g_heldSmokableRight == nullptr)
		{
			RestoreHiggsMouthRadius();
		}
	}

	void OnHiggsConsumed(bool isLeft, TESForm* consumedForm)
	{
		// Clear the held smokable when consumed (dropped at mouth)
		// The reference no longer exists at this point, only the base form is provided
		bool wasHoldingSmokable = false;

		if (isLeft)
		{
			if (g_heldSmokableLeft != nullptr)
			{
				_MESSAGE("[HIGGS Consumed] LEFT hand item consumed (form %08X) - clearing held smokable", consumedForm ? consumedForm->formID : 0);
				g_heldSmokableLeft = nullptr;
				wasHoldingSmokable = true;
			}
		}
		else
		{
			if (g_heldSmokableRight != nullptr)
			{
				_MESSAGE("[HIGGS Consumed] RIGHT hand item consumed (form %08X) - clearing held smokable", consumedForm ? consumedForm->formID : 0);
				g_heldSmokableRight = nullptr;
				wasHoldingSmokable = true;
			}
		}

		// Restore HIGGS MouthRadius if no more smokables are held
		if (wasHoldingSmokable && g_heldSmokableLeft == nullptr && g_heldSmokableRight == nullptr)
		{
			RestoreHiggsMouthRadius();
		}

		// TODO: In future, we may want to prevent consumption of smokable ingredients
		// when an empty pipe is equipped and instead use them to fill the pipe.
		// This would require intercepting the consume action before it happens.
		// For now, we just log it.
		if (consumedForm && SmokableIngredients::IsSmokable(consumedForm->formID))
		{
			bool anyEmptyPipeEquipped = g_emptyPipeEquippedLeft || g_emptyPipeEquippedRight ||
				g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight ||
				g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight;

			if (anyEmptyPipeEquipped)
			{
				_MESSAGE("[HIGGS Consumed] WARNING: Smokable ingredient '%s' was consumed while empty pipe equipped!",
					SmokableIngredients::GetSmokableName(consumedForm->formID));
				// TODO: This is where we would fill the pipe instead of consuming
			}
		}
	}

	void OnHiggsGrabbed(bool isLeft, TESObjectREFR* grabbedRefr)
	{
		// Check if any UNLIT empty pipe is equipped in either hand (not the herb-filled ones)
		bool anyEmptyPipeEquipped = g_emptyPipeEquippedLeft || g_emptyPipeEquippedRight ||
			g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight ||
			g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight;

		// Check if holding a Roll of Paper (for smoke rolling)
		bool holdingRollOfPaper = (g_heldRollOfPaperLeft != nullptr) || (g_heldRollOfPaperRight != nullptr);

		// Only process smokable ingredients if we have a pipe or roll of paper
		if (!anyEmptyPipeEquipped && !holdingRollOfPaper)
			return;

		if (!grabbedRefr)
			return;

		TESForm* baseForm = grabbedRefr->baseForm;
		if (!baseForm)
			return;

		// Check if this is a smokable ingredient
		bool isSmokable = SmokableIngredients::IsSmokable(baseForm->formID);

		if (isSmokable)
		{
			const char* handStr = isLeft ? "LEFT" : "RIGHT";
			const char* smokableName = SmokableIngredients::GetSmokableName(baseForm->formID);
			SmokableCategory category = SmokableIngredients::GetCategory(baseForm->formID);
			const char* categoryName = SmokableIngredients::GetCategoryName(category);

			_MESSAGE("[HIGGS Grab] *** SMOKABLE INGREDIENT GRABBED! ***");
			_MESSAGE("[HIGGS Grab] %s hand grabbed SMOKABLE: '%s' (BaseFormID: %08X, RefrID: %08X, Category: %s)",
				handStr, smokableName, baseForm->formID, grabbedRefr->formID, categoryName);

			// Track the held smokable for continuous scale updates
			if (isLeft)
			{
				g_heldSmokableLeft = grabbedRefr;
			}
			else
			{
				g_heldSmokableRight = grabbedRefr;
			}

			// Apply initial shrink using config value
			float targetScale = configSmokableGrabbedScale;
			if (baseForm && SmokableIngredients::IsCannabis(baseForm->formID))
			{
				targetScale = 1.0f;
			}
			ShrinkSmokableIngredient(grabbedRefr, targetScale);

			// Change HIGGS MouthRadius to prevent accidental consumption
			SetHiggsMouthRadius(static_cast<double>(configHiggsMouthRadiusSmokable));

			// Log context
			if (anyEmptyPipeEquipped)
			{
				if (g_emptyPipeEquippedLeft) _MESSAGE("[HIGGS Grab]   -> Empty Herb Pipe equipped in LEFT hand");
				if (g_emptyPipeEquippedRight) _MESSAGE("[HIGGS Grab]   -> Empty Herb Pipe equipped in RIGHT hand");
				if (g_emptyBonePipeEquippedLeft) _MESSAGE("[HIGGS Grab]   -> Empty Herb Bone Pipe equipped in LEFT hand");
				if (g_emptyBonePipeEquippedRight) _MESSAGE("[HIGGS Grab]   -> Empty Herb Bone Pipe equipped in RIGHT hand");
				if (g_emptyWoodenPipeEquippedLeft) _MESSAGE("[HIGGS Grab]   -> Empty Wooden Pipe equipped in LEFT hand");
				if (g_emptyWoodenPipeEquippedRight) _MESSAGE("[HIGGS Grab] -> Empty Wooden Pipe equipped in RIGHT hand");
			}

			if (holdingRollOfPaper)
			{
				if (g_heldRollOfPaperLeft) _MESSAGE("[HIGGS Grab]   -> Roll of Paper held in LEFT hand");
				if (g_heldRollOfPaperRight) _MESSAGE("[HIGGS Grab] -> Roll of Paper held in RIGHT hand");
			}

			// Check pipe filling condition using global bool
			if (anyEmptyPipeEquipped && g_controllersTouchingLongEnough)
			{
				_MESSAGE("[HIGGS Grab] *** PIPE FILL CONDITION MET! ***");
				_MESSAGE("[HIGGS Grab]   -> Empty pipe equipped: YES");
				_MESSAGE("[HIGGS Grab]   -> Smokable grabbed: '%s'", smokableName);
				_MESSAGE("[HIGGS Grab]   -> Controllers near for filling long enough: YES");
			}
			else if (g_vrInputTracker && g_vrInputTracker->AreControllersNearForPipeFilling())
			{
				int touchDurationMs = g_vrInputTracker->GetControllersTouchingDurationMs();
				_MESSAGE("[HIGGS Grab]   -> Controllers near for filling but not long enough (%d ms / %d ms required)",
					touchDurationMs, configControllerTouchDurationMs);
			}
			else
			{
				_MESSAGE("[HIGGS Grab]   -> Controllers NOT near for filling (need to touch for %d ms)", configControllerTouchDurationMs);
			}
		}
		// Non-smokable items are silently ignored
	}

	// ============================================
	// Pipe Filling Condition Check
	// ============================================
	bool IsPipeFillingConditionMet()
	{
		// Condition 1: Any empty pipe is equipped
		bool anyEmptyPipeEquipped = g_emptyPipeEquippedLeft || g_emptyPipeEquippedRight ||
			g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight ||
			g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight;

		if (!anyEmptyPipeEquipped)
			return false;

		// Condition 2: A smokable ingredient is being held
		bool holdingSmokable = (g_heldSmokableLeft != nullptr) || (g_heldSmokableRight != nullptr);

		if (!holdingSmokable)
			return false;

		// Condition 3: Controllers have been touching long enough
		if (!g_controllersTouchingLongEnough)
			return false;

		// All conditions met!
		return true;
	}

	// ============================================
	// Left-Handed Mode Detection
	// ============================================
	
	// Local RelocPtr for left-handed mode - the address 0x01E71778 is from GameInput.cpp
	static RelocPtr<bool> s_leftHandedMode(0x01E71778);
	
	bool IsLeftHandedMode()
	{
		// Access the left-handed mode flag from the game
		return *s_leftHandedMode;
	}

	void CheckAndLogLeftHandedMode(bool isStartup)
	{
		bool currentLeftHandedMode = IsLeftHandedMode();
		
		if (isStartup)
		{
			g_lastKnownLeftHandedMode = currentLeftHandedMode;
			_MESSAGE("==============================================");
			_MESSAGE("[LeftHandedMode] VR Controller Mode: %s", currentLeftHandedMode ? "LEFT-HANDED" : "RIGHT-HANDED (default)");
			_MESSAGE("[LeftHandedMode] NOTE: In left-handed mode, VR controllers are inverted!");
			_MESSAGE("==============================================");
		}
		else if (currentLeftHandedMode != g_lastKnownLeftHandedMode)
		{
			// Mode changed!
			_MESSAGE("==============================================");
			_MESSAGE("[LeftHandedMode] *** VR CONTROLLER MODE CHANGED! ***");
			_MESSAGE("[LeftHandedMode] Previous: %s", g_lastKnownLeftHandedMode ? "LEFT-HANDED" : "RIGHT-HANDED");
			_MESSAGE("[LeftHandedMode] Current:  %s", currentLeftHandedMode ? "LEFT-HANDED" : "RIGHT-HANDED");
			_MESSAGE("==============================================");
			g_lastKnownLeftHandedMode = currentLeftHandedMode;
		}
	}

	void StartMod()
	{
		_MESSAGE("==============================================");
		_MESSAGE("Interactive Pipe Smoking VR - StartMod");
		_MESSAGE("==============================================");
		
		// Check and log left-handed mode at startup
		CheckAndLogLeftHandedMode(true);
		
		// Initialize smokable ingredients list
		SmokableIngredients::Initialize();

		// Initialize haptics managers
		InitializeHaptics();
		
		// First, load and verify the ESP
		if (LoadAndLogESP())
		{
			// Log all ESP records (resolves form IDs)
			LogAllESPRecords();
			
			// Initialize the equip state manager
			InitializeEquipStateManager();
			_MESSAGE("EquipStateManager initialized, ptr=%p", g_equipStateManager);

			// Initialize the VR input tracker
			InitializeVRInputTracker();
			_MESSAGE("VRInputTracker initialized, ptr=%p", g_vrInputTracker);
			
			// Register equip event sink to detect pipe equips
			RegisterEquipEventSink();

			// Register menu event sink to restore near clip on menu open
			RegisterMenuEventSink();

			// Register death event sink to reset state on player death
			RegisterDeathEventSink();

			// Register HIGGS grab callbacks (for pipe filling, smoke rolling, etc.)
			// This must happen AFTER ESP is loaded so form IDs are resolved
			RegisterHiggsGrabCallback();
			
			// Initialize pipe crafting system (registers HIGGS callbacks for crafting)
			// This also must happen AFTER ESP is loaded
			InitializePipeCrafting();
		}
		
		// Cache the original VRIK nearClipDistance for later restoration
		if (vrikInterface && !g_vrikNearClipDistanceCached)
		{
			g_originalVrikNearClipDistance = static_cast<float>(vrikInterface->getSettingDouble("nearClipDistance"));
			g_vrikNearClipDistanceCached = true;
			_MESSAGE("Cached original VRIK nearClipDistance: %f", g_originalVrikNearClipDistance);
		}
		else if (!vrikInterface)
		{
			_MESSAGE("VRIK interface not available - cannot cache nearClipDistance");
		}
		
		// Check and log initial left-handed mode
		CheckAndLogLeftHandedMode(true);
		
		_MESSAGE("==============================================");
		_MESSAGE("Interactive Pipe Smoking VR - Initialization Complete");
		_MESSAGE("==============================================");
	}

	// ============================================
	// Weapon Display Name Helpers
	// Used to add effect category to item names (e.g., "Herb Wooden Pipe (Healing)")
	// ============================================

	void SetWeaponDisplayName(UInt32 weaponFormId, const char* baseName, SmokableCategory category)
	{
		if (weaponFormId == 0 || !baseName)
			return;

		TESForm* form = LookupFormByID(weaponFormId);
		if (!form)
			return;

		// Cast to weapon - only weapons have fullName member we can set
		TESObjectWEAP* weapon = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
		if (!weapon)
		{
			_MESSAGE("[WeaponName] WARNING: Form %08X is not a weapon, cannot set name", weaponFormId);
			return;
		}

		// Get category name
		const char* categoryName = SmokableIngredients::GetCategoryName(category);
		if (!categoryName || category == SmokableCategory::None)
		{
			// No category, just use base name
			weapon->fullName.name = BSFixedString(baseName);
			_MESSAGE("[WeaponName] Set weapon %08X name to: '%s'", weaponFormId, baseName);
			return;
		}

		// Build name with category suffix: "Base Name (Category)"
		char newName[256];
		snprintf(newName, sizeof(newName), "%s (%s)", baseName, categoryName);

		// Set the new name directly on the weapon's fullName component
		weapon->fullName.name = BSFixedString(newName);
		_MESSAGE("[WeaponName] Set weapon %08X name to: '%s'", weaponFormId, newName);
	}

	void RestoreWeaponDisplayName(UInt32 weaponFormId, const char* baseName)
	{
		if (weaponFormId == 0 || !baseName)
			return;

		TESForm* form = LookupFormByID(weaponFormId);
		if (!form)
			return;

		// Cast to weapon - only weapons have fullName member we can set
		TESObjectWEAP* weapon = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
		if (!weapon)
		{
			_MESSAGE("[WeaponName] WARNING: Form %08X is not a weapon, cannot restore name", weaponFormId);
			return;
		}

		// Restore to base name (no category suffix)
		weapon->fullName.name = BSFixedString(baseName);
		_MESSAGE("[WeaponName] Restored weapon %08X name to: '%s'", weaponFormId, baseName);
	}

	void ResetModState()
	{
		_MESSAGE("==============================================");
		_MESSAGE("Interactive Pipe Smoking VR - Resetting Mod State");
		_MESSAGE("==============================================");

		// Reset the equipped smoke item counter FIRST (before unequipping)
		ResetEquippedSmokeItemCount();

		// Unequip any smoke items the player has equipped
		UnequipAllSmokeItems();

		// Stop any playing smoke exhale effects immediately
		StopSmokeExhaleEffect();

		// Stop any playing burning sounds
		StopBurningSound();
		_MESSAGE("[Reset] Stopped burning sound");

		// Reset smoking mechanics (inhale count, glow state, etc.)
		ResetSmokingMechanics();
		_MESSAGE("[Reset] Reset smoking mechanics");

		// Reset all effect timers and cooldowns (special save cooldown, recreational IMAD state, etc.)
		ResetAllEffectTimers();
		_MESSAGE("[Reset] Reset all effect timers and cooldowns");

		// Reset pipe crafting state (hit count, scales, etc.)
		ResetCraftingState();
		_MESSAGE("[Reset] Reset pipe crafting state");

		// Reset VRIK finger positions to defaults
		if (vrikInterface)
		{
			vrikInterface->restoreFingers(true);  // Left hand
			vrikInterface->restoreFingers(false); // Right hand
			_MESSAGE("[Reset] Restored VRIK fingers for both hands");

			// First restore near clip distance to original if we have a cached value
			if (g_vrikNearClipDistanceCached)
			{
				vrikInterface->setSettingDouble("nearClipDistance", static_cast<double>(g_originalVrikNearClipDistance));
				_MESSAGE("[Reset] Restored VRIK nearClipDistance to: %.1f", g_originalVrikNearClipDistance);
			}

			// Now re-cache the current VRIK nearClipDistance (may have changed due to game reload)
			// Force re-cache by clearing the flag first
			g_vrikNearClipDistanceCached = false;
			g_originalVrikNearClipDistance = static_cast<float>(vrikInterface->getSettingDouble("nearClipDistance"));
			g_vrikNearClipDistanceCached = true;
			_MESSAGE("[Reset] Re-cached VRIK nearClipDistance: %.1f", g_originalVrikNearClipDistance);
		}

		// Restore HIGGS MouthRadius if it was modified
		RestoreHiggsMouthRadius();
		// Reset HIGGS cache so it gets re-cached on next use
		g_higgsMouthRadiusCached = false;
		g_higgsMouthRadiusModified = false;
		_MESSAGE("[Reset] Reset HIGGS MouthRadius cache");

		// Clear held smokable tracking
		g_heldSmokableLeft = nullptr;
		g_heldSmokableRight = nullptr;
		_MESSAGE("[Reset] Cleared held smokable tracking");

		// Clear held crafting items (from PipeCrafting)
		g_heldKnifeLeft = nullptr;
		g_heldKnifeRight = nullptr;
		g_heldCraftingItemLeft = nullptr;
		g_heldCraftingItemRight = nullptr;
		g_heldCraftingMaterialLeft = CraftingMaterialType::None;
		g_heldCraftingMaterialRight = CraftingMaterialType::None;
		_MESSAGE("[Reset] Cleared held crafting items and knives");

		// Clear held roll of paper (for smoke rolling)
		g_heldRollOfPaperLeft = nullptr;
		g_heldRollOfPaperRight = nullptr;
		g_rollOfPaperHeldLeft = false;
		g_rollOfPaperHeldRight = false;
		g_smokeRollingConditionLogged = false;
		_MESSAGE("[Reset] Cleared held roll of paper tracking");

		// NOTE: We do NOT clear the filled pipe smokable caches here!
		// Effect caches (g_filledWoodenPipeSmokableFormId, g_filledBonePipeSmokableFormId, 
		// g_filledRolledSmokeSmokableFormId) are only cleared when the lit item is actually
		// unequipped (handled in EquipState.cpp). This preserves effects across game loads.
		_MESSAGE("[Reset] Preserving filled pipe/smoke smokable caches (cleared only on lit item unequip)");

		// Clear active smokable (this gets set fresh when a lit item is equipped)
		g_activeSmokableFormId = 0;
		g_activeSmokableCategory = SmokableCategory::None;
		_MESSAGE("[Reset] Cleared active smokable");

		// Reset empty pipe tracking flags
		g_emptyPipeEquippedLeft = false;
		g_emptyPipeEquippedRight = false;
		g_emptyBonePipeEquippedLeft = false;
		g_emptyBonePipeEquippedRight = false;
		g_emptyWoodenPipeEquippedLeft = false;
		g_emptyWoodenPipeEquippedRight = false;
		g_controllersTouchingLongEnough = false;
		g_herbPipeFlippedLongEnough = false;
		_MESSAGE("[Reset] Cleared empty pipe equipped flags, controller touch state, and herb pipe flipped state");

		// Reset pipe filling condition logged state
		g_pipeFillingConditionLogged = false;
		_MESSAGE("[Reset] Reset pipe filling condition state");

		// Reset VR input tracking completely (stop and clear state, but don't restart - 
		// tracking will restart when player equips a smoke item)
		if (g_vrInputTracker)
		{
			g_vrInputTracker->StopTracking();
			g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
			g_vrInputTracker->SetHerbPipeEquippedHand(false, false);
			g_vrInputTracker->SetUnlitRolledSmokeEquippedHand(false, false);
			g_vrInputTracker->SetLitItemEquippedHand(false, false);
			_MESSAGE("[Reset] Stopped VR input tracking and cleared all equip states (will restart on equip)");
		}

		_MESSAGE("==============================================");
		_MESSAGE("Interactive Pipe Smoking VR - Reset Complete");
		_MESSAGE("==============================================");
	}
}

