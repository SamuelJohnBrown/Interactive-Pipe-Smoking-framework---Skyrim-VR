#pragma once

#include "Helper.h"
#include "SmokableIngredients.h"
#include "skse64/GameEvents.h"

namespace InteractivePipeSmokingVR
{
	extern SKSETrampolineInterface* g_trampolineInterface;
	extern HiggsPluginAPI::IHiggsInterface001* higgsInterface;
	extern vrikPluginApi::IVrikInterface001* vrikInterface;
	extern SkyrimVRESLPluginAPI::ISkyrimVRESLInterface001* skyrimVRESLInterface;

	// Near clip distance settings
	extern float nearClipDistance;
	extern float g_originalVrikNearClipDistance;
	extern bool g_vrikNearClipDistanceCached;
	
	// HIGGS MouthRadius settings
	extern double g_originalHiggsMouthRadius;
	extern bool g_higgsMouthRadiusCached;
	extern bool g_higgsMouthRadiusModified;

	// ESP plugin name
	constexpr const char* ESP_NAME = "Interactive_Herb_Smoking_VR.esp";
	
	// Weapons (dummy items) - UNLIT
	constexpr UInt32 ROLLED_SMOKE_WEAPON_BASE_FORMID = 0x005902;
	constexpr UInt32 HERB_WOODEN_PIPE_WEAPON_BASE_FORMID = 0x014C0A;
	constexpr UInt32 HERB_BONE_PIPE_WEAPON_BASE_FORMID = 0x000804;
	constexpr UInt32 EMPTY_WOODEN_PIPE_WEAPON_BASE_FORMID = 0x00080A;
	constexpr UInt32 EMPTY_BONE_PIPE_WEAPON_BASE_FORMID = 0x000810;

	// Weapons (dummy items) - LIT
	constexpr UInt32 ROLLED_SMOKE_LIT_WEAPON_BASE_FORMID = 0x014c2e;
	constexpr UInt32 WOODEN_PIPE_LIT_WEAPON_BASE_FORMID = 0x014c34;  // Was PIPE_LIT_WEAPON
	constexpr UInt32 BONE_PIPE_LIT_WEAPON_BASE_FORMID = 0x000817;


	// Armor visuals - Rolled Smoke (UNLIT)
	constexpr UInt32 SMOKE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x00FB07;
	constexpr UInt32 SMOKE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x005901;

	// Armor visuals - Rolled Smoke (LIT)
	constexpr UInt32 SMOKE_LIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x014c30;
	constexpr UInt32 SMOKE_LIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x014c31;

	// Armor visuals - Wooden Pipe / Herb Pipe (UNLIT)
	constexpr UInt32 HERB_WOODEN_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x014C15; // Left armor (base form id)
	constexpr UInt32 HERB_WOODEN_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x014C14; // Right armor (base form id)

	// Armor visuals - Wooden Pipe (LIT)
	constexpr UInt32 WOODEN_PIPE_LIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x014c45; // Was PIPE_LIT
	constexpr UInt32 WOODEN_PIPE_LIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x014c44; // Was PIPE_LIT

	// Armor visuals - Bone Pipe / Herb Bone Pipe (UNLIT)
	constexpr UInt32 HERB_BONE_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x000806; // Was BONE_PIPE_UNLIT
	constexpr UInt32 HERB_BONE_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x000805; // Was BONE_PIPE_UNLIT

	// Armor visuals - Bone Pipe (LIT)
	constexpr UInt32 BONE_PIPE_LIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x00081E;
	constexpr UInt32 BONE_PIPE_LIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x000818;

	// Armor visuals - Empty Wooden Pipe (UNLIT)
	constexpr UInt32 EMPTY_WOODEN_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x00080C; // Left armor (full: 0500080C)
	constexpr UInt32 EMPTY_WOODEN_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x00080D; // Right armor (full: 0500080D)

	// Armor visuals - Empty Bone Pipe (UNLIT)
	constexpr UInt32 EMPTY_BONE_PIPE_UNLIT_VISUAL_LEFT_ARMOR_BASE_FORMID = 0x000813;
	constexpr UInt32 EMPTY_BONE_PIPE_UNLIT_VISUAL_RIGHT_ARMOR_BASE_FORMID = 0x000812;

	// Sound records - Burning/Lighting sounds
	constexpr UInt32 BURNING_SOUND_1_BASE_FORMID = 0x00FB05;
	constexpr UInt32 BURNING_SOUND_2_BASE_FORMID = 0x014C3F;

	// Visual Effect records - Smoke exhale FX
	constexpr UInt32 SMOKE_EXHALE_FX_BASE_FORMID = 0x000811;

	// Resolved full form IDs (set at runtime) - Weapons UNLIT
	extern UInt32 g_rolledSmokeWeaponFullFormId;
	extern UInt32 g_herbWoodenPipeWeaponFullFormId; // Was g_emptyPipeWeaponFullFormId
	extern UInt32 g_herbBonePipeWeaponFullFormId;   // Was g_bonePipeWeaponFullFormId
	extern UInt32 g_emptyWoodenPipeWeaponFullFormId;
	extern UInt32 g_emptyBonePipeWeaponFullFormId;

	// Resolved full form IDs (set at runtime) - Weapons LIT
	extern UInt32 g_rolledSmokeLitWeaponFullFormId;
	extern UInt32 g_woodenPipeLitWeaponFullFormId;  // Was g_pipeLitWeaponFullFormId
	extern UInt32 g_bonePipeLitWeaponFullFormId;

	// Resolved full form IDs (set at runtime) - Rolled Smoke Armor
	extern UInt32 g_smokeUnlitVisualLeftArmorFullFormId;
	extern UInt32 g_smokeUnlitVisualRightArmorFullFormId;
	extern UInt32 g_smokeLitVisualLeftArmorFullFormId;
	extern UInt32 g_smokeLitVisualRightArmorFullFormId;

	// Resolved full form IDs (set at runtime) - Wooden Pipe Armor
	extern UInt32 g_herbWoodenPipeUnlitVisualLeftArmorFullFormId;  // Was g_pipeUnlitVisualLeftArmorFullFormId
	extern UInt32 g_herbWoodenPipeUnlitVisualRightArmorFullFormId; // Was g_pipeUnlitVisualRightArmorFullFormId
	extern UInt32 g_woodenPipeLitVisualLeftArmorFullFormId; // Was g_pipeLitVisualLeftArmorFullFormId
	extern UInt32 g_woodenPipeLitVisualRightArmorFullFormId;// Was g_pipeLitVisualRightArmorFullFormId

	// Resolved full form IDs (set at runtime) - Bone Pipe Armor
	extern UInt32 g_herbBonePipeUnlitVisualLeftArmorFullFormId; // Was g_bonePipeUnlitVisualLeftArmorFullFormId
	extern UInt32 g_herbBonePipeUnlitVisualRightArmorFullFormId;// Was g_bonePipeUnlitVisualRightArmorFullFormId
	extern UInt32 g_bonePipeLitVisualLeftArmorFullFormId;
	extern UInt32 g_bonePipeLitVisualRightArmorFullFormId;

	// Resolved full form IDs (set at runtime) - Empty Wooden Pipe Armor
	extern UInt32 g_emptyWoodenPipeUnlitVisualLeftArmorFullFormId;
	extern UInt32 g_emptyWoodenPipeUnlitVisualRightArmorFullFormId;

	// Resolved full form IDs (set at runtime) - Empty Bone Pipe Armor
	extern UInt32 g_emptyBonePipeUnlitVisualLeftArmorFullFormId;
	extern UInt32 g_emptyBonePipeUnlitVisualRightArmorFullFormId;

	// Resolved full form IDs (set at runtime) - Sound records
	extern UInt32 g_burningSound1FullFormId;
	extern UInt32 g_burningSound2FullFormId;

	// Resolved full form IDs (set at runtime) - Visual Effect records
	extern UInt32 g_smokeExhaleFxFullFormId;

	// ============================================
	// VisualEffect Play/Stop Functions (Papyrus native)
	// ============================================
	typedef void(*_VisualEffect_Play)(VMClassRegistry* registry, UInt32 stackId, BGSReferenceEffect* effect, TESObjectREFR* target, float duration, TESObjectREFR* facingObject);
	extern RelocAddr<_VisualEffect_Play> VisualEffect_Play;

	// Play smoke exhale visual effect at player
	void PlaySmokeExhaleEffect();

	// Stop smoke exhale visual effect at player
	void StopSmokeExhaleEffect();

	// Flag to track if ESP is loaded
	extern bool espLoaded;
	
	void StartMod();
	
	// VRIK settings functions
	void SetNearClipDistance(float distance);
	void RestoreVrikSettings();
	
	// ESP loading and logging functions
	bool LoadAndLogESP();
	void LogAllESPRecords();
	
	// Equip event handling
	void RegisterEquipEventSink();
	void UnregisterEquipEventSink();

	// Helpers - UNLIT weapons
	bool IsRolledSmokeWeapon(UInt32 formId);
	bool IsHerbWoodenPipeWeapon(UInt32 formId); // Was IsEmptyPipeWeapon
	bool IsHerbBonePipeWeapon(UInt32 formId);   // Was IsBonePipeWeapon
	bool IsEmptyWoodenPipeWeapon(UInt32 formId);
	bool IsEmptyBonePipeWeapon(UInt32 formId);

	// Helpers - LIT weapons
	bool IsRolledSmokeLitWeapon(UInt32 formId);
	bool IsWoodenPipeLitWeapon(UInt32 formId); // Was IsPipeLitWeapon
	bool IsBonePipeLitWeapon(UInt32 formId);
	
	// Check if all pipe filling conditions are met
	bool IsPipeFillingConditionMet();

	// Continuously check pipe filling condition (called every frame while holding smokable)
	void CheckPipeFillingCondition();

	// Track if controllers have been touching long enough (for pipe filling)
	extern bool g_controllersTouchingLongEnough;

	// Track if herb pipe hand has been flipped (facing down) long enough
	extern bool g_herbPipeFlippedLongEnough;

	// Track if empty pipe is equipped (for HIGGS grab detection)
	extern bool g_emptyPipeEquippedLeft;
	extern bool g_emptyPipeEquippedRight;
	extern bool g_emptyBonePipeEquippedLeft;
	extern bool g_emptyBonePipeEquippedRight;
	extern bool g_emptyWoodenPipeEquippedLeft;
	extern bool g_emptyWoodenPipeEquippedRight;

	// Track if Roll of Paper is held (for smoke rolling - treated like empty pipe)
	extern bool g_rollOfPaperHeldLeft;
	extern bool g_rollOfPaperHeldRight;

	// Track currently held smokable ingredients
	extern TESObjectREFR* g_heldSmokableLeft;
	extern TESObjectREFR* g_heldSmokableRight;

	// Track the smokable ingredient that was used to fill each pipe type (for effects)
	// Separate caches for each pipe type so they can have independent effects
	extern UInt32 g_filledWoodenPipeSmokableFormId;
	extern SmokableCategory g_filledWoodenPipeSmokableCategory;

	extern UInt32 g_filledBonePipeSmokableFormId;
	extern SmokableCategory g_filledBonePipeSmokableCategory;

	extern UInt32 g_filledRolledSmokeSmokableFormId;
	extern SmokableCategory g_filledRolledSmokeSmokableCategory;

	// Active smokable (set when a lit item is equipped, used by smoking mechanics)
	extern UInt32 g_activeSmokableFormId;
	extern SmokableCategory g_activeSmokableCategory;

	// HIGGS grab callbacks
	void OnHiggsGrabbed(bool isLeft, TESObjectREFR* grabbedRefr);
	void OnHiggsDropped(bool isLeft, TESObjectREFR* droppedRefr);
	void OnHiggsConsumed(bool isLeft, TESForm* consumedForm);

	// Smokable scaling helpers
	void ShrinkSmokableIngredient(TESObjectREFR* refr, float targetScale);
	void UpdateHeldSmokableScale();

	// HIGGS mouth radius helpers
	void SetHiggsMouthRadius(double radius);
	void RestoreHiggsMouthRadius();

	// Sound playing helpers
	void PlaySoundAtPlayer(UInt32 soundFormID);
	void PlayRandomBurningSound();
	void StopBurningSound();

	// Reset mod state (called on game load/death)
	void ResetModState();

	// Equip event sink class
	class PipeEquipEventSink : public BSTEventSink<TESEquipEvent>
	{
	public:
		static PipeEquipEventSink* GetSingleton();
		virtual EventResult ReceiveEvent(TESEquipEvent* evn, EventDispatcher<TESEquipEvent>* dispatcher) override;
	};

	// Menu event sink class
	class MenuOpenCloseEventSink : public BSTEventSink<MenuOpenCloseEvent>
	{
	public:
		static MenuOpenCloseEventSink* GetSingleton();
		virtual EventResult ReceiveEvent(MenuOpenCloseEvent* evn, EventDispatcher<MenuOpenCloseEvent>* dispatcher) override;
	};
	void RegisterMenuEventSink();
	void UnregisterMenuEventSink();

	// Death event sink class
	class ActorDeathEventSink : public BSTEventSink<TESDeathEvent>
	{
	public:
		static ActorDeathEventSink* GetSingleton();
		virtual EventResult ReceiveEvent(TESDeathEvent* evn, EventDispatcher<TESDeathEvent>* dispatcher) override;
	};
	void RegisterDeathEventSink();
	void UnregisterDeathEventSink();

	// ============================================
	// Left-Handed Mode Detection
	// ============================================
	// Check if VR left-handed mode is enabled
	bool IsLeftHandedMode();
	
	// Check and log left-handed mode (isStartup=true for initial check, false for periodic check)
	void CheckAndLogLeftHandedMode(bool isStartup);
	
	// Track last known left-handed mode state
	extern bool g_lastKnownLeftHandedMode;

	// Set weapon display name with category suffix
	// Example: "Herb Wooden Pipe (Healing)"
	void SetWeaponDisplayName(UInt32 weaponFormId, const char* baseName, SmokableCategory category);

	// Restore weapon display name to original (no category suffix)
	void RestoreWeaponDisplayName(UInt32 weaponFormId, const char* baseName);
}