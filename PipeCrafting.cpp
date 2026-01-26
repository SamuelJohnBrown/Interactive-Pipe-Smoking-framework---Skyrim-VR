#include "PipeCrafting.h"
#include "Engine.h"
#include "EquipState.h"
#include "SmokableIngredients.h"
#include "Haptics.h"
#include "VRInputTracker.h"
#include "config.h"
#include "higgsinterface001.h"
#include "skse64/GameObjects.h"
#include "skse64/GameRTTI.h"
#include "skse64/NiNodes.h"
#include <chrono>

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Knife/Dagger Equipped State - REMOVED, now using HIGGS grabbed detection
	// ============================================
	// bool g_knifeEquippedLeft = false;  // No longer used
	// bool g_knifeEquippedRight = false; // No longer used

	// ============================================
	// Held Crafting Item State
	// ============================================
	TESObjectREFR* g_heldCraftingItemLeft = nullptr;
	TESObjectREFR* g_heldCraftingItemRight = nullptr;

	// ============================================
	// Held Knife State (grabbed via HIGGS, not equipped)
	// ============================================
	TESObjectREFR* g_heldKnifeLeft = nullptr;
	TESObjectREFR* g_heldKnifeRight = nullptr;

	// ============================================
	// Material type of held crafting items
	// ============================================
	CraftingMaterialType g_heldCraftingMaterialLeft = CraftingMaterialType::None;
	CraftingMaterialType g_heldCraftingMaterialRight = CraftingMaterialType::None;

	// ============================================
	// Smoke Rolling State - Roll of Paper
	// ============================================
	TESObjectREFR* g_heldRollOfPaperLeft = nullptr;
	TESObjectREFR* g_heldRollOfPaperRight = nullptr;

	// Track if smoke rolling condition was already logged (to avoid spam)
	bool g_smokeRollingConditionLogged = false;

	// ============================================
	// Crafting Hit Counter
	// ============================================
	int g_craftingHitCount = 0;

	// ============================================
	// Crafting hit cooldown (1 second between hits)
	// ============================================
	static std::chrono::steady_clock::time_point s_lastHitTime;
	static bool s_hasHitBefore = false;
	constexpr int HIT_COOLDOWN_MS = 1000; // 1 second cooldown

	// ============================================
	// Crafting item scale (65% of original = shrink by 35% when grabbed)
	// ============================================
	constexpr float CRAFTING_ITEM_SCALE = 0.65f;

	// Shrink amount per hit (20% smaller each hit)
	constexpr float CRAFTING_HIT_SHRINK_FACTOR = 0.80f;

	// Track current scale of crafting items
	static float s_craftingItemScaleLeft = 1.0f;
	static float s_craftingItemScaleRight = 1.0f;

	// ============================================
	// Helper to check if a reference is still valid
	// ============================================
	static bool IsRefrValid(TESObjectREFR* refr)
	{
		if (!refr)
			return false;
		
		if (refr->flags & TESForm::kFlagIsDeleted)
			return false;
		
		if (refr->formID == 0)
			return false;

		return true;
	}

	// ============================================
	// Shrink crafting item (similar to smokable shrinking)
	// ============================================
	static void ShrinkCraftingItem(TESObjectREFR* refr, float targetScale)
	{
		if (!IsRefrValid(refr))
			return;

		NiNode* rootNode = refr->GetNiNode();
		if (!rootNode)
			return;

		// Set the scale on the root node's local transform
		rootNode->m_localTransform.scale = targetScale;
		rootNode->m_worldTransform.scale = targetScale;

		// Recursively set scale on all child nodes
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

	// ============================================
	// Helper to get form name
	// ============================================
	static const char* GetFormName(TESForm* form)
	{
		if (!form)
			return "NULL";

		// Try to get the name based on form type
		// Most items inherit from TESBoundObject which has fullName
		
		// Try as TESObjectREFR first (has baseForm)
		TESObjectREFR* refr = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
		if (refr && refr->baseForm)
		{
			form = refr->baseForm;
		}

		// Try common item types that have fullName
		TESObjectWEAP* weapon = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
		if (weapon)
		{
			const char* name = weapon->fullName.GetName();
			if (name && name[0] != '\0')
				return name;
		}

		TESObjectARMO* armor = DYNAMIC_CAST(form, TESForm, TESObjectARMO);
		if (armor)
		{
			const char* name = armor->fullName.GetName();
			if (name && name[0] != '\0')
				return name;
		}

		TESObjectMISC* misc = DYNAMIC_CAST(form, TESForm, TESObjectMISC);
		if (misc)
		{
			const char* name = misc->fullName.GetName();
			if (name && name[0] != '\0')
				return name;
		}

		TESObjectBOOK* book = DYNAMIC_CAST(form, TESForm, TESObjectBOOK);
		if (book)
		{
			const char* name = book->fullName.GetName();
			if (name && name[0] != '\0')
				return name;
		}

		IngredientItem* ingredient = DYNAMIC_CAST(form, TESForm, IngredientItem);
		if (ingredient)
		{
			const char* name = ingredient->fullName.GetName();
			if (name && name[0] != '\0')
				return name;
		}

		AlchemyItem* potion = DYNAMIC_CAST(form, TESForm, AlchemyItem);
		if (potion)
		{
			const char* name = potion->fullName.GetName();
			if (name && name[0] != '\0')
				return name;
		}

		// Fallback to GetFullName() which works for some forms
		const char* fallbackName = form->GetFullName();
		if (fallbackName && fallbackName[0] != '\0')
			return fallbackName;

		return "Unknown";
	}

	// ============================================
	// Check if weapon is a knife or dagger
	// ============================================
	bool IsKnifeOrDagger(TESForm* form)
	{
		if (!form)
			return false;

		TESObjectWEAP* weapon = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
		if (!weapon)
			return false;

		// Check weapon type - Dagger is type 2
		// WeaponType: 0=HandToHand, 1=OneHandSword, 2=OneHandDagger, 3=OneHandAxe, etc.
		UInt8 weaponType = weapon->gameData.type;
		
		// Type 2 is OneHandDagger
		if (weaponType == 2)
			return true;

		// Also check by keyword or name for modded daggers/knives
		const char* name = weapon->fullName.GetName();
		if (name)
		{
			// Case-insensitive check for common knife/dagger names
			if (strstr(name, "Dagger") || strstr(name, "dagger") ||
				strstr(name, "Knife") || strstr(name, "knife") ||
				strstr(name, "Shiv") || strstr(name, "shiv"))
			{
				return true;
			}
		}

		return false;
	}

	// ============================================
	// Check if item name indicates bone material
	// ============================================
	bool IsBoneMaterial(const char* itemName)
	{
		if (!itemName)
			return false;

		// Check for "Bone" or "bone" in the name
		if (strstr(itemName, "Bone") || strstr(itemName, "bone") ||
			strstr(itemName, "BONE"))
		{
			return true;
		}

		return false;
	}

	// ============================================
	// Check if a form is a Roll of Paper
	// ============================================
	bool IsRollOfPaper(UInt32 formId)
	{
		return (formId == ROLL_OF_PAPER_BASE_FORMID);
	}

	// ============================================
	// Check if a form is a grabbable Empty Wooden Pipe MISC item
	// ============================================
	bool IsGrabbableEmptyWoodenPipe(UInt32 formId)
	{
		// Check base form ID
		if (formId == EMPTY_WOODEN_PIPE_MISC_BASE_FORMID)
			return true;

		// Also check full form ID from ESP
		UInt32 fullFormId = GetFullFormIdFromEspAndFormId(ESP_NAME, EMPTY_WOODEN_PIPE_MISC_BASE_FORMID);
		if (fullFormId != 0 && formId == fullFormId)
			return true;

		return false;
	}

	// ============================================
	// Check if a form is a grabbable Empty Bone Pipe MISC item
	// ============================================
	bool IsGrabbableEmptyBonePipe(UInt32 formId)
	{
		// Check base form ID
		if (formId == EMPTY_BONE_PIPE_MISC_BASE_FORMID)
			return true;

		// Also check full form ID from ESP
		UInt32 fullFormId = GetFullFormIdFromEspAndFormId(ESP_NAME, EMPTY_BONE_PIPE_MISC_BASE_FORMID);
		if (fullFormId != 0 && formId == fullFormId)
			return true;

		return false;
	}

	// ============================================
	// Check for already-grabbed smokable ingredients
	// Called when Roll of Paper is grabbed or empty pipe is equipped
	// ============================================
	void CheckForAlreadyGrabbedSmokable()
	{
		if (!higgsInterface)
			return;

		// Check left hand
		TESObjectREFR* leftGrabbed = higgsInterface->GetGrabbedObject(true);
		if (leftGrabbed && leftGrabbed->baseForm && g_heldSmokableLeft == nullptr)
		{
			UInt32 formId = leftGrabbed->baseForm->formID;
			if (SmokableIngredients::IsSmokable(formId))
			{
				g_heldSmokableLeft = leftGrabbed;
				const char* smokableName = SmokableIngredients::GetSmokableName(formId);
				_MESSAGE("[CheckGrabbed] Found already-grabbed smokable in LEFT hand: '%s' (FormID: %08X)", smokableName, formId);
				
				// Apply shrink and HIGGS mouth radius
				float targetScale = configSmokableGrabbedScale;
				if (SmokableIngredients::IsCannabis(formId))
				{
					targetScale = 1.0f;
				}
				ShrinkSmokableIngredient(leftGrabbed, targetScale);
				SetHiggsMouthRadius(static_cast<double>(configHiggsMouthRadiusSmokable));
			}
		}

		// Check right hand
		TESObjectREFR* rightGrabbed = higgsInterface->GetGrabbedObject(false);
		if (rightGrabbed && rightGrabbed->baseForm && g_heldSmokableRight == nullptr)
		{
			UInt32 formId = rightGrabbed->baseForm->formID;
			if (SmokableIngredients::IsSmokable(formId))
			{
				g_heldSmokableRight = rightGrabbed;
				const char* smokableName = SmokableIngredients::GetSmokableName(formId);
				_MESSAGE("[CheckGrabbed] Found already-grabbed smokable in RIGHT hand: '%s' (FormID: %08X)", smokableName, formId);
				
				// Apply shrink and HIGGS mouth radius
				float targetScale = configSmokableGrabbedScale;
				if (SmokableIngredients::IsCannabis(formId))
				{
					targetScale = 1.0f;
				}
				ShrinkSmokableIngredient(rightGrabbed, targetScale);
				SetHiggsMouthRadius(static_cast<double>(configHiggsMouthRadiusSmokable));
			}
		}
	}

	// ============================================
	// Reset Crafting State
	// ============================================
	void ResetCraftingState()
	{
		g_craftingHitCount = 0;
		s_hasHitBefore = false;
		s_craftingItemScaleLeft = CRAFTING_ITEM_SCALE;
		s_craftingItemScaleRight = CRAFTING_ITEM_SCALE;
		_MESSAGE("[PipeCrafting] Crafting state reset - hit count: 0, scale reset to %.0f%%", CRAFTING_ITEM_SCALE * 100.0f);
	}

	// ============================================
	// Weapon Equip State Changed - No longer needed for knife tracking
	// ============================================
	void OnWeaponEquipStateChanged(TESForm* weapon, bool isEquip, bool isLeftHand)
	{
		// No longer tracking knife equip state - knives are now grabbed via HIGGS
		// Keep this function as a stub in case it's called from elsewhere
	}

	// ============================================
	// Check if a grabbed object is a knife/dagger
	// ============================================
	static bool IsGrabbedKnife(TESObjectREFR* refr)
	{
		if (!refr || !refr->baseForm)
			return false;
		
		return IsKnifeOrDagger(refr->baseForm);
	}

	// ============================================
	// Check if player has a non-smoke weapon, spell, or torch equipped in either hand
	// Returns true if player has anything equipped that isn't a smoke item
	// ============================================
	static bool HasWeaponOrSpellEquipped()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return false;

		// Check left hand
		TESForm* leftEquipped = player->GetEquippedObject(true);
		if (leftEquipped)
		{
			// Check if it's one of our dummy smoke weapons - if so, allow
			if (IsRolledSmokeWeapon(leftEquipped->formID) ||
				IsHerbWoodenPipeWeapon(leftEquipped->formID) ||
				IsHerbBonePipeWeapon(leftEquipped->formID) ||
				IsEmptyWoodenPipeWeapon(leftEquipped->formID) ||
				IsEmptyBonePipeWeapon(leftEquipped->formID) ||
				IsRolledSmokeLitWeapon(leftEquipped->formID) ||
				IsWoodenPipeLitWeapon(leftEquipped->formID) ||
				IsBonePipeLitWeapon(leftEquipped->formID))
			{
				// It's a smoke item, don't block
			}
			else
			{
				// It's a weapon/torch/shield/spell - block
				return true;
			}
		}

		// Check right hand
		TESForm* rightEquipped = player->GetEquippedObject(false);
		if (rightEquipped)
		{
			// Check if it's one of our dummy smoke weapons - if so, allow
			if (IsRolledSmokeWeapon(rightEquipped->formID) ||
				IsHerbWoodenPipeWeapon(rightEquipped->formID) ||
				IsHerbBonePipeWeapon(rightEquipped->formID) ||
				IsEmptyWoodenPipeWeapon(rightEquipped->formID) ||
				IsEmptyBonePipeWeapon(rightEquipped->formID) ||
				IsRolledSmokeLitWeapon(rightEquipped->formID) ||
				IsWoodenPipeLitWeapon(rightEquipped->formID) ||
				IsBonePipeLitWeapon(rightEquipped->formID))
			{
				// It's a smoke item, don't block
			}
			else
			{
				// It's a weapon/torch/shield/spell - block
				return true;
			}
		}

		return false;
	}

	// ============================================
	// HIGGS Collision Callback
	// ============================================
	void OnCraftingCollision(bool isLeft, float mass, float separatingVelocity)
	{
		// isLeft refers to VR controller hand (from HIGGS)
		// Check if knife is GRABBED (via HIGGS) in the colliding hand
		bool knifeInCollidingHand = isLeft ? (g_heldKnifeLeft != nullptr) : (g_heldKnifeRight != nullptr);
		if (!knifeInCollidingHand)
			return;

		// Check if we're holding a crafting item in the OTHER VR controller hand
		bool holdingItemInOtherHand = isLeft ? (g_heldCraftingItemRight != nullptr) : (g_heldCraftingItemLeft != nullptr);
		if (!holdingItemInOtherHand)
			return;

		// Get the held item and material type (from the OTHER VR controller)
		TESObjectREFR* heldItem = isLeft ? g_heldCraftingItemRight : g_heldCraftingItemLeft;
		CraftingMaterialType materialType = isLeft ? g_heldCraftingMaterialRight : g_heldCraftingMaterialLeft;
		
		// itemInLeftVRController tracks which VR controller has the crafting material
		bool itemInLeftVRController = !isLeft;
		
		const char* itemName = "Unknown";
		if (heldItem && heldItem->baseForm)
		{
			itemName = GetFormName(heldItem->baseForm);
		}

		// Only count significant hits (velocity threshold)
		const float MIN_HIT_VELOCITY = 0.5f;
		if (separatingVelocity < MIN_HIT_VELOCITY)
			return;

		// Check hit cooldown (1 second between hits)
		auto now = std::chrono::steady_clock::now();
		if (s_hasHitBefore)
		{
			auto timeSinceLastHit = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastHitTime).count();
			if (timeSinceLastHit < HIT_COOLDOWN_MS)
			{
				// Still in cooldown, ignore this hit
				return;
			}
		}

		// Record this hit time
		s_lastHitTime = now;
		s_hasHitBefore = true;

		// Increment hit counter
		g_craftingHitCount++;

		const char* knifeHandVR = isLeft ? "LEFT" : "RIGHT";
		const char* itemHandVR = isLeft ? "RIGHT" : "LEFT";
		const char* pipeType = (materialType == CraftingMaterialType::Bone) ? "BONE PIPE" : "WOODEN PIPE";

		// Shrink the item to 80% of original size on each hit
		float* currentScale = itemInLeftVRController ? &s_craftingItemScaleLeft : &s_craftingItemScaleRight;
		*currentScale *= CRAFTING_HIT_SHRINK_FACTOR;
		ShrinkCraftingItem(heldItem, *currentScale);

		_MESSAGE("[PipeCrafting] *** KNIFE HIT DETECTED! (%s CRAFTING) ***", pipeType);
		_MESSAGE("[PipeCrafting]   -> Knife grabbed in %s VR controller hit item in %s VR controller", knifeHandVR, itemHandVR);
		_MESSAGE("[PipeCrafting]   -> Item: %s", itemName);
		_MESSAGE("[PipeCrafting]   -> Material: %s", materialType == CraftingMaterialType::Bone ? "BONE" : "WOOD");
		_MESSAGE("[PipeCrafting]   -> Velocity: %.2f, Mass: %.2f", separatingVelocity, mass);
		_MESSAGE("[PipeCrafting]   -> Hit count: %d / %d", g_craftingHitCount, CRAFTING_HITS_REQUIRED);
		_MESSAGE("[PipeCrafting]   -> New scale: %.0f%% (shrunk by 20%%)", *currentScale * 100.0f);

		// Check if we've reached the required hits
		if (g_craftingHitCount >= CRAFTING_HITS_REQUIRED)
		{
			_MESSAGE("[PipeCrafting] ============================================");
			_MESSAGE("[PipeCrafting] *** PERFECT CRAFTING ACHIEVED! ***");
			_MESSAGE("[PipeCrafting] *** %s CRAFTED FROM: %s ***", pipeType, itemName);
			_MESSAGE("[PipeCrafting] ============================================");

			// Scale the crafting item to 0 (hide it)
			if (heldItem)
			{
				ShrinkCraftingItem(heldItem, 0.0f);
				_MESSAGE("[PipeCrafting]   -> Scaled %s to 0 (hidden)", itemName);
			}

			// Clear the held crafting item reference (using VR controller hand)
			if (itemInLeftVRController)
			{
				g_heldCraftingItemLeft = nullptr;
				g_heldCraftingMaterialLeft = CraftingMaterialType::None;
			}
			else
			{
				g_heldCraftingItemRight = nullptr;
				g_heldCraftingMaterialRight = CraftingMaterialType::None;
			}

			// Equip the appropriate pipe to the hand that was holding the crafting material
			// VR controller hand is used directly since HIGGS uses VR controller hands
			if (g_equipStateManager)
			{
				// Convert VR controller hand to game hand for equipping
				bool equipToGameLeftHand;
				if (IsLeftHandedMode())
				{
					// In left-handed mode: left VR controller = right game hand
					equipToGameLeftHand = !itemInLeftVRController;
					_MESSAGE("[PipeCrafting]   -> Left-handed mode: VR %s -> Game %s",
						itemInLeftVRController ? "LEFT" : "RIGHT",
						equipToGameLeftHand ? "LEFT" : "RIGHT");
				}
				else
				{
					equipToGameLeftHand = itemInLeftVRController;
				}

				if (materialType == CraftingMaterialType::Bone)
				{
					g_equipStateManager->EquipEmptyBonePipe(equipToGameLeftHand);
					_MESSAGE("[PipeCrafting]   -> Equipped Empty Bone Pipe to game %s hand", equipToGameLeftHand ? "LEFT" : "RIGHT");
				}
				else
				{
					g_equipStateManager->EquipEmptyWoodenPipe(equipToGameLeftHand);
					_MESSAGE("[PipeCrafting]   -> Equipped Empty Wooden Pipe to game %s hand", equipToGameLeftHand ? "LEFT" : "RIGHT");
				}
			}

			// Reset crafting state
			ResetCraftingState();
		}
	}

	// ============================================
	// HIGGS Grab Callback
	// ============================================
 void OnItemGrabbed(bool isLeft, TESObjectREFR* grabbedRefr)
	{
		if (!grabbedRefr)
			return;

		TESForm* baseForm = grabbedRefr->baseForm;
		if (!baseForm)
			return;

		// Skip all our logic if player has a weapon, spell, or torch equipped (excluding smoke items)
		if (HasWeaponOrSpellEquipped())
			return;

		const char* handStr = isLeft ? "LEFT" : "RIGHT";
		const char* formName = GetFormName(baseForm);

		// ============================================
		// 1. Check for Knife/Dagger grab (for crafting)
		// ============================================
		if (IsKnifeOrDagger(baseForm))
		{
			_MESSAGE("[PipeCrafting] *** KNIFE GRABBED! ***");
			_MESSAGE("[PipeCrafting]   -> %s grabbed in %s VR controller", formName, handStr);

			if (isLeft)
				g_heldKnifeLeft = grabbedRefr;
			else
				g_heldKnifeRight = grabbedRefr;

			_MESSAGE("[PipeCrafting]   -> Knife tracked for crafting (no shrink applied)");
			return;
		}

		// ============================================
		// 2. Check for Grabbable Empty Wooden Pipe (MISC item from world)
		// ============================================
		if (IsGrabbableEmptyWoodenPipe(baseForm->formID))
		{
			_MESSAGE("[PipeGrab] *** EMPTY WOODEN PIPE MISC ITEM GRABBED in %s VR controller ***", handStr);

			ShrinkCraftingItem(grabbedRefr, 0.0f);

			bool equipToGameLeftHand = IsLeftHandedMode() ? !isLeft : isLeft;
			
			if (g_equipStateManager)
				g_equipStateManager->EquipEmptyWoodenPipe(equipToGameLeftHand);

			return;
		}

		// ============================================
		// 3. Check for Grabbable Empty Bone Pipe (MISC item from world)
		// ============================================
		if (IsGrabbableEmptyBonePipe(baseForm->formID))
		{
			_MESSAGE("[PipeGrab] *** EMPTY BONE PIPE MISC ITEM GRABBED in %s VR controller ***", handStr);

			ShrinkCraftingItem(grabbedRefr, 0.0f);

			bool equipToGameLeftHand = IsLeftHandedMode() ? !isLeft : isLeft;
			
			if (g_equipStateManager)
				g_equipStateManager->EquipEmptyBonePipe(equipToGameLeftHand);

			return;
		}

		// ============================================
		// 4. Check for Roll of Paper (Smoke Rolling)
		// ============================================
		if (IsRollOfPaper(baseForm->formID))
		{
			_MESSAGE("[SmokeRolling] *** ROLL OF PAPER GRABBED in %s VR controller ***", handStr);

			if (isLeft)
			{
				g_heldRollOfPaperLeft = grabbedRefr;
				g_rollOfPaperHeldLeft = true;
			}
			else
			{
				g_heldRollOfPaperRight = grabbedRefr;
				g_rollOfPaperHeldRight = true;
			}

			ShrinkCraftingItem(grabbedRefr, ROLL_OF_PAPER_SCALE);

			if (g_vrInputTracker)
				g_vrInputTracker->StartTracking();

			CheckForAlreadyGrabbedSmokable();
			return;
		}

		// ============================================
		// 5. Check for Crafting Material (only if knife grabbed in OTHER hand)
		// ============================================
		bool knifeInOtherHand = isLeft ? (g_heldKnifeRight != nullptr) : (g_heldKnifeLeft != nullptr);
		
		if (knifeInOtherHand && IsValidCraftingMaterial(baseForm))
		{
			CraftingMaterialType materialType = IsBoneMaterial(formName) ? CraftingMaterialType::Bone : CraftingMaterialType::Wood;

			_MESSAGE("[PipeCrafting] *** CRAFTING MATERIAL GRABBED in %s VR controller (knife in other hand) ***", handStr);
			_MESSAGE("[PipeCrafting]   -> Material: %s, Type: %s", formName, materialType == CraftingMaterialType::Bone ? "BONE" : "WOOD");

			if (isLeft)
			{
				g_heldCraftingItemLeft = grabbedRefr;
				g_heldCraftingMaterialLeft = materialType;
				s_craftingItemScaleLeft = CRAFTING_ITEM_SCALE;
			}
			else
			{
				g_heldCraftingItemRight = grabbedRefr;
				g_heldCraftingMaterialRight = materialType;
				s_craftingItemScaleRight = CRAFTING_ITEM_SCALE;
			}

			ShrinkCraftingItem(grabbedRefr, CRAFTING_ITEM_SCALE);
			ResetCraftingState();
			return;
		}

		// ============================================
		// Not a crafting-related item - do nothing
		// ============================================
	}

	// ============================================
	// HIGGS Drop Callback
	// ============================================
	void OnItemDropped(bool isLeft, TESObjectREFR* droppedRefr)
	{
		// ============================================
		// Check for Knife drop
		// ============================================
		if (isLeft && g_heldKnifeLeft != nullptr)
		{
			_MESSAGE("[PipeCrafting] LEFT VR controller dropped knife");
			g_heldKnifeLeft = nullptr;
		}
		else if (!isLeft && g_heldKnifeRight != nullptr)
		{
			_MESSAGE("[PipeCrafting] RIGHT VR controller dropped knife");
			g_heldKnifeRight = nullptr;
		}

		// ============================================
		// Check for Smokable Ingredient drop while Roll of Paper held (Smoke Rolling)
		// ============================================
		bool hasRollOfPaperLeft = (g_heldRollOfPaperLeft != nullptr);
		bool hasRollOfPaperRight = (g_heldRollOfPaperRight != nullptr);
		bool hasRollOfPaper = hasRollOfPaperLeft || hasRollOfPaperRight;

		bool droppedSmokable = false;
		TESObjectREFR* droppedSmokableRefr = nullptr;
		UInt32 smokableFormId = 0;
		SmokableCategory smokableCategory = SmokableCategory::None;
		const char* smokableName = "Unknown";

		// Check if we dropped a smokable ingredient
		if (isLeft && g_heldSmokableLeft != nullptr)
		{
			droppedSmokable = true;
			droppedSmokableRefr = g_heldSmokableLeft;
		}
		else if (!isLeft && g_heldSmokableRight != nullptr)
		{
			droppedSmokable = true;
			droppedSmokableRefr = g_heldSmokableRight;
		}

		// Check smoke rolling completion: dropping smokable while holding Roll of Paper + controllers touching
		if (droppedSmokable && droppedSmokableRefr && hasRollOfPaper)
		{
			bool controllersTouching = g_vrInputTracker && g_vrInputTracker->AreControllersTouching();

			_MESSAGE("[SmokeRolling Drop DEBUG] hasRollOfPaperLeft=%d hasRollOfPaperRight=%d controllersTouching=%d",
				hasRollOfPaperLeft ? 1 : 0, hasRollOfPaperRight ? 1 : 0, controllersTouching ? 1 : 0);

			if (controllersTouching)
			{
				// Get smokable info before clearing
				if (droppedSmokableRefr->baseForm)
				{
					smokableFormId = droppedSmokableRefr->baseForm->formID;
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
				_MESSAGE("[SmokeRolling]   -> Stored in ROLLED SMOKE cache: '%s' (%s)", smokableName, SmokableIngredients::GetCategoryName(smokableCategory));

				// Scale the dropped smokable ingredient to 0 (makes it disappear visually)
				ShrinkSmokableIngredient(droppedSmokableRefr, 0.0f);
				_MESSAGE("[SmokeRolling]   -> Scaled smokable ingredient to 0 (hidden)");

				// Scale the Roll of Paper to 0 (makes it disappear visually)
				TESObjectREFR* rollOfPaper = hasRollOfPaperLeft ? g_heldRollOfPaperLeft : g_heldRollOfPaperRight;
				if (rollOfPaper)
				{
					ShrinkCraftingItem(rollOfPaper, 0.0f);
					_MESSAGE("[SmokeRolling]   -> Scaled Roll of Paper to 0 (hidden)");
				}

				// Trigger haptic feedback on the hand with the Roll of Paper to confirm
				TriggerHapticFeedback(hasRollOfPaperLeft, hasRollOfPaperRight, 0.5f, 0.3f);
				_MESSAGE("[SmokeRolling]   -> Haptic feedback triggered on %s hand!", hasRollOfPaperLeft ? "LEFT" : "RIGHT");

				// Clear smokable tracking
				if (isLeft)
					g_heldSmokableLeft = nullptr;
				else
					g_heldSmokableRight = nullptr;

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
						// In left-handed mode: left VR controller = right game hand
						equipToGameLeftHand = !hasRollOfPaperLeft;
						_MESSAGE("[SmokeRolling]   -> Left-handed mode: VR %s -> Game %s", 
							hasRollOfPaperLeft ? "LEFT" : "RIGHT",
							equipToGameLeftHand ? "LEFT" : "RIGHT");
					}
					else
					{
						equipToGameLeftHand = hasRollOfPaperLeft;
					}
					
					g_equipStateManager->EquipUnlitRolledSmoke(equipToGameLeftHand);
					_MESSAGE("[SmokeRolling] -> Equipping Unlit Rolled Smoke to game %s hand", equipToGameLeftHand ? "LEFT" : "RIGHT");
				}

				// Stop VR input tracking since we no longer have a Roll of Paper
				bool anyEmptyPipeEquipped = g_emptyPipeEquippedLeft || g_emptyPipeEquippedRight ||
					g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight ||
					g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight;

				if (!anyEmptyPipeEquipped && g_vrInputTracker)
				{
					g_vrInputTracker->StopTracking();
					_MESSAGE("[SmokeRolling]   -> Stopped VR input tracking");
				}

				// Reset the smoke rolling condition logged flag
				g_smokeRollingConditionLogged = false;

				// Restore HIGGS MouthRadius since we're done with the smokable
				RestoreHiggsMouthRadius();

				// Early return - we handled this drop as smoke rolling
				return;
			}
		}

		// ============================================
		// Check for Roll of Paper drop (normal drop, not smoke rolling)
		// ============================================
		bool wasRollOfPaper = false;
		TESObjectREFR* rollOfPaper = nullptr;

		if (isLeft && g_heldRollOfPaperLeft != nullptr)
		{
			rollOfPaper = g_heldRollOfPaperLeft;
			wasRollOfPaper = true;
			g_heldRollOfPaperLeft = nullptr;
			g_rollOfPaperHeldLeft = false;
			_MESSAGE("[SmokeRolling] LEFT hand dropped Roll of Paper");
		}
		else if (!isLeft && g_heldRollOfPaperRight != nullptr)
		{
			rollOfPaper = g_heldRollOfPaperRight;
			wasRollOfPaper = true;
			g_heldRollOfPaperRight = nullptr;
			g_rollOfPaperHeldRight = false;
			_MESSAGE("[SmokeRolling] RIGHT hand dropped Roll of Paper");
		}

		// Restore Roll of Paper scale
		if (wasRollOfPaper && rollOfPaper && IsRefrValid(rollOfPaper))
		{
			ShrinkCraftingItem(rollOfPaper, 1.0f);
			_MESSAGE("[SmokeRolling]   -> Restored Roll of Paper to 100%% scale");
		}

		// Stop VR input tracking if no Roll of Paper is held anymore
		if (wasRollOfPaper && g_heldRollOfPaperLeft == nullptr && g_heldRollOfPaperRight == nullptr)
		{
			// Only stop tracking if no empty pipes are equipped either
			bool anyEmptyPipeEquipped = g_emptyPipeEquippedLeft || g_emptyPipeEquippedRight ||
				g_emptyBonePipeEquippedLeft || g_emptyBonePipeEquippedRight ||
				g_emptyWoodenPipeEquippedLeft || g_emptyWoodenPipeEquippedRight;

			if (!anyEmptyPipeEquipped && g_vrInputTracker)
			{
				g_vrInputTracker->StopTracking();
				_MESSAGE("[SmokeRolling]   -> Stopped VR input tracking");
			}
		}

		// ============================================
		// Check for Crafting Item drop
		// ============================================
		bool wasCraftingItem = false;
		TESObjectREFR* craftingItem = nullptr;

		if (isLeft && g_heldCraftingItemLeft != nullptr)
		{
			craftingItem = g_heldCraftingItemLeft;
			wasCraftingItem = true;
			g_heldCraftingItemLeft = nullptr;
			g_heldCraftingMaterialLeft = CraftingMaterialType::None;
			_MESSAGE("[PipeCrafting] LEFT hand dropped crafting item");
		}
		else if (!isLeft && g_heldCraftingItemRight != nullptr)
		{
			craftingItem = g_heldCraftingItemRight;
			wasCraftingItem = true;
			g_heldCraftingItemRight = nullptr;
			g_heldCraftingMaterialRight = CraftingMaterialType::None;
			_MESSAGE("[PipeCrafting] RIGHT hand dropped crafting item");
		}

		// Restore the item's original scale if it was a crafting item
		if (wasCraftingItem && craftingItem && IsRefrValid(craftingItem))
		{
			ShrinkCraftingItem(craftingItem, 1.0f);
			_MESSAGE("[PipeCrafting]   -> Restored item to 100%% scale");
		}

		// Reset crafting state if no items are held
		if (g_heldCraftingItemLeft == nullptr && g_heldCraftingItemRight == nullptr)
		{
			if (g_craftingHitCount > 0)
			{
				_MESSAGE("[PipeCrafting] No crafting items held - resetting hit count");
				ResetCraftingState();
			}
		}
	}

	// ============================================
	// Update held crafting item scale (called every frame via HIGGS callback)
	// ============================================
	void UpdateHeldCraftingItemScale()
	{
		// Update pipe crafting items
		if (g_heldCraftingItemLeft && IsRefrValid(g_heldCraftingItemLeft))
		{
			ShrinkCraftingItem(g_heldCraftingItemLeft, s_craftingItemScaleLeft);
		}
		if (g_heldCraftingItemRight && IsRefrValid(g_heldCraftingItemRight))
		{
			ShrinkCraftingItem(g_heldCraftingItemRight, s_craftingItemScaleRight);
		}

		// Update roll of paper items (smoke rolling)
		if (g_heldRollOfPaperLeft && IsRefrValid(g_heldRollOfPaperLeft))
		{
			ShrinkCraftingItem(g_heldRollOfPaperLeft, ROLL_OF_PAPER_SCALE);
		}
		if (g_heldRollOfPaperRight && IsRefrValid(g_heldRollOfPaperRight))
		{
			ShrinkCraftingItem(g_heldRollOfPaperRight, ROLL_OF_PAPER_SCALE);
		}

		// Check smoke rolling condition
		CheckSmokeRollingCondition();
	}

	// ============================================
	// Check Smoke Rolling Condition
	// Player needs: Roll of Paper in one hand + Smokable ingredient in other hand + hands touching
	// ============================================
	void CheckSmokeRollingCondition()
	{
		// Check if we have a roll of paper in either hand
		bool hasRollOfPaperLeft = (g_heldRollOfPaperLeft != nullptr);
		bool hasRollOfPaperRight = (g_heldRollOfPaperRight != nullptr);
		bool hasRollOfPaper = hasRollOfPaperLeft || hasRollOfPaperRight;

		if (!hasRollOfPaper)
		{
			g_smokeRollingConditionLogged = false;
			return;
		}

		// Check if we have a smokable ingredient in the OTHER hand
		bool hasSmokableLeft = (g_heldSmokableLeft != nullptr);
		bool hasSmokableRight = (g_heldSmokableRight != nullptr);

		// Need roll of paper in one hand and smokable in the other
		bool validCombination = (hasRollOfPaperLeft && hasSmokableRight) || (hasRollOfPaperRight && hasSmokableLeft);

		if (!validCombination)
		{
			g_smokeRollingConditionLogged = false;
			return;
		}

		// Check if controllers have been touching long enough (using global flag set by VRInputTracker)
		if (!g_controllersTouchingLongEnough)
		{
			g_smokeRollingConditionLogged = false;
			return;
		}

		// All conditions met! Trigger weak haptic pulse on both hands
		TriggerHapticPulse(true, true, 0.08f);  // Weak continuous feedback on both hands

		if (!g_smokeRollingConditionLogged)
		{
			g_smokeRollingConditionLogged = true;

			// Get smokable info for logging
			TESObjectREFR* heldSmokable = hasSmokableLeft ? g_heldSmokableLeft : g_heldSmokableRight;
			const char* smokableName = "Unknown";
			UInt32 smokableFormId = 0;
			SmokableCategory smokableCategory = SmokableCategory::None;

			if (heldSmokable && heldSmokable->baseForm)
			{
				smokableFormId = heldSmokable->baseForm->formID;
				smokableName = SmokableIngredients::GetSmokableName(smokableFormId);
				smokableCategory = SmokableIngredients::GetCategory(smokableFormId);
			}

			_MESSAGE("[SmokeRolling] *** READY TO ROLL - DROP TO CREATE SMOKE ***");
			_MESSAGE("[SmokeRolling]   -> Roll of Paper in %s hand", hasRollOfPaperLeft ? "LEFT" : "RIGHT");
			_MESSAGE("[SmokeRolling]   -> Smokable '%s' in %s hand", smokableName, hasSmokableLeft ? "LEFT" : "RIGHT");
			_MESSAGE("[SmokeRolling]   -> Smokable category: %s", SmokableIngredients::GetCategoryName(smokableCategory));
			_MESSAGE("[SmokeRolling]   -> Controllers touching long enough: YES");
			_MESSAGE("[SmokeRolling]   -> Continuous haptic feedback started - DROP to roll!");
		}
	}

	// ============================================
	// Initialize Pipe Crafting System
	// ============================================
	void InitializePipeCrafting()
	{
		// Reset state
		g_heldKnifeLeft = nullptr;
		g_heldKnifeRight = nullptr;
		g_heldCraftingItemLeft = nullptr;
		g_heldCraftingItemRight = nullptr;
		g_craftingHitCount = 0;

		if (!higgsInterface)
		{
			_MESSAGE("[PipeCrafting] HIGGS interface not available - crafting disabled");
			return;
		}

		// Register HIGGS callbacks
		higgsInterface->AddGrabbedCallback(OnItemGrabbed);
		higgsInterface->AddDroppedCallback(OnItemDropped);
		higgsInterface->AddCollisionCallback(OnCraftingCollision);
		_MESSAGE("[PipeCrafting] Registered HIGGS grabbed, dropped, and collision callbacks");
	}

	// ============================================
	// Shutdown Pipe Crafting System
	// ============================================
	void ShutdownPipeCrafting()
	{
		g_heldKnifeLeft = nullptr;
		g_heldKnifeRight = nullptr;
		g_heldCraftingItemLeft = nullptr;
		g_heldCraftingItemRight = nullptr;
		g_craftingHitCount = 0;
		_MESSAGE("[PipeCrafting] Shutdown");
	}

	// ============================================
	// Check if an item is a valid crafting material for pipe making
	// Only firewood, wooden items, or bone items should trigger crafting
	// ============================================
	bool IsValidCraftingMaterial(TESForm* baseForm)
	{
		if (!baseForm)
			return false;

		// Check form type - must be a MISC item (type 32)
		if (baseForm->formType != kFormType_Misc)
			return false;

		// Get the item name for keyword-based detection
		const char* itemName = GetFormName(baseForm);
		if (!itemName || itemName[0] == '\0')
			return false;

		// Check for wood/firewood items
		if (strstr(itemName, "Firewood") || strstr(itemName, "firewood") ||
			strstr(itemName, "Wood") || strstr(itemName, "wood") ||
			strstr(itemName, "WOOD") || strstr(itemName, "FIREWOOD"))
		{
			return true;
		}

		// Check for bone items
		if (strstr(itemName, "Bone") || strstr(itemName, "bone") ||
			strstr(itemName, "BONE"))
		{
			return true;
		}

		// Also check specific form IDs that are known crafting materials
		// Firewood: 0x0006F993 (Skyrim.esm)
		if (baseForm->formID == 0x0006F993)
			return true;

		return false;
	}
}
