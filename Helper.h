#pragma once
#include "skse64/PapyrusSpell.h"
#include "skse64/PapyrusGame.h"
#include "skse64/PapyrusActor.h"
#include "skse64/PapyrusPotion.h"
#include "skse64/GameMenus.h"
#include "skse64_common/SafeWrite.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <xbyak/xbyak.h>
#include "skse64/NiExtraData.h"
#include "skse64_common/BranchTrampoline.h"
#include <skse64/GameRTTI.h>
#include <skse64/GameData.h>
#include <skse64/NiTypes.h>
#include <skse64/NiGeometry.h>
#include <skse64/GameExtraData.h>
#include <skse64/GameHandlers.h>

#include "skse64/NiExtraData.h"
#include <skse64/NiControllers.h>
#include "skse64/InternalTasks.h"

#include <deque>
#include <queue>
#include <array>
#include "skse64\GameVR.h"
#include <skse64/PapyrusEvents.h>

#include "config.h"

namespace InteractivePipeSmokingVR
{
	UInt32 GetFullFormIdMine(const char* espName, UInt32 baseFormId);
	void ShowErrorBoxAndTerminate(const char* errorString);
	void GameLoad();
	void PostLoadGame();

	// Item manipulation wrappers
	void RemoveItemFromInventory(TESObjectREFR* target, TESForm* item, SInt32 count, bool silent);

	// Delete world object (cleanup spawned/dropped items from the world)
	// This calls Papyrus ObjectReference.Delete to remove the reference from the game
	void DeleteWorldObject(TESObjectREFR* objRef);

	// Actor value manipulation wrappers
	void RestoreActorValue(Actor* actor, const char* valueName, float amount);
	void DamageActorValue(Actor* actor, const char* valueName, float amount);

	// Save game helper
	// Safely requests a game save with the given name
	// The save will be processed on the next frame by the game's save system
	// Returns true if the request was successfully queued, false otherwise
	bool RequestSaveGame(const char* saveName);

	// Cast spell on player helper
	// Casts the specified spell on the player
	// formId: The full form ID of the SpellItem to cast
	void CastSpellOnPlayer(UInt32 formId);

	// Advance game time helper function
	// Advances the in-game time by the specified number of hours
	void AdvanceGameTime(float hours);

	// Image Space Modifier helper
	// Applies an IMAD with a given strength and duration.
	// formId: The form ID of the TESImageSpaceModifier.
	// strength: The strength of the effect (0.0 to 1.0).
	// duration: How long the effect should last in seconds (0 = indefinite/handled by modifier).
	void ApplyImageSpaceModifier(UInt32 formId, float strength, float duration = 0.0f);

	// Removes an active IMAD.
	// formId: The form ID of the TESImageSpaceModifier to remove.
	void RemoveImageSpaceModifier(UInt32 formId);

	// Applies an IMAD with cross fade.
	// formId: The form ID of the TESImageSpaceModifier.
	// fadeDuration: The duration of the fade in effects.
	// activeDuration: Total time the effect is active before removal.
	// maxStrength: Maximum strength to fade to (0.0 to 1.0, default 1.0).
	void ApplyImageSpaceModifierCrossFade(UInt32 formId, float fadeDuration, float activeDuration, float maxStrength = 1.0f);
}