#include "EquipState.h"
#include "Engine.h"
#include "VRInputTracker.h"
#include "SmokingMechanics.h"
#include "PipeCrafting.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/PluginAPI.h"
#include <thread>
#include <chrono>

namespace InteractivePipeSmokingVR
{
	// External task interface from main.cpp
	extern SKSETaskInterface* g_task;

	// ============================================
	// Delayed Equip Armor Task (runs on game thread after delay)
	// ============================================
	class DelayedEquipArmorTask : public TaskDelegate
	{
	public:
		UInt32 m_armorFormId;

		DelayedEquipArmorTask(UInt32 armorFormId) : m_armorFormId(armorFormId) {}

		virtual void Run() override
		{
			Actor* player = (*g_thePlayer);
			if (!player)
			{
				_MESSAGE("[DelayedEquip] Player not available");
				return;
			}

			TESForm* armorForm = LookupFormByID(m_armorFormId);
			if (!armorForm)
			{
				_MESSAGE("[DelayedEquip] Armor form %08X not found", m_armorFormId);
				return;
			}

			EquipManager* equipMan = EquipManager::GetSingleton();
			if (!equipMan)
			{
				_MESSAGE("[DelayedEquip] EquipManager not available");
				return;
			}

			// EquipItem params: actor, item, extraData, count, slot, withEquipSound, preventUnequip, showMsg, unk
			// withEquipSound = false for silent equip
			CALL_MEMBER_FN(equipMan, EquipItem)(player, armorForm, nullptr, 1, nullptr, false, false, false, nullptr);
			_MESSAGE("[DelayedEquip] Equipped armor %08X (silent)", m_armorFormId);
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	// ============================================
	// Delayed Equip Weapon Task (runs on game thread after delay)
	// ============================================
	class DelayedEquipWeaponTask : public TaskDelegate
	{
	public:
		UInt32 m_weaponFormId;
		bool m_equipToLeftHand;

		DelayedEquipWeaponTask(UInt32 weaponFormId, bool equipToLeftHand) 
			: m_weaponFormId(weaponFormId), m_equipToLeftHand(equipToLeftHand) {}

		virtual void Run() override
		{
			Actor* player = (*g_thePlayer);
			if (!player)
			{
				_MESSAGE("[DelayedEquipWeapon] Player not available");
				return;
			}

			TESForm* weaponForm = LookupFormByID(m_weaponFormId);
			if (!weaponForm)
			{
				_MESSAGE("[DelayedEquipWeapon] Weapon form %08X not found", m_weaponFormId);
				return;
			}

			EquipManager* equipMan = EquipManager::GetSingleton();
			if (!equipMan)
			{
				_MESSAGE("[DelayedEquipWeapon] EquipManager not available");
				return;
			}

			// Get the appropriate slot for left or right hand
			BGSEquipSlot* slot = nullptr;
			if (m_equipToLeftHand)
			{
				slot = GetLeftHandSlot();
			}
			else
			{
				slot = GetRightHandSlot();
			}

			// EquipItem params: actor, item, extraData, count, slot, withEquipSound, preventUnequip, showMsg, unk
			CALL_MEMBER_FN(equipMan, EquipItem)(player, weaponForm, nullptr, 1, slot, false, false, false, nullptr);
			_MESSAGE("[DelayedEquipWeapon] Equipped weapon %08X to %s hand (silent)", m_weaponFormId, m_equipToLeftHand ? "LEFT" : "RIGHT");
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	// ============================================
	// Thread function to delay then queue the equip task
	// ============================================
	static void DelayedEquipThread(UInt32 armorFormId, int delayMs)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
		
		if (g_task)
		{
			g_task->AddTask(new DelayedEquipArmorTask(armorFormId));
			_MESSAGE("[EquipState] Queued equip task after %dms delay for armor %08X", delayMs, armorFormId);
		}
	}

	// ============================================
	// Thread function to delay then queue the weapon equip task
	// ============================================
	static void DelayedEquipWeaponThread(UInt32 weaponFormId, bool equipToLeftHand, int delayMs)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
		
		if (g_task)
		{
			g_task->AddTask(new DelayedEquipWeaponTask(weaponFormId, equipToLeftHand));
			_MESSAGE("[EquipState] Queued weapon equip task after %dms delay for weapon %08X to %s hand", 
				delayMs, weaponFormId, equipToLeftHand ? "LEFT" : "RIGHT");
		}
	}

	// ============================================
	// Global Manager Instance
	// ============================================
	EquipStateManager* g_equipStateManager = nullptr;

	// Track number of equipped smoke items (for VR input tracking)
	static int g_equippedSmokeItemCount = 0;

	// Track fire spell equipped state
	static bool g_fireSpellLeftHand = false;
	static bool g_fireSpellRightHand = false;

	// Flag to skip VRIK finger restoration during pipe fill transition
	static bool g_skipFingerRestoreOnUnequip = false;

	// Flag to skip inventory removal during hand swap (for lit items)
	static bool g_isHandSwapUnequip = false;

	// Cached finger positions for equipped smokable items
	static CachedFingerPositions g_cachedFingerPositionsLeft;
	static CachedFingerPositions g_cachedFingerPositionsRight;

	// Helper to cache finger positions
	static void CacheFingerPositions(bool isLeftHand, float thumb1, float thumb2, float index1, float index2,
		float middle1, float middle2, float ring1, float ring2, float pinky1, float pinky2)
	{
		CachedFingerPositions& cache = isLeftHand ? g_cachedFingerPositionsLeft : g_cachedFingerPositionsRight;
		cache.isSet = true;
		cache.isLeftHand = isLeftHand;
		cache.thumb1 = thumb1; cache.thumb2 = thumb2;
		cache.index1 = index1; cache.index2 = index2;
		cache.middle1 = middle1; cache.middle2 = middle2;
		cache.ring1 = ring1; cache.ring2 = ring2;
		cache.pinky1 = pinky1; cache.pinky2 = pinky2;
		_MESSAGE("[FingerCache] Cached finger positions for %s hand", isLeftHand ? "LEFT" : "RIGHT");
	}

	// Helper to clear cached finger positions
	static void ClearCachedFingerPositions(bool isLeftHand)
	{
		CachedFingerPositions& cache = isLeftHand ? g_cachedFingerPositionsLeft : g_cachedFingerPositionsRight;
		if (cache.isSet)
		{
			cache.isSet = false;
			_MESSAGE("[FingerCache] Cleared finger positions for %s hand", isLeftHand ? "LEFT" : "RIGHT");
		}
	}

	// Helper to get cached finger positions (returns nullptr if not cached)
	CachedFingerPositions* GetCachedFingerPositions(bool isLeftHand)
	{
		CachedFingerPositions& cache = isLeftHand ? g_cachedFingerPositionsLeft : g_cachedFingerPositionsRight;
		return cache.isSet ? &cache : nullptr;
	}

	// ============================================
	// Helper Functions
	// ============================================
	static const char* HandStr(bool left, bool right)
	{
		if (left && right) return "Both";
		if (left) return "Left";
		if (right) return "Right";
		return "Unknown";
	}

	// ============================================
	// Left-Handed Mode Correction Helper
	// In left-handed mode, VR controllers are inverted relative to game hand slots.
	// This function converts from game hand slot to VR controller hand.
	// ============================================
	static void GetVRControllerHands(bool gameLeftHand, bool gameRightHand, bool& vrLeftController, bool& vrRightController)
	{
		if (IsLeftHandedMode())
		{
			// In left-handed mode, game left = VR right controller, game right = VR left controller
			vrLeftController = gameRightHand;
			vrRightController = gameLeftHand;
			
			if (gameLeftHand || gameRightHand)
			{
				_MESSAGE("[LeftHandedMode] Inverting hands: Game(%s) -> VR Controller(%s)",
					HandStr(gameLeftHand, gameRightHand),
					HandStr(vrLeftController, vrRightController));
			}
		}
		else
		{
			// In right-handed mode (default), game hand = VR controller hand
			vrLeftController = gameLeftHand;
			vrRightController = gameRightHand;
		}
	}

	// Convenience overload that returns a single bool for VRIK's isLeft parameter
	static bool GetVRControllerIsLeft(bool gameLeftHand, bool gameRightHand)
	{
		bool vrLeft, vrRight;
		GetVRControllerHands(gameLeftHand, gameRightHand, vrLeft, vrRight);
		return vrLeft;
	}

	// Check if a spell has a specific keyword (by editor ID substring)
	static bool SpellHasKeyword(SpellItem* spell, const char* keywordEditorId)
	{
		if (!spell || !keywordEditorId)
			return false;

		// Iterate through spell effects
		for (UInt32 i = 0; i < spell->effectItemList.count; ++i)
		{
			MagicItem::EffectItem* effectItem = nullptr;
			spell->effectItemList.GetNthItem(i, effectItem);
			
			if (!effectItem || !effectItem->mgef)
				continue;

			EffectSetting* effect = effectItem->mgef;
			
			// Check keywords on the effect
			if (effect->keywordForm.numKeywords > 0 && effect->keywordForm.keywords)
			{
				for (UInt32 k = 0; k < effect->keywordForm.numKeywords; ++k)
				{
					BGSKeyword* keyword = effect->keywordForm.keywords[k];
					if (keyword && keyword->keyword.data)
					{
						if (strstr(keyword->keyword.data, keywordEditorId) != nullptr)
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	// Check if spell is a fire damage spell
	static bool IsFireSpell(SpellItem* spell)
	{
		return SpellHasKeyword(spell, "MagicDamageFire");
	}

	// ============================================
	// Equip State Manager Implementation
	// ============================================

	EquipStateManager::EquipStateManager()
	{
	}

	EquipStateManager::~EquipStateManager()
	{
	}

	void EquipStateManager::Initialize()
	{
		_MESSAGE("EquipStateManager: Initialized");
	}

	// ============================================
	// Equip/Unequip Visual Armor Helpers
	// ============================================
	void EquipStateManager::EquipVisualArmor(UInt32 armorFormId)
	{
		if (armorFormId == 0)
			return;

		TESForm* armorForm = LookupFormByID(armorFormId);
		if (!armorForm)
		{
			_MESSAGE("[EquipState] EquipVisualArmor: Armor form %08X not found", armorFormId);
			return;
		}

		Actor* player = (*g_thePlayer);
		if (!player)
			return;

		// Add the armor to player's inventory (silent = true)
		TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
		AddItem_Native(nullptr, 0, playerRef, armorForm, 1, true);
		_MESSAGE("[EquipState] Added armor %08X to inventory (silent)", armorFormId);

		// Start a thread that waits 50ms then queues the equip task
		std::thread equipThread(DelayedEquipThread, armorFormId, 15);
		equipThread.detach();
	}

	void EquipStateManager::UnequipVisualArmor(UInt32 armorFormId)
	{
		if (armorFormId == 0)
			return;

		TESForm* armorForm = LookupFormByID(armorFormId);
		if (!armorForm)
			return;

		Actor* player = (*g_thePlayer);
		if (!player)
			return;

		EquipManager* equipMan = EquipManager::GetSingleton();
		if (!equipMan)
			return;

		// UnequipItem params: actor, item, extraData, count, slot, unkFlag1, preventEquip, unkFlag2, unkFlag3, unk
		// All flags false for silent unequip
		CALL_MEMBER_FN(equipMan, UnequipItem)(player, armorForm, nullptr, 1, nullptr, false, false, false, false, nullptr);
		_MESSAGE("[EquipState] Unequipped armor %08X (silent)", armorFormId);

		// Remove the armor from inventory (silent = true)
		TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
		RemoveItemFromInventory(playerRef, armorForm, 1, true);
		_MESSAGE("[EquipState] Removed armor %08X from inventory (silent)", armorFormId);
	}

	// ============================================
	// Unequip and Remove Weapon Helper
	// ============================================
	void EquipStateManager::UnequipAndRemoveWeapon(UInt32 weaponFormId, const char* weaponName)
	{
		if (weaponFormId == 0)
		{
			_MESSAGE("[EquipState] UnequipAndRemoveWeapon: No weapon form ID provided");
			return;
		}

		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[EquipState] UnequipAndRemoveWeapon: Player not available");
			return;
		}

		EquipManager* equipMan = EquipManager::GetSingleton();
		if (!equipMan)
		{
			_MESSAGE("[EquipState] UnequipAndRemoveWeapon: EquipManager not available");
			return;
		}

		TESForm* weaponForm = LookupFormByID(weaponFormId);
		if (!weaponForm)
		{
			_MESSAGE("[EquipState] UnequipAndRemoveWeapon: Weapon form %08X not found", weaponFormId);
			return;
		}

		TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);

		// Unequip the weapon (silent)
		CALL_MEMBER_FN(equipMan, UnequipItem)(player, weaponForm, nullptr, 1, nullptr, false, false, false, false, nullptr);
		_MESSAGE("[EquipState] Unequipped %s weapon %08X (silent)", weaponName, weaponFormId);

		// Remove the weapon from inventory (silent)
		RemoveItemFromInventory(playerRef, weaponForm, 1, true);
		_MESSAGE("[EquipState] Removed %s weapon %08X from inventory (silent)", weaponName, weaponFormId);
	}

	// ============================================
	// Unequip and Remove Empty Pipe (for pipe filling)
	// ============================================
	void EquipStateManager::UnequipAndRemoveEmptyPipe(bool fromLeftHand, bool fromRightHand)
	{
		// Determine which empty pipe weapon to remove and which herb pipe to equip based on what's equipped
		UInt32 emptyWeaponFormId = 0;
		UInt32 herbWeaponFormId = 0;
		const char* emptyPipeName = "Unknown";
		const char* herbPipeName = "Unknown";
		const char* herbPipeBaseName = "Unknown"; // Base name without category suffix
		bool isWoodenPipe = false;
		bool isBonePipe = false;
		bool wasInLeftHand = false;
		bool wasInRightHand = false;
		SmokableCategory smokableCategory = SmokableCategory::None;

		if (g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight)
		{
			emptyWeaponFormId = g_emptyWoodenPipeWeaponFullFormId;
			herbWeaponFormId = g_herbWoodenPipeWeaponFullFormId;
			emptyPipeName = "Empty Wooden Pipe";
			herbPipeName = "Herb Wooden Pipe";
			herbPipeBaseName = "Herb Wooden Pipe";
			isWoodenPipe = true;
			wasInLeftHand = g_emptyWoodenPipeEquippedLeft;
			wasInRightHand = g_emptyWoodenPipeEquippedRight;
			smokableCategory = g_filledWoodenPipeSmokableCategory;
			_MESSAGE("[PipeFill] Detected WOODEN pipe being filled");
		}
		else if (g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight)
		{
			emptyWeaponFormId = g_emptyBonePipeWeaponFullFormId;
			herbWeaponFormId = g_herbBonePipeWeaponFullFormId;
			emptyPipeName = "Empty Bone Pipe";
			herbPipeName = "Herb Bone Pipe";
			herbPipeBaseName = "Herb Bone Pipe";
			isBonePipe = true;
			wasInLeftHand = g_emptyBonePipeEquippedLeft;
			wasInRightHand = g_emptyBonePipeEquippedRight;
			smokableCategory = g_filledBonePipeSmokableCategory;
			_MESSAGE("[PipeFill] Detected BONE pipe being filled");
		}

		if (emptyWeaponFormId == 0)
		{
			_MESSAGE("[EquipState] UnequipAndRemoveEmptyPipe: No empty pipe weapon form ID found");
			return;
		}

		_MESSAGE("[PipeFill] Unequipping %s (wasInLeft=%d, wasInRight=%d)", emptyPipeName, wasInLeftHand ? 1 : 0, wasInRightHand ? 1 : 0);

		// NOTE: Finger restore skip is now handled by checking g_herbPipeFlippedLongEnough
		// in the unequip handlers, not by a flag

		// Unequip and remove the herb pipe visual armor
		if (g_emptyWoodenPipeEquippedLeft)
			UnequipVisualArmor(g_emptyWoodenPipeUnlitVisualLeftArmorFullFormId);
		if (g_emptyWoodenPipeEquippedRight)
			UnequipVisualArmor(g_emptyWoodenPipeUnlitVisualRightArmorFullFormId);
		if (g_emptyBonePipeEquippedLeft)
			UnequipVisualArmor(g_emptyBonePipeUnlitVisualLeftArmorFullFormId);
		if (g_emptyBonePipeEquippedRight)
			UnequipVisualArmor(g_emptyBonePipeUnlitVisualRightArmorFullFormId);

		// Unequip and remove the empty weapon
		UnequipAndRemoveWeapon(emptyWeaponFormId, emptyPipeName);

		// Clear the equipped flags
		g_emptyWoodenPipeEquippedLeft = false;
		g_emptyWoodenPipeEquippedRight = false;
		g_emptyBonePipeEquippedLeft = false;
		g_emptyBonePipeEquippedRight = false;
		_MESSAGE("[EquipState] Cleared empty pipe equipped flags");

		// Set the display name with category suffix BEFORE equipping
		_MESSAGE("[PipeFill] About to set display name: herbWeaponFormId=%08X, baseName='%s', category=%d (%s)",
			herbWeaponFormId, herbPipeBaseName, static_cast<int>(smokableCategory), 
			SmokableIngredients::GetCategoryName(smokableCategory));
		
		if (herbWeaponFormId != 0 && smokableCategory != SmokableCategory::None)
		{
			SetWeaponDisplayName(herbWeaponFormId, herbPipeBaseName, smokableCategory);
		}
		else
		{
			_MESSAGE("[PipeFill] WARNING: Skipping SetWeaponDisplayName - herbWeaponFormId=%08X, category=%d",
				herbWeaponFormId, static_cast<int>(smokableCategory));
		}

		// Now add and equip the herb-filled pipe to the same hand
		if (herbWeaponFormId != 0)
		{
			Actor* player = (*g_thePlayer);
			if (player)
			{
				TESForm* herbWeaponForm = LookupFormByID(herbWeaponFormId);
				if (herbWeaponForm)
				{
					TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
					
					// Add the herb pipe to inventory (silent)
					AddItem_Native(nullptr, 0, playerRef, herbWeaponForm, 1, true);
					_MESSAGE("[PipeFill] Added %s weapon %08X to inventory (silent)", herbPipeName, herbWeaponFormId);

					// wasInLeftHand refers to VR controller, need to convert to game hand
					bool equipToGameLeftHand;
					if (IsLeftHandedMode())
					{
						// In left-handed mode: left VR controller = right game hand
						equipToGameLeftHand = !wasInLeftHand;
						_MESSAGE("[PipeFill]   -> Left-handed mode: VR %s -> Game %s", 
							wasInLeftHand ? "LEFT" : "RIGHT",
							equipToGameLeftHand ? "LEFT" : "RIGHT");
					}
					else
					{
						equipToGameLeftHand = wasInLeftHand;
					}

					// Equip the herb pipe to the correct game hand after a 20ms delay
					std::thread equipThread(DelayedEquipWeaponThread, herbWeaponFormId, equipToGameLeftHand, 20);
					equipThread.detach();
					_MESSAGE("[PipeFill] Scheduled %s to equip to game %s hand in 20ms", herbPipeName, equipToGameLeftHand ? "LEFT" : "RIGHT");
				}
				else
				{
					_MESSAGE("[PipeFill] ERROR: Herb weapon form %08X not found!", herbWeaponFormId);
					// Reset the skip flag since we failed
					g_skipFingerRestoreOnUnequip = false;
				}
			}
		}
		else
		{
			_MESSAGE("[PipeFill] ERROR: No herb weapon form ID available!");
			// Reset the skip flag since we failed
			g_skipFingerRestoreOnUnequip = false;
		}
	}

	// ============================================
	// Unequip Herb Pipe and Equip Empty (for pipe emptying/dumping)
	// ============================================
	void EquipStateManager::UnequipHerbPipeAndEquipEmpty(bool fromLeftHand, bool fromRightHand)
	{
		// NOTE: fromLeftHand/fromRightHand are already GAME hands (converted by caller in HandleHerbPipeEmptied)
		// Do NOT convert them again!

		// Determine which herb pipe weapon to remove and which empty pipe to equip
		// We need to check which herb pipe is currently equipped by checking the player's equipped items
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[PipeEmpty] Player not available");
			return;
		}

		UInt32 herbWeaponFormId = 0;
		UInt32 emptyWeaponFormId = 0;
		const char* herbPipeName = "Unknown";
		const char* emptyPipeName = "Unknown";
		bool isWoodenPipe = false;
		bool isBonePipe = false;

		// Check what's equipped in the relevant hand
		TESForm* equippedItem = nullptr;
		if (fromLeftHand)
		{
			equippedItem = player->GetEquippedObject(true); // true = left hand
		}
		else if (fromRightHand)
		{
			equippedItem = player->GetEquippedObject(false); // false = right hand
		}

		if (equippedItem)
		{
			// Check if it's a herb wooden pipe
			if (IsHerbWoodenPipeWeapon(equippedItem->formID))
			{
				herbWeaponFormId = g_herbWoodenPipeWeaponFullFormId;
				emptyWeaponFormId = g_emptyWoodenPipeWeaponFullFormId;
				herbPipeName = "Herb Wooden Pipe";
				emptyPipeName = "Empty Wooden Pipe";
				isWoodenPipe = true;
				_MESSAGE("[PipeEmpty] Detected WOODEN herb pipe being emptied");
			}
			// Check if it's a herb bone pipe
			else if (IsHerbBonePipeWeapon(equippedItem->formID))
			{
				herbWeaponFormId = g_herbBonePipeWeaponFullFormId;
				emptyWeaponFormId = g_emptyBonePipeWeaponFullFormId;
				herbPipeName = "Herb Bone Pipe";
				emptyPipeName = "Empty Bone Pipe";
			 isBonePipe = true;
				_MESSAGE("[PipeEmpty] Detected BONE herb pipe being emptied");
			}
		}

		if (herbWeaponFormId == 0)
		{
			_MESSAGE("[PipeEmpty] ERROR: No herb pipe weapon found equipped!");
			return;
		}

		_MESSAGE("[PipeEmpty] Unequipping %s (fromLeft=%d, fromRight=%d)", herbPipeName, fromLeftHand ? 1 : 0, fromRightHand ? 1 : 0);

		// NOTE: Finger restore skip is now handled by checking g_herbPipeFlippedLongEnough
		// in the unequip handlers, not by a flag

		// Unequip and remove the herb pipe visual armor
		if (isWoodenPipe)
		{
			if (fromLeftHand)
				UnequipVisualArmor(g_herbWoodenPipeUnlitVisualLeftArmorFullFormId);
			if (fromRightHand)
				UnequipVisualArmor(g_herbWoodenPipeUnlitVisualRightArmorFullFormId);
		}
		else if (isBonePipe)
		{
			if (fromLeftHand)
				UnequipVisualArmor(g_herbBonePipeUnlitVisualLeftArmorFullFormId);
			if (fromRightHand)
				UnequipVisualArmor(g_herbBonePipeUnlitVisualRightArmorFullFormId);
		}

		// Unequip and remove the herb pipe weapon
		UnequipAndRemoveWeapon(herbWeaponFormId, herbPipeName);

		// Now add and equip the empty pipe to the same hand
		if (emptyWeaponFormId != 0)
		{
			TESForm* emptyWeaponForm = LookupFormByID(emptyWeaponFormId);
			if (emptyWeaponForm)
			{
				TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
				
				// Add the empty pipe to inventory (silent)
				AddItem_Native(nullptr, 0, playerRef, emptyWeaponForm, 1, true);
				_MESSAGE("[PipeEmpty] Added %s weapon %08X to inventory (silent)", emptyPipeName, emptyWeaponFormId);

				// fromLeftHand/fromRightHand are ALREADY game hands (converted by caller)
				// Do NOT convert again - just use them directly
				bool equipToGameLeftHand = fromLeftHand;
				_MESSAGE("[PipeEmpty]   -> Equipping to game %s hand (already game hands from caller)", 
					equipToGameLeftHand ? "LEFT" : "RIGHT");

				// Equip the empty pipe to the correct game hand after a 20ms delay
				std::thread equipThread(DelayedEquipWeaponThread, emptyWeaponFormId, equipToGameLeftHand, 20);
				equipThread.detach();
				_MESSAGE("[PipeEmpty] Scheduled %s to equip to game %s hand in 20ms", emptyPipeName, equipToGameLeftHand ? "LEFT" : "RIGHT");
			}
			else
			{
				_MESSAGE("[PipeEmpty] ERROR: Empty weapon form %08X not found!", emptyWeaponFormId);
				// Reset the skip flag since we failed
				g_skipFingerRestoreOnUnequip = false;
			}
		}
		else
		{
			_MESSAGE("[PipeEmpty] ERROR: No empty weapon form ID available!");
			// Reset the skip flag since we failed
			g_skipFingerRestoreOnUnequip = false;
		}
	}

	// ============================================
	// Deplete Lit Pipe to Empty Pipe (after 25 inhales)
	// ============================================
	void EquipStateManager::DepleteLitPipeToEmpty()
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[Deplete] Player not available");
			return;
		}

		// Determine which lit pipe is equipped
		UInt32 litWeaponFormId = 0;
		UInt32 emptyWeaponFormId = 0;
		const char* litName = "Unknown";
		const char* emptyName = "Unknown";
		bool inLeftHand = false;
		bool inRightHand = false;

		// Check left hand
		TESForm* leftItem = player->GetEquippedObject(true);
		if (leftItem)
		{
			if (IsWoodenPipeLitWeapon(leftItem->formID))
			{
				litWeaponFormId = g_woodenPipeLitWeaponFullFormId;
				emptyWeaponFormId = g_emptyWoodenPipeWeaponFullFormId;
				litName = "Wooden Pipe Lit";
				emptyName = "Empty Wooden Pipe";
				inLeftHand = true;
			}
			else if (IsBonePipeLitWeapon(leftItem->formID))
			{
				litWeaponFormId = g_bonePipeLitWeaponFullFormId;
				emptyWeaponFormId = g_emptyBonePipeWeaponFullFormId;
				litName = "Bone Pipe Lit";
				emptyName = "Empty Bone Pipe";
				inLeftHand = true;
			}
		}

		// Check right hand if not found in left
		if (litWeaponFormId == 0)
		{
			TESForm* rightItem = player->GetEquippedObject(false);
			if (rightItem)
			{
				if (IsWoodenPipeLitWeapon(rightItem->formID))
				{
					litWeaponFormId = g_woodenPipeLitWeaponFullFormId;
					emptyWeaponFormId = g_emptyWoodenPipeWeaponFullFormId;
					litName = "Wooden Pipe Lit";
					emptyName = "Empty Wooden Pipe";
					inRightHand = true;
				}
				else if (IsBonePipeLitWeapon(rightItem->formID))
				{
					litWeaponFormId = g_bonePipeLitWeaponFullFormId;
					emptyWeaponFormId = g_emptyBonePipeWeaponFullFormId;
					litName = "Bone Pipe Lit";
					emptyName = "Empty Bone Pipe";
					inRightHand = true;
				}
			}
		}

		if (litWeaponFormId == 0 || emptyWeaponFormId == 0)
		{
			_MESSAGE("[Deplete] ERROR: No lit pipe found equipped!");
			return;
		}

		_MESSAGE("[Deplete] Swapping %s -> %s (hand: %s)", litName, emptyName, inLeftHand ? "LEFT" : "RIGHT");

		// Set flag to skip VRIK finger restoration during this transition
		g_skipFingerRestoreOnUnequip = true;

		// 1. Unequip and remove lit weapon
		UnequipAndRemoveWeapon(litWeaponFormId, litName);

		// 2. Add empty pipe weapon to inventory and equip after 15ms delay
		TESForm* emptyWeaponForm = LookupFormByID(emptyWeaponFormId);
		if (emptyWeaponForm)
		{
			TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
			AddItem_Native(nullptr, 0, playerRef, emptyWeaponForm, 1, true);
			_MESSAGE("[Deplete] Added %s to inventory", emptyName);

			// Equip the empty pipe to the same hand after a 15ms delay
			bool equipToLeft = inLeftHand;
			std::thread equipThread(DelayedEquipWeaponThread, emptyWeaponFormId, equipToLeft, 15);
			equipThread.detach();
			_MESSAGE("[Deplete] Scheduled %s to equip to %s hand in 15ms", emptyName, equipToLeft ? "LEFT" : "RIGHT");
		}
		else
		{
			_MESSAGE("[Deplete] ERROR: Empty weapon form %08X not found!", emptyWeaponFormId);
			g_skipFingerRestoreOnUnequip = false; // Reset flag on error
		}

		// Update VR tracker to stop tracking lit item
		if (g_vrInputTracker)
		{
			g_vrInputTracker->SetLitItemEquippedHand(false, false);
		}
	}

	// ============================================
	// Unequip Current Smokable (for hand swapping)
	// ============================================
	void EquipStateManager::UnequipCurrentSmokable(bool fromLeftVRController)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[HandSwap] Player not available");
			return;
		}

		// Convert VR controller hand to game hand (accounts for left-handed mode)
		bool gameLeftHand;
		if (IsLeftHandedMode())
		{
			// In left-handed mode: left VR controller = right game hand, right VR controller = left game hand
			gameLeftHand = !fromLeftVRController;
		}
		else
		{
			// In right-handed mode: left VR controller = left game hand
			gameLeftHand = fromLeftVRController;
		}

		_MESSAGE("[HandSwap] UnequipCurrentSmokable: VR controller=%s, game hand=%s (left-handed=%d)",
			fromLeftVRController ? "LEFT" : "RIGHT",
			gameLeftHand ? "LEFT" : "RIGHT",
			IsLeftHandedMode() ? 1 : 0);

		// Get what's equipped in that game hand
		TESForm* equippedItem = player->GetEquippedObject(gameLeftHand);
		if (!equippedItem)
		{
			_MESSAGE("[HandSwap] No item equipped in game %s hand", gameLeftHand ? "LEFT" : "RIGHT");
			return;
		}

		_MESSAGE("[HandSwap] Found equipped item: FormID=%08X", equippedItem->formID);

		// Log preserved smokable effects - depends on which lit item is equipped
		// For hand swap, we need to determine which type is being swapped
		// Check the equipped item to determine which cache to log
		if (equippedItem)
		{
			const char* smokableName = "Unknown";
			const char* categoryName = "None";
			UInt32 smokableFormId = 0;

			if (IsWoodenPipeLitWeapon(equippedItem->formID))
			{
				smokableFormId = g_filledWoodenPipeSmokableFormId;
				if (smokableFormId != 0)
				{
					smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
					categoryName = SmokableIngredients::GetCategoryName(g_filledWoodenPipeSmokableCategory);
				}
				_MESSAGE("[HandSwap] *** PRESERVING WOODEN PIPE SMOKABLE EFFECTS: '%s' (%s) FormID=%08X ***", 
					smokableName, categoryName, smokableFormId);
			}
			else if (IsBonePipeLitWeapon(equippedItem->formID))
			{
				smokableFormId = g_filledBonePipeSmokableFormId;
				if (smokableFormId != 0)
				{
					smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
					categoryName = SmokableIngredients::GetCategoryName(g_filledBonePipeSmokableCategory);
				}
				_MESSAGE("[HandSwap] *** PRESERVING BONE PIPE SMOKABLE EFFECTS: '%s' (%s) FormID=%08X ***", 
					smokableName, categoryName, smokableFormId);
			}
			else if (IsRolledSmokeLitWeapon(equippedItem->formID))
			{
				smokableFormId = g_filledRolledSmokeSmokableFormId;
				if (smokableFormId != 0)
				{
					smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
					categoryName = SmokableIngredients::GetCategoryName(g_filledRolledSmokeSmokableCategory);
				}
				_MESSAGE("[HandSwap] *** PRESERVING ROLLED SMOKE SMOKABLE EFFECTS: '%s' (%s) FormID=%08X ***", 
					smokableName, categoryName, smokableFormId);
			}
		}
		// Get the EquipManager to unequip
		EquipManager* equipMan = EquipManager::GetSingleton();
		if (!equipMan)
		{
			_MESSAGE("[HandSwap] EquipManager not available");
			return;
		}

		// Set flag to prevent inventory removal during hand swap
		// This is critical for lit items which should NOT be removed from inventory
		// It also ensures smokable effects are NOT cleared
		g_isHandSwapUnequip = true;
		_MESSAGE("[HandSwap] Set g_isHandSwapUnequip=true (preventing inventory removal and effect clearing)");

		// Get the appropriate slot for this hand
		BGSEquipSlot* slot = gameLeftHand ? GetLeftHandSlot() : GetRightHandSlot();

		// Unequip the item (silent)
		CALL_MEMBER_FN(equipMan, UnequipItem)(player, equippedItem, nullptr, 1, slot, false, false, false, false, nullptr);
		_MESSAGE("[HandSwap] Unequipped item %08X from game %s hand (silent)", equippedItem->formID, gameLeftHand ? "LEFT" : "RIGHT");

		// Re-equip the same item to the OPPOSITE game hand after a short delay
		bool oppositeGameLeftHand = !gameLeftHand;
		std::thread equipThread(DelayedEquipWeaponThread, equippedItem->formID, oppositeGameLeftHand, 15);
		equipThread.detach();
		_MESSAGE("[HandSwap] Scheduled re-equip to game %s hand in 50ms", oppositeGameLeftHand ? "LEFT" : "RIGHT");
	}

	// ============================================
	// Equip Empty Wooden Pipe (for crafting)
	// ============================================
	void EquipStateManager::EquipEmptyWoodenPipe(bool inLeftHand)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[Crafting] Player not available");
			return;
		}

		_MESSAGE("[Crafting] Equipping Empty Wooden Pipe to %s hand", inLeftHand ? "LEFT" : "RIGHT");

		// Add empty wooden pipe weapon to inventory
		TESForm* emptyPipeForm = LookupFormByID(g_emptyWoodenPipeWeaponFullFormId);
		if (emptyPipeForm)
		{
			TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
			AddItem_Native(nullptr, 0, playerRef, emptyPipeForm, 1, true);
			_MESSAGE("[Crafting] Added Empty Wooden Pipe to inventory");

			// Equip after a short delay
			std::thread equipThread(DelayedEquipWeaponThread, g_emptyWoodenPipeWeaponFullFormId, inLeftHand, 15);
			equipThread.detach();
			_MESSAGE("[Crafting] Scheduled Empty Wooden Pipe to equip to %s hand in 15ms", inLeftHand ? "LEFT" : "RIGHT");
		}
		else
		{
			_MESSAGE("[Crafting] ERROR: Empty Wooden Pipe form %08X not found!", g_emptyWoodenPipeWeaponFullFormId);
		}
	}

	// ============================================
	// Equip Empty Bone Pipe (for crafting)
	// ============================================
	void EquipStateManager::EquipEmptyBonePipe(bool inLeftHand)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[Crafting] Player not available");
			return;
		}

		_MESSAGE("[Crafting] Equipping Empty Bone Pipe to %s hand", inLeftHand ? "LEFT" : "RIGHT");

		// Add empty bone pipe weapon to inventory
		TESForm* emptyPipeForm = LookupFormByID(g_emptyBonePipeWeaponFullFormId);
		if (emptyPipeForm)
		{
			TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
			AddItem_Native(nullptr, 0, playerRef, emptyPipeForm, 1, true);
			_MESSAGE("[Crafting] Added Empty Bone Pipe to inventory");

			// Equip after a short delay
			std::thread equipThread(DelayedEquipWeaponThread, g_emptyBonePipeWeaponFullFormId, inLeftHand, 15);
			equipThread.detach();
			_MESSAGE("[Crafting] Scheduled Empty Bone Pipe to equip to %s hand in 15ms", inLeftHand ? "LEFT" : "RIGHT");
		}
		else
		{
			_MESSAGE("[Crafting] ERROR: Empty Bone Pipe form %08X not found!", g_emptyBonePipeWeaponFullFormId);
		}
	}

	// ============================================
	// Equip Unlit Rolled Smoke (for smoke rolling)
	// ============================================
	void EquipStateManager::EquipUnlitRolledSmoke(bool inLeftHand)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[SmokeRolling] Player not available");
			return;
		}

		_MESSAGE("[SmokeRolling] Equipping Unlit Rolled Smoke to %s hand", inLeftHand ? "LEFT" : "RIGHT");

		// Set the display name with category suffix BEFORE equipping
		if (g_rolledSmokeWeaponFullFormId != 0 && g_filledRolledSmokeSmokableCategory != SmokableCategory::None)
		{
			SetWeaponDisplayName(g_rolledSmokeWeaponFullFormId, "Rolled Smoke", g_filledRolledSmokeSmokableCategory);
		}

		// Add unlit rolled smoke weapon to inventory
		TESForm* rolledSmokeForm = LookupFormByID(g_rolledSmokeWeaponFullFormId);
		if (rolledSmokeForm)
		{
			TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
			AddItem_Native(nullptr, 0, playerRef, rolledSmokeForm, 1, true);
			_MESSAGE("[SmokeRolling] Added Unlit Rolled Smoke to inventory");

			// Equip after a short delay (20ms as requested)
			std::thread equipThread(DelayedEquipWeaponThread, g_rolledSmokeWeaponFullFormId, inLeftHand, 20);
			equipThread.detach();
			_MESSAGE("[SmokeRolling] Scheduled Unlit Rolled Smoke to equip to %s hand in 20ms", inLeftHand ? "LEFT" : "RIGHT");
		}
		else
		{
			_MESSAGE("[SmokeRolling] ERROR: Unlit Rolled Smoke form %08X not found!", g_rolledSmokeWeaponFullFormId);
		}
	}

	// ============================================
	// Light Herb Pipe (swap unlit for lit variant)
	// ============================================
	void EquipStateManager::LightHerbPipe(bool inLeftHand, bool inRightHand)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[Lighting] Player not available");
			return;
		}

		// Determine which herb pipe is equipped and get the lit variant
		UInt32 unlitWeaponFormId = 0;
		UInt32 litWeaponFormId = 0;
		const char* unlitName = "Unknown";
		const char* unlitBaseName = "Unknown"; // Base name for restoring display name
		const char* litName = "Unknown";
		bool isWoodenPipe = false;
		bool isBonePipe = false;

		// Check what's equipped in the relevant hand
		TESForm* equippedItem = nullptr;
		if (inLeftHand)
		{
			equippedItem = player->GetEquippedObject(true); // true = left hand
		}
		else if (inRightHand)
		{
			equippedItem = player->GetEquippedObject(false); // false = right hand
		}

		if (equippedItem)
		{
			if (IsHerbWoodenPipeWeapon(equippedItem->formID))
			{
				unlitWeaponFormId = g_herbWoodenPipeWeaponFullFormId;
				litWeaponFormId = g_woodenPipeLitWeaponFullFormId;
				unlitName = "Herb Wooden Pipe";
				unlitBaseName = "Herb Wooden Pipe";
				litName = "Wooden Pipe Lit";
				isWoodenPipe = true;
				_MESSAGE("[Lighting] Detected WOODEN herb pipe to light");
			}
			else if (IsHerbBonePipeWeapon(equippedItem->formID))
			{
				unlitWeaponFormId = g_herbBonePipeWeaponFullFormId;
				litWeaponFormId = g_bonePipeLitWeaponFullFormId;
				unlitName = "Herb Bone Pipe";
				unlitBaseName = "Herb Bone Pipe";
				litName = "Bone Pipe Lit";
				isBonePipe = true;
				_MESSAGE("[Lighting] Detected BONE herb pipe to light");
			}
		}

		if (unlitWeaponFormId == 0 || litWeaponFormId == 0)
		{
			_MESSAGE("[Lighting] ERROR: Could not determine herb pipe type to light!");
			return;
		}

		// Log the preserved smokable effects from the type-specific cache
		if (isWoodenPipe)
		{
			if (g_filledWoodenPipeSmokableFormId != 0)
			{
				const char* smokableName = SmokableIngredients::GetSmokableName(g_filledWoodenPipeSmokableFormId);
				const char* categoryName = SmokableIngredients::GetCategoryName(g_filledWoodenPipeSmokableCategory);
				_MESSAGE("[Lighting] Smokable effects preserved from WOODEN PIPE cache: '%s' (%s) - FormID: %08X", smokableName, categoryName, g_filledWoodenPipeSmokableFormId);
			}
			else
			{
				_MESSAGE("[Lighting] WARNING: No smokable effects cached in WOODEN PIPE cache");
			}
		}
		else if (isBonePipe)
		{
			if (g_filledBonePipeSmokableFormId != 0)
			{
				const char* smokableName = SmokableIngredients::GetSmokableName(g_filledBonePipeSmokableFormId);
				const char* categoryName = SmokableIngredients::GetCategoryName(g_filledBonePipeSmokableCategory);
				_MESSAGE("[Lighting] Smokable effects preserved from BONE PIPE cache: '%s' (%s) - FormID: %08X", smokableName, categoryName, g_filledBonePipeSmokableFormId);
			}
			else
			{
				_MESSAGE("[Lighting] WARNING: No smokable effects cached in BONE PIPE cache");
			}
		}

		_MESSAGE("[Lighting] Lighting %s (inLeft=%d, inRight=%d)", unlitName, inLeftHand ? 1 : 0, inRightHand ? 1 : 0);

		// Set flag to skip VRIK finger restoration during this unequip
		g_skipFingerRestoreOnUnequip = true;
		_MESSAGE("[Lighting] Set skip finger restore flag (transitioning to lit pipe)");

		// Restore the weapon display name to base name (remove category suffix) before unequipping
		// This ensures the unlit weapon in inventory has its original name for next use
		RestoreWeaponDisplayName(unlitWeaponFormId, unlitBaseName);

		// Unequip and remove the unlit herb pipe weapon
		UnequipAndRemoveWeapon(unlitWeaponFormId, unlitName);

		// Add the lit pipe to inventory
		TESForm* litWeaponForm = LookupFormByID(litWeaponFormId);
		if (litWeaponForm)
		{
			TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
			AddItem_Native(nullptr, 0, playerRef, litWeaponForm, 1, true);
			_MESSAGE("[Lighting] Added %s weapon %08X to inventory (silent)", litName, litWeaponFormId);

			// Equip the lit pipe to the same hand after a 15ms delay
			bool equipToLeft = inLeftHand;
			std::thread equipThread(DelayedEquipWeaponThread, litWeaponFormId, equipToLeft, 15);
			equipThread.detach();
			_MESSAGE("[Lighting] Scheduled %s to equip to %s hand in 15ms", litName, equipToLeft ? "LEFT" : "RIGHT");
			
			// Log which type-specific cache will be used when lit item is equipped
			if (isWoodenPipe)
			{
				_MESSAGE("[Lighting] Smokable effects will apply when smoking from WOODEN PIPE cache: '%s' (%s)", 
					SmokableIngredients::GetSmokableName(g_filledWoodenPipeSmokableFormId),
					SmokableIngredients::GetCategoryName(g_filledWoodenPipeSmokableCategory));
			}
			else if (isBonePipe)
			{
				_MESSAGE("[Lighting] Smokable effects will apply when smoking from BONE PIPE cache: '%s' (%s)", 
					SmokableIngredients::GetSmokableName(g_filledBonePipeSmokableFormId),
					SmokableIngredients::GetCategoryName(g_filledBonePipeSmokableCategory));
			}
		}
		else
		{
			_MESSAGE("[Lighting] ERROR: Lit weapon form %08X not found!", litWeaponFormId);
			g_skipFingerRestoreOnUnequip = false;
		}
	}

	// ============================================
	// Light Rolled Smoke (swap unlit for lit variant)
	// ============================================
	void EquipStateManager::LightRolledSmoke(bool inLeftHand, bool inRightHand)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[Lighting] Player not available");
			return;
		}

		_MESSAGE("[Lighting] Lighting Rolled Smoke (inLeft=%d, inRight=%d)", inLeftHand ? 1 : 0, inRightHand ? 1 : 0);

		// Log the preserved smokable effects from the rolled smoke cache
		if (g_filledRolledSmokeSmokableFormId != 0)
		{
			const char* smokableName = SmokableIngredients::GetSmokableName(g_filledRolledSmokeSmokableFormId);
			const char* categoryName = SmokableIngredients::GetCategoryName(g_filledRolledSmokeSmokableCategory);
			_MESSAGE("[Lighting] Smokable effects preserved from ROLLED SMOKE cache: '%s' (%s) - FormID: %08X", smokableName, categoryName, g_filledRolledSmokeSmokableFormId);
		}
		else
		{
			_MESSAGE("[Lighting] WARNING: No smokable effects cached in ROLLED SMOKE cache");
		}

		// Set flag to skip VRIK finger restoration during this unequip
		g_skipFingerRestoreOnUnequip = true;
		_MESSAGE("[Lighting] Set skip finger restore flag (transitioning to lit smoke)");

		// Restore the weapon display name to base name (remove category suffix) before unequipping
		// This ensures the unlit weapon in inventory has its original name for next use
		RestoreWeaponDisplayName(g_rolledSmokeWeaponFullFormId, "Rolled Smoke");

		// Unequip and remove the unlit rolled smoke weapon
		UnequipAndRemoveWeapon(g_rolledSmokeWeaponFullFormId, "Rolled Smoke");

		// Add the lit rolled smoke to inventory
		TESForm* litWeaponForm = LookupFormByID(g_rolledSmokeLitWeaponFullFormId);
		if (litWeaponForm)
		{
			TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
			AddItem_Native(nullptr, 0, playerRef, litWeaponForm, 1, true);
			_MESSAGE("[Lighting] Added Rolled Smoke Lit weapon %08X to inventory (silent)", g_rolledSmokeLitWeaponFullFormId);

			// Equip the lit smoke to the same hand after a 15ms delay
			bool equipToLeft = inLeftHand;
			std::thread equipThread(DelayedEquipWeaponThread, g_rolledSmokeLitWeaponFullFormId, equipToLeft, 15);
			equipThread.detach();
			_MESSAGE("[Lighting] Scheduled Rolled Smoke Lit to equip to %s hand in 15ms", equipToLeft ? "LEFT" : "RIGHT");
			
			_MESSAGE("[Lighting] Smokable effects will apply when smoking from ROLLED SMOKE cache: '%s' (%s)", 
				SmokableIngredients::GetSmokableName(g_filledRolledSmokeSmokableFormId),
				SmokableIngredients::GetCategoryName(g_filledRolledSmokeSmokableCategory));
		}
		else
		{
			_MESSAGE("[Lighting] ERROR: Rolled Smoke Lit weapon form %08X not found!", g_rolledSmokeLitWeaponFullFormId);
			g_skipFingerRestoreOnUnequip = false;
		}
	}

	// ============================================
	// Product-Specific Equip Handlers - UNLIT
	// ============================================
	void EquipStateManager::HandleRolledSmokeEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		// Convert game hands to VR controller hands (accounts for left-handed mode)
	 bool vrLeftController, vrRightController;
	 GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = 0;
			
			// Use VR controller hand for visual armor selection
			if (vrLeftController)
			{
				visualArmorFormId = g_smokeUnlitVisualLeftArmorFullFormId;
			}
			else if (vrRightController)
			{
				visualArmorFormId = g_smokeUnlitVisualRightArmorFullFormId;
			}

			EquipVisualArmor(visualArmorFormId);

			// Set finger positions for holding the smoke using VRIK
			// Use VR controller hand for VRIK (isLeft refers to VR controller, not game hand)
			bool isLeftVRController = vrLeftController;
			_MESSAGE("[EquipState] vrikInterface ptr = %p", vrikInterface);
			if (vrikInterface)
			{
				float thumb1 = 0.100000f, thumb2 = 0.200000f;
				float index1 = 0.900000f, index2 = 0.890000f;
				float middle1 = 0.900000f, middle2 = 0.890000f;
				float ring1 = 0.000000f, ring2 = 0.000000f;
				float pinky1 = 0.090000f, pinky2 = 0.090000f;

				vrikInterface->setFingerRange(isLeftVRController, 
					thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				
				// Cache the finger positions
				CacheFingerPositions(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				
				_MESSAGE("[EquipState] Set VRIK finger range for %s VR controller (smoke unlit, game hand=%s)", 
					isLeftVRController ? "LEFT" : "RIGHT", HandStr(inLeftHand, inRightHand));
			}
			else
			{
				_MESSAGE("[EquipState] WARNING: vrikInterface is null, cannot set finger range");
			}

			// Start VR input tracking and set smoke item hand
			// Use VR controller hands for tracking
			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1)
				{
					g_vrInputTracker->StartTracking();
				}
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetUnlitRolledSmokeEquippedHand(vrLeftController, vrRightController);
			}
		}
		else
		{
			// On unequip, remove both left and right UNLIT variants (safe cleanup)
			UnequipVisualArmor(g_smokeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_smokeUnlitVisualRightArmorFullFormId);
			// Also remove LIT variants in case they were equipped
			UnequipVisualArmor(g_smokeLitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_smokeLitVisualRightArmorFullFormId);

			// Restore finger positions using VRIK
			_MESSAGE("[EquipState] vrikInterface ptr = %p (for restore)", vrikInterface);
			if (vrikInterface)
			{
				vrikInterface->restoreFingers(true);  // Left hand
				vrikInterface->restoreFingers(false); // Right hand
				_MESSAGE("[EquipState] Restored VRIK fingers for both hands (smoke)");
			}
			else
			{
				_MESSAGE("[EquipState] WARNING: vrikInterface is null, cannot restore fingers");
			}

			// Clear cached finger positions
			ClearCachedFingerPositions(true);  // Left
			ClearCachedFingerPositions(false); // Right

			// Clear unlit rolled smoke tracking
			if (g_vrInputTracker)
			{
				g_vrInputTracker->SetUnlitRolledSmokeEquippedHand(false, false);
			}

			// Stop VR input tracking if no more smoke items equipped
			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 0)
				{
					g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
					g_vrInputTracker->StopTracking();
				}
			}
		}
	}

	void EquipStateManager::HandleHerbWoodenPipeEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		// Convert game hands to VR controller hands (accounts for left-handed mode)
	 bool vrLeftController, vrRightController;
	 GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = 0;
			
			// Use VR controller hand for visual armor selection
			if (vrLeftController)
			{
				visualArmorFormId = g_herbWoodenPipeUnlitVisualLeftArmorFullFormId;
			}
			else if (vrRightController)
			{
				visualArmorFormId = g_herbWoodenPipeUnlitVisualRightArmorFullFormId;
			}

			_MESSAGE("[EquipState] HandleHerbWoodenPipeEquip: visualArmorFormId=%08X, vrLeftController=%d", visualArmorFormId, vrLeftController ? 1 : 0);
			EquipVisualArmor(visualArmorFormId);

			// Set finger positions for holding the pipe using VRIK
			// Use VR controller hand for VRIK
			bool isLeftVRController = vrLeftController;
			_MESSAGE("[EquipState] vrikInterface ptr = %p (herb wooden pipe)", vrikInterface);
			if (vrikInterface)
			{
				float thumb1 = 0.700000f, thumb2 = 0.600000f;
				float index1 = 0.700000f, index2 = 0.600000f;
				float middle1 = 0.020000f, middle2 = 0.010000f;
				float ring1 = 0.000000f, ring2 = 0.020000f;
				float pinky1 = 0.000000f, pinky2 = 0.000000f;

				vrikInterface->setFingerRange(isLeftVRController, 
					thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				
				// Cache the finger positions
				CacheFingerPositions(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				
				_MESSAGE("[EquipState] Set VRIK finger range for %s VR controller (herb wooden pipe unlit, game hand=%s)", 
					isLeftVRController ? "LEFT" : "RIGHT", HandStr(inLeftHand, inRightHand));
			}
			else
			{
				_MESSAGE("[EquipState] WARNING: vrikInterface is null, cannot set finger range (herb wooden pipe)");
			}

			// Start VR input tracking and set smoke item hand
			// Use VR controller hands for tracking
			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1)
				{
					g_vrInputTracker->StartTracking();
				}
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetHerbPipeEquippedHand(vrLeftController, vrRightController);
			}
		}
		else
		{
			// On unequip, remove both left and right UNLIT variants (safe cleanup)
			UnequipVisualArmor(g_herbWoodenPipeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_herbWoodenPipeUnlitVisualRightArmorFullFormId);
			// Also remove LIT variants in case they were equipped
			UnequipVisualArmor(g_smokeLitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_smokeLitVisualRightArmorFullFormId);

			// Restore finger positions using VRIK - BUT skip if controller is still flipped (emptying position)
			if (g_herbPipeFlippedLongEnough)
			{
				_MESSAGE("[EquipState] Skipping VRIK finger restore (controller still flipped/emptying)");
			}
			else
			{
				_MESSAGE("[EquipState] vrikInterface ptr = %p (herb wooden pipe restore)", vrikInterface);
				if (vrikInterface)
				{
					vrikInterface->restoreFingers(true);  // Left hand
					vrikInterface->restoreFingers(false); // Right hand
					_MESSAGE("[EquipState] Restored VRIK fingers for both hands (herb wooden pipe)");
				}
				else
				{
					_MESSAGE("[EquipState] WARNING: vrikInterface is null, cannot restore fingers (herb wooden pipe)");
				}
			}

			// Clear cached finger positions
			ClearCachedFingerPositions(true);  // Left
			ClearCachedFingerPositions(false); // Right

			// Clear herb pipe tracking
			if (g_vrInputTracker)
			{
				g_vrInputTracker->SetHerbPipeEquippedHand(false, false);
			}

			// Stop VR input tracking if no more smoke items equipped
			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 0)
				{
					g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
					g_vrInputTracker->StopTracking();
				}
			}
		}
	}

	void EquipStateManager::HandleHerbBonePipeEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		// Convert game hands to VR controller hands (accounts for left-handed mode)
	 bool vrLeftController, vrRightController;
	 GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = 0;
			
			// Use VR controller hand for visual armor selection
			if (vrLeftController)
			{
				visualArmorFormId = g_herbBonePipeUnlitVisualLeftArmorFullFormId;
			}
			else if (vrRightController)
			{
				visualArmorFormId = g_herbBonePipeUnlitVisualRightArmorFullFormId;
			}

			_MESSAGE("[EquipState] HandleHerbBonePipeEquip: visualArmorFormId=%08X, vrLeftController=%d", visualArmorFormId, vrLeftController ? 1 : 0);
			EquipVisualArmor(visualArmorFormId);

			// Set finger positions for holding the bone pipe using VRIK
			// Use VR controller hand for VRIK
			bool isLeftVRController = vrLeftController;
			_MESSAGE("[EquipState] vrikInterface ptr = %p (herb bone pipe)", vrikInterface);
			if (vrikInterface)
			{
				float thumb1 = 0.700000f, thumb2 = 0.600000f;
				float index1 = 0.700000f, index2 = 0.600000f;
				float middle1 = 0.020000f, middle2 = 0.010000f;
				float ring1 = 0.000000f, ring2 = 0.020000f;
				float pinky1 = 0.000000f, pinky2 = 0.000000f;

				vrikInterface->setFingerRange(isLeftVRController, 
					thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
			}

			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1)
					g_vrInputTracker->StartTracking();
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetHerbPipeEquippedHand(vrLeftController, vrRightController);
			}

			CheckForAlreadyGrabbedSmokable();
		}
		else
		{


			UnequipVisualArmor(g_herbBonePipeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_herbBonePipeUnlitVisualRightArmorFullFormId);

			if (g_skipFingerRestoreOnUnequip)
			{
				_MESSAGE("[EquipState] Skipping VRIK finger restore (pipe fill transition)");
				g_skipFingerRestoreOnUnequip = false;
			}
			else
			{
				if (vrikInterface)
				{
					vrikInterface->restoreFingers(true);
					vrikInterface->restoreFingers(false);
				}
			}

			ClearCachedFingerPositions(true);
			ClearCachedFingerPositions(false);

			// Clear herb pipe tracking
			if (g_vrInputTracker)
			{
				g_vrInputTracker->SetHerbPipeEquippedHand(false, false);
			}

			// Stop VR input tracking if no more smoke items equipped
			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 0)
				{
					g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
					g_vrInputTracker->StopTracking();
				}
			}
		}
	}

	void EquipStateManager::HandleEmptyWoodenPipeEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		bool vrLeftController, vrRightController;
		GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = 0;
			if (vrLeftController)
			{
				visualArmorFormId = g_emptyWoodenPipeUnlitVisualLeftArmorFullFormId;
				g_emptyWoodenPipeEquippedLeft = true;
			}
			else if (vrRightController)
			{
				visualArmorFormId = g_emptyWoodenPipeUnlitVisualRightArmorFullFormId;
				g_emptyWoodenPipeEquippedRight = true;
			}
			EquipVisualArmor(visualArmorFormId);

			bool isLeftVRController = vrLeftController;
			if (vrikInterface)
			{
				float thumb1 = 0.700000f, thumb2 = 0.600000f;
				float index1 = 0.700000f, index2 = 0.600000f;
				float middle1 = 0.020000f, middle2 = 0.010000f;
				float ring1 = 0.000000f, ring2 = 0.020000f;
				float pinky1 = 0.000000f, pinky2 = 0.000000f;
				vrikInterface->setFingerRange(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				CacheFingerPositions(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
			}
			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1) g_vrInputTracker->StartTracking();
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
			}
			CheckForAlreadyGrabbedSmokable();
		}
		else
		{
			g_emptyWoodenPipeEquippedLeft = false;
			g_emptyWoodenPipeEquippedRight = false;
			UnequipVisualArmor(g_emptyWoodenPipeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_emptyWoodenPipeUnlitVisualRightArmorFullFormId);
			if (g_skipFingerRestoreOnUnequip)
			{
				g_skipFingerRestoreOnUnequip = false;
			}
			else if (vrikInterface)
			{
				vrikInterface->restoreFingers(true);
				vrikInterface->restoreFingers(false);
			}
			ClearCachedFingerPositions(true);
			ClearCachedFingerPositions(false);
			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker && g_equippedSmokeItemCount == 0)
			{
				g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
				g_vrInputTracker->StopTracking();
			}
		}
	}

	void EquipStateManager::HandleEmptyBonePipeEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		bool vrLeftController, vrRightController;
		GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = 0;
			if (vrLeftController)
			{
				visualArmorFormId = g_emptyBonePipeUnlitVisualLeftArmorFullFormId;
				g_emptyBonePipeEquippedLeft = true;
			}
			else if (vrRightController)
			{
				visualArmorFormId = g_emptyBonePipeUnlitVisualRightArmorFullFormId;
				g_emptyBonePipeEquippedRight = true;
			}
			EquipVisualArmor(visualArmorFormId);

			bool isLeftVRController = vrLeftController;
			if (vrikInterface)
			{
				float thumb1 = 0.700000f, thumb2 = 0.600000f;
				float index1 = 0.700000f, index2 = 0.600000f;
				float middle1 = 0.020000f, middle2 = 0.010000f;
				float ring1 = 0.000000f, ring2 = 0.020000f;
				float pinky1 = 0.000000f, pinky2 = 0.000000f;
				vrikInterface->setFingerRange(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				CacheFingerPositions(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
			}
			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1) g_vrInputTracker->StartTracking();
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetHerbPipeEquippedHand(vrLeftController, vrRightController);
			}

			CheckForAlreadyGrabbedSmokable();
		}
		else
		{


			UnequipVisualArmor(g_emptyBonePipeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_emptyBonePipeUnlitVisualRightArmorFullFormId);

			if (g_skipFingerRestoreOnUnequip)
			{
				_MESSAGE("[EquipState] Skipping VRIK finger restore (pipe fill transition)");
				g_skipFingerRestoreOnUnequip = false;
			}
			else
			{
				if (vrikInterface)
				{
					vrikInterface->restoreFingers(true);
					vrikInterface->restoreFingers(false);
				}
			}

			ClearCachedFingerPositions(true);
			ClearCachedFingerPositions(false);

			// Clear herb pipe tracking
			if (g_vrInputTracker)
			{
				g_vrInputTracker->SetHerbPipeEquippedHand(false, false);
			}

			// Stop VR input tracking if no more smoke items equipped
			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 0)
				{
					g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
					g_vrInputTracker->StopTracking();
				}
			}
		}
	}

	// ============================================
	// Product-Specific Equip Handlers - LIT
	// ============================================
	void EquipStateManager::HandleRolledSmokeLitEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		bool vrLeftController, vrRightController;
		GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = vrLeftController ? g_smokeLitVisualLeftArmorFullFormId : g_smokeLitVisualRightArmorFullFormId;

			if (visualArmorFormId != 0)
			{
				TESForm* armorForm = LookupFormByID(visualArmorFormId);
				if (armorForm)
				{
					Actor* player = (*g_thePlayer);
					if ( player)
					{
						TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
						AddItem_Native(nullptr, 0, playerRef, armorForm, 1, true);
						std::thread equipThread(DelayedEquipThread, visualArmorFormId, 20);
						equipThread.detach();
					}
				}
			}

			bool isLeftVRController = vrLeftController;
			if (vrikInterface)
			{
				float thumb1 = 0.100000f, thumb2 = 0.200000f;
				float index1 = 0.900000f, index2 = 0.890000f;
				float middle1 = 0.900000f, middle2 = 0.890000f;
				float ring1 = 0.000000f, ring2 = 0.000000f;
				float pinky1 = 0.090000f, pinky2 = 0.090000f;

				vrikInterface->setFingerRange(isLeftVRController, 
					thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
				
				CacheFingerPositions(isLeftVRController, thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
			}

			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1)
					g_vrInputTracker->StartTracking();
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetLitItemEquippedHand(vrLeftController, vrRightController);
				}

			// Copy rolled smoke cache to active smokable for smoking mechanics
			g_activeSmokableFormId = g_filledRolledSmokeSmokableFormId;
			g_activeSmokableCategory = g_filledRolledSmokeSmokableCategory;
			_MESSAGE("[EquipState] Lit Rolled Smoke equipped - active smokable set from ROLLED SMOKE cache: FormID=%08X, Category=%s",
				g_activeSmokableFormId, SmokableIngredients::GetCategoryName(g_activeSmokableCategory));

			// Initialize smoking mechanics for this lit item
			InitializeSmokingMechanics();

			// Trigger glow node initialization for the lit smoke
			OnLitPipeEquipped();
		}
		else
		{
			// Unequip - remove visual armors
			UnequipVisualArmor(g_smokeLitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_smokeLitVisualRightArmorFullFormId);
			UnequipVisualArmor(g_smokeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_smokeUnlitVisualRightArmorFullFormId);

			if (g_isHandSwapUnequip)
			{
				_MESSAGE("[EquipState] Hand swap unequip - skipping inventory removal and effect clearing for lit smoke");
				g_isHandSwapUnequip = false;
			}
			else
			{
				// Clear the rolled smoke specific cache AND active smokable
				g_filledRolledSmokeSmokableFormId = 0;
				g_filledRolledSmokeSmokableCategory = SmokableCategory::None;
				g_activeSmokableFormId = 0;
				g_activeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[EquipState] Cleared ROLLED SMOKE cache and active smokable (lit rolled smoke unequipped)");

				// Reset smoking mechanics
				ResetSmokingMechanics();

				Actor* player = (*g_thePlayer);
				if (player)
				{
					TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
					if (g_rolledSmokeLitWeaponFullFormId != 0)
					{
						TESForm* litWeaponForm = LookupFormByID(g_rolledSmokeLitWeaponFullFormId);
						if (litWeaponForm)
							RemoveItemFromInventory(playerRef, litWeaponForm, 1, true);
					}
					if (g_rolledSmokeWeaponFullFormId != 0)
					{
						TESForm* unlitWeaponForm = LookupFormByID(g_rolledSmokeWeaponFullFormId);
						if (unlitWeaponForm)
							AddItem_Native(nullptr, 0, playerRef, unlitWeaponForm, 1, true);
					}
				}
			}

			if (vrikInterface)
			{
				vrikInterface->restoreFingers(true);
				vrikInterface->restoreFingers(false);
			}

			ClearCachedFingerPositions(true);
			ClearCachedFingerPositions(false);

			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker && g_equippedSmokeItemCount == 0)
			{
				g_vrInputTracker->SetLitItemEquippedHand(false, false);
				g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
				g_vrInputTracker->StopTracking();
			}
		}
	}

	void EquipStateManager::HandleWoodenPipeLitEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		bool vrLeftController, vrRightController;
		GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = vrLeftController ? g_smokeLitVisualLeftArmorFullFormId : g_smokeLitVisualRightArmorFullFormId;

			if (visualArmorFormId != 0)
			{
				TESForm* armorForm = LookupFormByID(visualArmorFormId);
				if (armorForm)
				{
					Actor* player = (*g_thePlayer);
					if ( player)
					{
						TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
						AddItem_Native(nullptr, 0, playerRef, armorForm, 1, true);
						std::thread equipThread(DelayedEquipThread, visualArmorFormId, 50);
						equipThread.detach();
						}
			}
			}

			// Use VR controller hand for VRIK
			bool isLeftVRController = vrLeftController;
			if (vrikInterface)
			{
				vrikInterface->setFingerRange(isLeftVRController, 
					0.700000f, 0.600000f,
					0.700000f, 0.600000f,
					0.020000f, 0.010000f,
					0.000000f, 0.020000f,
					0.000000f, 0.000000f);
				_MESSAGE("[EquipState] Set VRIK finger range for %s VR controller (wooden pipe lit, game hand=%s)", 
					isLeftVRController ? "LEFT" : "RIGHT", HandStr(inLeftHand, inRightHand));
			}

			// Use VR controller hands for tracking
			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1)
					g_vrInputTracker->StartTracking();
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetLitItemEquippedHand(vrLeftController, vrRightController);
			}

			// Copy wooden pipe cache to active smokable for smoking mechanics
			g_activeSmokableFormId = g_filledWoodenPipeSmokableFormId;
			g_activeSmokableCategory = g_filledWoodenPipeSmokableCategory;
			_MESSAGE("[EquipState] Lit Wooden Pipe equipped - active smokable set from WOODEN PIPE cache: FormID=%08X, Category=%s",
				g_activeSmokableFormId, SmokableIngredients::GetCategoryName(g_activeSmokableCategory));

			// Initialize smoking mechanics for this lit item
			InitializeSmokingMechanics();

			// Trigger glow node initialization for the lit pipe
			OnLitPipeEquipped();

			_MESSAGE("[EquipState] Wooden Pipe Lit EQUIPPED to %s VR controller (game hand=%s, count: %d)", 
				vrLeftController ? "LEFT" : "RIGHT", HandStr(inLeftHand, inRightHand), g_equippedSmokeItemCount);
		}
		else
		{
			UnequipVisualArmor(g_woodenPipeLitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_woodenPipeLitVisualRightArmorFullFormId);
			UnequipVisualArmor(g_herbWoodenPipeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_herbWoodenPipeUnlitVisualRightArmorFullFormId);

			// Skip inventory changes and effect clearing if this is a hand swap unequip
			if (g_isHandSwapUnequip)
			{
				_MESSAGE("[EquipState] Hand swap unequip - skipping inventory removal and effect clearing for lit wooden pipe");
				g_isHandSwapUnequip = false; // Reset the flag
			}
			else
			{
				// Clear the wooden pipe specific cache AND active smokable
				g_filledWoodenPipeSmokableFormId = 0;
				g_filledWoodenPipeSmokableCategory = SmokableCategory::None;
				g_activeSmokableFormId = 0;
				g_activeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[EquipState] Cleared WOODEN PIPE cache and active smokable (lit wooden pipe unequipped)");

				// Reset smoking mechanics
				ResetSmokingMechanics();

				Actor* player = (*g_thePlayer);
				if (player)
				{
					TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
					if (g_woodenPipeLitWeaponFullFormId != 0)
					{
						TESForm* litWeaponForm = LookupFormByID(g_woodenPipeLitWeaponFullFormId);
						if (litWeaponForm)
							RemoveItemFromInventory(playerRef, litWeaponForm, 1, true);
					}
					if (g_herbWoodenPipeWeaponFullFormId != 0)
					{
						TESForm* unlitWeaponForm = LookupFormByID(g_herbWoodenPipeWeaponFullFormId);
						if (unlitWeaponForm)
							AddItem_Native(nullptr, 0, playerRef, unlitWeaponForm, 1, true);
					}
				}
			}

			if (vrikInterface)
			{
				vrikInterface->restoreFingers(true);
				vrikInterface->restoreFingers(false);
			}

			ClearCachedFingerPositions(true);
			ClearCachedFingerPositions(false);

			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker && g_equippedSmokeItemCount == 0)
			{
				g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
				g_vrInputTracker->StopTracking();
			}
		}
	}

	void EquipStateManager::HandleBonePipeLitEquip(bool isEquip, bool inLeftHand, bool inRightHand)
	{
		bool vrLeftController, vrRightController;
		GetVRControllerHands(inLeftHand, inRightHand, vrLeftController, vrRightController);

		if (isEquip)
		{
			UInt32 visualArmorFormId = vrLeftController ? g_bonePipeLitVisualLeftArmorFullFormId : g_bonePipeLitVisualRightArmorFullFormId;
			_MESSAGE("[EquipState] HandleBonePipeLitEquip: vrLeftController=%d, visualArmorFormId=%08X (left=%08X, right=%08X)", vrLeftController ? 1 : 0, visualArmorFormId, g_bonePipeLitVisualLeftArmorFullFormId, g_bonePipeLitVisualRightArmorFullFormId);

			EquipVisualArmor(visualArmorFormId);

			bool isLeftVRController = vrLeftController;
			if (vrikInterface)
			{
				float thumb1 = 0.700000f, thumb2 = 0.600000f;
				float index1 = 0.700000f, index2 = 0.600000f;
				float middle1 = 0.020000f, middle2 = 0.010000f;
				float ring1 = 0.000000f, ring2 = 0.020000f;
				float pinky1 = 0.000000f, pinky2 = 0.000000f;

				vrikInterface->setFingerRange(isLeftVRController, 
					thumb1, thumb2, index1, index2, middle1, middle2, ring1, ring2, pinky1, pinky2);
			}

			g_equippedSmokeItemCount++;
			if (g_vrInputTracker)
			{
				if (g_equippedSmokeItemCount == 1)
					g_vrInputTracker->StartTracking();
				g_vrInputTracker->SetSmokeItemEquippedHand(vrLeftController, vrRightController);
				g_vrInputTracker->SetLitItemEquippedHand(vrLeftController, vrRightController);
			}

			// Copy bone pipe cache to active smokable for smoking mechanics
			g_activeSmokableFormId = g_filledBonePipeSmokableFormId;
			g_activeSmokableCategory = g_filledBonePipeSmokableCategory;
			_MESSAGE("[EquipState] Lit Bone Pipe equipped - active smokable set from BONE PIPE cache: FormID=%08X, Category=%s",
				g_activeSmokableFormId, SmokableIngredients::GetCategoryName(g_activeSmokableCategory));

			// Initialize smoking mechanics for this lit item
			InitializeSmokingMechanics();

			OnLitPipeEquipped();
		}
		else
		{
			UnequipVisualArmor(g_bonePipeLitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_bonePipeLitVisualRightArmorFullFormId);
			UnequipVisualArmor(g_herbBonePipeUnlitVisualLeftArmorFullFormId);
			UnequipVisualArmor(g_herbBonePipeUnlitVisualRightArmorFullFormId);

			// Skip inventory changes and effect clearing if this is a hand swap unequip
			if (g_isHandSwapUnequip)
			{
				_MESSAGE("[EquipState] Hand swap unequip - skipping inventory removal and effect clearing for lit bone pipe");
				g_isHandSwapUnequip = false;
			}
			else
			{
				// Clear the bone pipe specific cache AND active smokable
				g_filledBonePipeSmokableFormId = 0;
				g_filledBonePipeSmokableCategory = SmokableCategory::None;
				g_activeSmokableFormId = 0;
				g_activeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[EquipState] Cleared BONE PIPE cache and active smokable (lit bone pipe unequipped)");

				// Reset smoking mechanics
				ResetSmokingMechanics();

				Actor* player = (*g_thePlayer);
				if (player)
				{
					TESObjectREFR* playerRef = static_cast<TESObjectREFR*>(player);
					if (g_bonePipeLitWeaponFullFormId != 0)
					{
						TESForm* litWeaponForm = LookupFormByID(g_bonePipeLitWeaponFullFormId);
						if (litWeaponForm)
							RemoveItemFromInventory(playerRef, litWeaponForm, 1, true);
					}
					if (g_herbBonePipeWeaponFullFormId != 0)
					{
						TESForm* unlitWeaponForm = LookupFormByID(g_herbBonePipeWeaponFullFormId);
						if (unlitWeaponForm)
							AddItem_Native(nullptr, 0, playerRef, unlitWeaponForm, 1, true);
					}
				}
			}

			if (vrikInterface)
			{
				vrikInterface->restoreFingers(true);
				vrikInterface->restoreFingers(false);
			}

			ClearCachedFingerPositions(true);
			ClearCachedFingerPositions(false);

			// Decrement smoke item count and stop tracking if none left
			g_equippedSmokeItemCount--;
			if (g_equippedSmokeItemCount < 0) g_equippedSmokeItemCount = 0;
			if (g_vrInputTracker)
			{
				g_vrInputTracker->SetLitItemEquippedHand(false, false);
				if (g_equippedSmokeItemCount == 0)
				{
					g_vrInputTracker->SetSmokeItemEquippedHand(false, false);
					g_vrInputTracker->StopTracking();
				}
			}
		}
	}

	// ============================================
	// Handle Dummy Weapon Equip/Unequip Events
	// ============================================
	void EquipStateManager::OnDummyWeaponEquipEvent(const TESEquipEvent& evn, bool inLeftHand, bool inRightHand)
	{
		Actor* player = (*g_thePlayer);
		if (!player)
			return;

		const bool isEquip = evn.equipped;
		
		const bool isRolledSmoke = IsRolledSmokeWeapon(evn.baseObject);
		const bool isHerbWoodenPipe = IsHerbWoodenPipeWeapon(evn.baseObject);
		const bool isHerbBonePipe = IsHerbBonePipeWeapon(evn.baseObject);
		const bool isEmptyWoodenPipe = IsEmptyWoodenPipeWeapon(evn.baseObject);
		const bool isEmptyBonePipe = IsEmptyBonePipeWeapon(evn.baseObject);
		const bool isRolledSmokeLit = IsRolledSmokeLitWeapon(evn.baseObject);
		const bool isWoodenPipeLit = IsWoodenPipeLitWeapon(evn.baseObject);
		const bool isBonePipeLit = IsBonePipeLitWeapon(evn.baseObject);

		const char* productType = "Unknown";
		if (isRolledSmoke) productType = "RolledSmoke";
		else if (isHerbWoodenPipe) productType = "HerbWoodenPipe";
		else if (isHerbBonePipe) productType = "HerbBonePipe";
		else if (isEmptyWoodenPipe) productType = "EmptyWoodenPipe";
		else if (isEmptyBonePipe) productType = "EmptyBonePipe";
		else if (isRolledSmokeLit) productType = "RolledSmokeLit";
		else if (isWoodenPipeLit) productType = "WoodenPipeLit";
		else if (isBonePipeLit) productType = "BonePipeLit";

		_MESSAGE("[EquipState] OnDummyWeaponEquipEvent: baseObject=%08X equip=%d product=%s hand=%s",
			evn.baseObject, isEquip ? 1 : 0, productType, HandStr(inLeftHand, inRightHand));

		if (isRolledSmoke)
			HandleRolledSmokeEquip(isEquip, inLeftHand, inRightHand);
		else if (isHerbWoodenPipe)
			HandleHerbWoodenPipeEquip(isEquip, inLeftHand, inRightHand);
		else if (isHerbBonePipe)
			HandleHerbBonePipeEquip(isEquip, inLeftHand, inRightHand);
		else if (isEmptyWoodenPipe)
			HandleEmptyWoodenPipeEquip(isEquip, inLeftHand, inRightHand);
		else if (isEmptyBonePipe)
			HandleEmptyBonePipeEquip(isEquip, inLeftHand, inRightHand);
		else if (isRolledSmokeLit)
			HandleRolledSmokeLitEquip(isEquip, inLeftHand, inRightHand);
		else if (isWoodenPipeLit)
			HandleWoodenPipeLitEquip(isEquip, inLeftHand, inRightHand);
		else if (isBonePipeLit)
			HandleBonePipeLitEquip(isEquip, inLeftHand, inRightHand);
	}

	// ============================================
	// Global Functions
	// ============================================

	void InitializeEquipStateManager()
	{
		if (g_equipStateManager == nullptr)
		{
			g_equipStateManager = new EquipStateManager();
			g_equipStateManager->Initialize();
						_MESSAGE("EquipStateManager: Global instance created");
		}
	}

	void ShutdownEquipStateManager()
	{
		if (g_equipStateManager != nullptr)
		{
			delete g_equipStateManager;
			g_equipStateManager = nullptr;
			_MESSAGE("EquipStateManager: Global instance destroyed");
		}
	}

	void UnequipAllSmokeItems()
	{
		_MESSAGE("[GameLoad] Checking for equipped smoke items to unequip...");

		// Reset the equipped smoke item count first
		g_equippedSmokeItemCount = 0;
		_MESSAGE("[GameLoad] Reset g_equippedSmokeItemCount to 0");

		Actor* player = (*g_thePlayer);
		if (!player)
		{
			_MESSAGE("[GameLoad] Player not available");
			return;
		}

		EquipManager* equipMan = EquipManager::GetSingleton();
		if (!equipMan)
		{
			_MESSAGE("[GameLoad] EquipManager not available");
			return;
		}

		// Check left hand
		TESForm* leftItem = player->GetEquippedObject(true);
		if (leftItem)
		{
			bool isSmokeItem = IsRolledSmokeWeapon(leftItem->formID) ||
				IsRolledSmokeLitWeapon(leftItem->formID) ||
				IsHerbWoodenPipeWeapon(leftItem->formID) ||
				IsHerbBonePipeWeapon(leftItem->formID) ||
				IsEmptyWoodenPipeWeapon(leftItem->formID) ||
				IsEmptyBonePipeWeapon(leftItem->formID) ||
				IsWoodenPipeLitWeapon(leftItem->formID) ||
				IsBonePipeLitWeapon(leftItem->formID);

			if (isSmokeItem)
			{
				_MESSAGE("[GameLoad] Found smoke item in LEFT hand: %08X - unequipping", leftItem->formID);
				BGSEquipSlot* leftSlot = GetLeftHandSlot();
				CALL_MEMBER_FN(equipMan, UnequipItem)(player, leftItem, nullptr, 1, leftSlot, false, false, false, false, nullptr);
			}
		}

		// Check right hand
		TESForm* rightItem = player->GetEquippedObject(false);
		if (rightItem)
		{
			bool isSmokeItem = IsRolledSmokeWeapon(rightItem->formID) ||
				IsRolledSmokeLitWeapon(rightItem->formID) ||
				IsHerbWoodenPipeWeapon(rightItem->formID) ||
				IsHerbBonePipeWeapon(rightItem->formID) ||
				IsEmptyWoodenPipeWeapon(rightItem->formID) ||
				IsEmptyBonePipeWeapon(rightItem->formID) ||
				IsWoodenPipeLitWeapon(rightItem->formID) ||
				IsBonePipeLitWeapon(rightItem->formID);

			if (isSmokeItem)
			{
				_MESSAGE("[GameLoad] Found smoke item in RIGHT hand: %08X - unequipping", rightItem->formID);
				BGSEquipSlot* rightSlot = GetRightHandSlot();
				CALL_MEMBER_FN(equipMan, UnequipItem)(player, rightItem, nullptr, 1, rightSlot, false, false, false, false, nullptr);
			}
		}

		_MESSAGE("[GameLoad] Smoke item unequip check complete");
	}

	void ResetEquippedSmokeItemCount()
	{
		g_equippedSmokeItemCount = 0;
		_MESSAGE("[Reset] Reset g_equippedSmokeItemCount to 0");
	}

}
