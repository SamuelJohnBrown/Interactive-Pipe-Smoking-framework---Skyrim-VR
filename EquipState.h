#pragma once

#include "Helper.h"
#include "skse64/GameEvents.h"

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Cached Finger Positions (for VRIK)
	// ============================================
	struct CachedFingerPositions
	{
		bool isSet = false;
		bool isLeftHand = false;
		float thumb1 = 0, thumb2 = 0;
		float index1 = 0, index2 = 0;
		float middle1 = 0, middle2 = 0;
		float ring1 = 0, ring2 = 0;
		float pinky1 = 0, pinky2 = 0;
	};

	// Get cached finger positions for specified hand (returns nullptr if not cached)
	CachedFingerPositions* GetCachedFingerPositions(bool isLeftHand);

	// ============================================
	// Equip State Manager
	// Handles equipping visual armor when smoke products are equipped
	// Supports: Rolled Smoke, Pipes, Bongs (both UNLIT and LIT variants)
	// ============================================
	class EquipStateManager
	{
	public:
		EquipStateManager();
		~EquipStateManager();

		// Initialize the manager
		void Initialize();

		// Called when a dummy weapon (rolled smoke, pipe, bong) is equipped/unequipped
		void OnDummyWeaponEquipEvent(const TESEquipEvent& evn, bool inLeftHand, bool inRightHand);

		// Unequip and remove empty pipe dummy weapon (used during pipe filling)
		void UnequipAndRemoveEmptyPipe(bool fromLeftHand, bool fromRightHand);

		// Unequip herb-filled pipe and equip empty pipe (for pipe emptying/dumping)
		void UnequipHerbPipeAndEquipEmpty(bool fromLeftHand, bool fromRightHand);

		// Light herb pipe (swap unlit for lit variant)
		void LightHerbPipe(bool inLeftHand, bool inRightHand);

		// Light rolled smoke (swap unlit for lit variant)
		void LightRolledSmoke(bool inLeftHand, bool inRightHand);

		// Equip herb-filled pipe (after filling)
		void EquipHerbFilledPipe(bool inLeftHand, bool inRightHand);

		// Deplete lit pipe to empty pipe (after 25 inhales or when dumped)
		void DepleteLitPipeToEmpty();

		// Unequip current smokable from specified VR controller hand (for hand swapping)
		// This unequips the dummy weapon - armor is auto-handled by equip event handlers
		void UnequipCurrentSmokable(bool fromLeftVRController);

		// Equip empty wooden pipe (for crafting completion)
		void EquipEmptyWoodenPipe(bool inLeftHand);

		// Equip empty bone pipe (for crafting)
		void EquipEmptyBonePipe(bool inLeftHand);

		// Equip unlit rolled smoke (for smoke rolling)
		void EquipUnlitRolledSmoke(bool inLeftHand);

	private:
		// Equip/Unequip visual armor helpers
		void EquipVisualArmor(UInt32 armorFormId);
		void UnequipVisualArmor(UInt32 armorFormId);
		
		// Unequip and remove a weapon from inventory
		void UnequipAndRemoveWeapon(UInt32 weaponFormId, const char* weaponName);
		
		// Product-specific equip handlers - UNLIT
		void HandleRolledSmokeEquip(bool isEquip, bool inLeftHand, bool inRightHand);
		void HandleHerbWoodenPipeEquip(bool isEquip, bool inLeftHand, bool inRightHand); // Was HandlePipeEquip
		void HandleHerbBonePipeEquip(bool isEquip, bool inLeftHand, bool inRightHand);   // Was HandleBonePipeEquip
		void HandleEmptyWoodenPipeEquip(bool isEquip, bool inLeftHand, bool inRightHand);
		void HandleEmptyBonePipeEquip(bool isEquip, bool inLeftHand, bool inRightHand);
		void HandleBongEquip(bool isEquip, bool inLeftHand, bool inRightHand);

		// Product-specific equip handlers - LIT
		void HandleRolledSmokeLitEquip(bool isEquip, bool inLeftHand, bool inRightHand);
		void HandleWoodenPipeLitEquip(bool isEquip, bool inLeftHand, bool inRightHand); // Was HandlePipeLitEquip
		void HandleBonePipeLitEquip(bool isEquip, bool inLeftHand, bool inRightHand);
	};

	// ============================================
	// Global Manager Instance
	// ============================================
	extern EquipStateManager* g_equipStateManager;

	// Initialize/Shutdown the global manager
	void InitializeEquipStateManager();
	void ShutdownEquipStateManager();

	// Unequip all dummy smoke items from player (called on game load)
	void UnequipAllSmokeItems();

	// Reset the equipped smoke item counter (called on game load)
	void ResetEquippedSmokeItemCount();

}
