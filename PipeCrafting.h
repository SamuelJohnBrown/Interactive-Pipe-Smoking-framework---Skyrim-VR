#pragma once

#include "skse64/GameReferences.h"

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Pipe Crafting System
	// Handles crafting pipes from grabbed items
	// ============================================

	// Crafting material types
	enum class CraftingMaterialType
	{
		None,
		Wood,   // For wooden pipe crafting
		Bone    // For bone pipe crafting
	};

	// Knife/Dagger grabbed state (via HIGGS, not equipped)
	extern TESObjectREFR* g_heldKnifeLeft;
	extern TESObjectREFR* g_heldKnifeRight;

	// Currently held item for crafting
	extern TESObjectREFR* g_heldCraftingItemLeft;
	extern TESObjectREFR* g_heldCraftingItemRight;

	// Material type of held crafting items
	extern CraftingMaterialType g_heldCraftingMaterialLeft;
	extern CraftingMaterialType g_heldCraftingMaterialRight;

	// Crafting hit counter
	extern int g_craftingHitCount;

	// Number of hits required for perfect crafting
	constexpr int CRAFTING_HITS_REQUIRED = 3;

	// ============================================
	// Smoke Rolling System
	// Handles rolling smokes from paper + herb
	// ============================================

	// Roll of Paper base form ID (vanilla Skyrim)
	constexpr UInt32 ROLL_OF_PAPER_BASE_FORMID = 0x00033761;

	// Grabbable Empty Pipe MISC items (world-placed items that can be picked up with HIGGS)
	// These are MISC items from the ESP, not the dummy weapons
	constexpr UInt32 EMPTY_WOODEN_PIPE_MISC_BASE_FORMID = 0x000800;  // BaseFormID in logs shows 2B000800
	constexpr UInt32 EMPTY_BONE_PIPE_MISC_BASE_FORMID = 0x000801;    // BaseFormID in logs shows 2B000801

	// Currently held roll of paper for smoke rolling
	extern TESObjectREFR* g_heldRollOfPaperLeft;
	extern TESObjectREFR* g_heldRollOfPaperRight;

	// Scale for roll of paper when grabbed (65%)
	constexpr float ROLL_OF_PAPER_SCALE = 0.65f;

	// Track if smoke rolling condition was already logged (to avoid spam)
	extern bool g_smokeRollingConditionLogged;

	// Initialize pipe crafting system (registers HIGGS callbacks)
	void InitializePipeCrafting();

	// Shutdown pipe crafting system
	void ShutdownPipeCrafting();

	// HIGGS callback for when player grabs an item
	void OnItemGrabbed(bool isLeft, TESObjectREFR* grabbedRefr);

	// HIGGS callback for when player drops an item
	void OnItemDropped(bool isLeft, TESObjectREFR* droppedRefr);

	// HIGGS callback for collision detection
	void OnCraftingCollision(bool isLeft, float mass, float separatingVelocity);

	// Update held crafting item scale (called every frame)
	void UpdateHeldCraftingItemScale();

	// Check if a weapon form is a knife/dagger
	bool IsKnifeOrDagger(TESForm* form);

	// Check if an item name indicates bone material
	bool IsBoneMaterial(const char* itemName);

	// Check if a form is a Roll of Paper
	bool IsRollOfPaper(UInt32 formId);

	// Check if a form is a grabbable Empty Wooden Pipe MISC item (from world)
	bool IsGrabbableEmptyWoodenPipe(UInt32 formId);

	// Check if a form is a grabbable Empty Bone Pipe MISC item (from world)
	bool IsGrabbableEmptyBonePipe(UInt32 formId);

	// Check if an item is a valid crafting material for pipe making (firewood, wood, bone)
	bool IsValidCraftingMaterial(TESForm* baseForm);

	// Check for already-grabbed smokable ingredients (called when Roll of Paper grabbed or empty pipe equipped)
	void CheckForAlreadyGrabbedSmokable();

	// Called when weapon is equipped/unequipped (legacy - no longer used for knife tracking)
	void OnWeaponEquipStateChanged(TESForm* weapon, bool isEquip, bool isLeftHand);

	// Reset crafting state
	void ResetCraftingState();

	// Check smoke rolling condition (called every frame)
	void CheckSmokeRollingCondition();
}
