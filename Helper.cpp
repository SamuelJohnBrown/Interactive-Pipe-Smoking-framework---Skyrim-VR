#include "Helper.h"
#include "Engine.h"

namespace InteractivePipeSmokingVR
{
	// External task interface from main.cpp
	extern SKSETaskInterface* g_task;

	// RemoveItem native function address
	typedef void(*_RemoveItem_Native)(VMClassRegistry* registry, UInt32 stackId, TESObjectREFR* akSource, TESForm* akItemToRemove, SInt32 aiCount, bool abSilent, TESObjectREFR* akOtherContainer);
	RelocAddr<_RemoveItem_Native> RemoveItem_Native(0x009D1190);

	// RestoreActorValue native function address (Papyrus Actor.RestoreActorValue)
	typedef void(*_RestoreActorValue_Native)(VMClassRegistry* registry, UInt32 stackId, Actor* actor, BSFixedString const& valueName, float amount);
	RelocAddr<_RestoreActorValue_Native> RestoreActorValue_Native(0x0986480);

	// DamageActorValue native function address (Papyrus Actor.DamageActorValue)
	typedef void(*_DamageActorValue_Native)(VMClassRegistry* registry, UInt32 stackId, Actor* actor, BSFixedString const& valueName, float amount);
	RelocAddr<_DamageActorValue_Native> DamageActorValue_Native(0x09848B0);

	// ApplyImageSpaceModifier native function address (matching WeaponThrowVR's working implementation)
	typedef void(*_ApplyImageSpaceModifier_Native)(VMClassRegistry* registry, UInt32 stackId, TESImageSpaceModifier* imad, float strength);
	RelocAddr<_ApplyImageSpaceModifier_Native> ApplyImageSpaceModifier_Native(0x009C3C70);

	// RemoveImageSpaceModifier native function address (matching WeaponThrowVR's working implementation)
	typedef void(*_RemoveImageSpaceModifier_Native)(VMClassRegistry* registry, UInt32 stackId, TESImageSpaceModifier* imad);
	RelocAddr<_RemoveImageSpaceModifier_Native> RemoveImageSpaceModifier_Native(0x009C3CE0);

	std::uintptr_t Write5Call(std::uintptr_t a_src, std::uintptr_t a_dst)
	{
		const auto disp = reinterpret_cast<std::int32_t*>(a_src + 1);
		const auto nextOp = a_src + 5;
		const auto func = nextOp + *disp;
		g_branchTrampoline.Write5Call(a_src, a_dst);
		return func;
	}

	void LeftHandedModeChange()
	{
		const int value = vlibGetSetting("bLeftHandedMode:VRInput");
		if (value != leftHandedMode)
		{
			leftHandedMode = value;

			LOG("Left Handed Mode is %s.", leftHandedMode ? "ON" : "OFF");
		}
	}

	void ShowErrorBox(const char* errorString)
	{
		int msgboxID = MessageBox(
			NULL,
			(LPCTSTR)errorString,
			(LPCTSTR)"Interactive Pipe Smoking VR Fatal Error",
			MB_ICONERROR | MB_OK | MB_TASKMODAL
		);
	}

	void ShowErrorBoxAndLog(const char* errorString)
	{
		_ERROR(errorString);
		ShowErrorBox(errorString);
	}

	void ShowErrorBoxAndTerminate(const char* errorString)
	{
		ShowErrorBoxAndLog(errorString);
		*((int*)0) = 0xDEADBEEF; // crash
	}

	template<typename T>
	T* LoadFormAndLog(const std::string& pluginName, UInt32& fullFormId, UInt32 baseFormId, const char* formName) 
	{
		fullFormId = GetFullFormIdFromEspAndFormId(pluginName.c_str(), GetBaseFormID(baseFormId));
		if (fullFormId > 0) 
		{
			TESForm* form = LookupFormByID(fullFormId);
			if (form) 
			{
				T* castedForm = nullptr;
				if constexpr (std::is_same_v<T, BGSProjectile>) 
				{
					castedForm = DYNAMIC_CAST(form, TESForm, BGSProjectile);
				}
				else if constexpr (std::is_same_v<T, TESAmmo>) 
				{
					castedForm = DYNAMIC_CAST(form, TESForm, TESAmmo);
				}
				else if constexpr (std::is_same_v<T, TESObjectWEAP>) 
				{
					castedForm = DYNAMIC_CAST(form, TESForm, TESObjectWEAP);
				}
				else if constexpr (std::is_same_v<T, TESObjectREFR>) 
				{
					castedForm = DYNAMIC_CAST(form, TESForm, TESObjectREFR);
				}
				else if constexpr (std::is_same_v<T, BGSSoundDescriptorForm>) 
				{
					castedForm = DYNAMIC_CAST(form, TESForm, BGSSoundDescriptorForm);
				}

				if (castedForm) 
				{
					LOG_ERR("%s found. formid: %x", formName, fullFormId);
					return castedForm;
				}
				else 
				{
					LOG_ERR("%s null. formid: %x", formName, fullFormId);
				}
			}
			else 
			{
				LOG_ERR("%s not found. formid: %x", formName, fullFormId);
			}
		}
		return nullptr;
	}

	void GameLoad()
	{
		LeftHandedModeChange();





	}


	void PostLoadGame()
	{
		if ((*g_thePlayer) && (*g_thePlayer)->loadedState)
		{
			_MESSAGE("[PostLoadGame] Game load detected - resetting mod state");
			
			// Reset all mod state when loading a game (including loading the same save again)
			ResetModState();
			
			// Re-cache VRIK nearClipDistance after load since it may have been changed
			if (vrikInterface)
			{
				g_vrikNearClipDistanceCached = false;
				g_originalVrikNearClipDistance = static_cast<float>(vrikInterface->getSettingDouble("nearClipDistance"));
				g_vrikNearClipDistanceCached = true;
				_MESSAGE("[PostLoadGame] Re-cached VRIK nearClipDistance: %.1f", g_originalVrikNearClipDistance);
			}
			
			// Re-cache HIGGS MouthRadius after load
			if (higgsInterface)
			{
				g_higgsMouthRadiusCached = false;
				g_higgsMouthRadiusModified = false;
				double mouthRadius = 0.0;
				if (higgsInterface->GetSettingDouble("MouthRadius", mouthRadius))
				{
					g_originalHiggsMouthRadius = mouthRadius;
					g_higgsMouthRadiusCached = true;
					_MESSAGE("[PostLoadGame] Re-cached HIGGS MouthRadius: %.2f", g_originalHiggsMouthRadius);
				}
			}
			
			_MESSAGE("[PostLoadGame] Post-load re-initialization complete");
		}
	}

	UInt32 GetFullFormIdMine(const char* espName, UInt32 baseFormId)
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
				std::pair<const char*, UInt32> formIdPair = { espName, baseFormId };
				
				const ModInfo* modInfo = NEWLookupAllLoadedModByName(formIdPair.first);
				if (modInfo)
				{
					if (IsValidModIndex(modInfo->modIndex)) //If plugin is in the load order.
					{
						fullFormID = GetFullFormID(modInfo, GetBaseFormID(formIdPair.second));
					}
				}
			}
		}
		return fullFormID;
	}

	void RemoveItemFromInventory(TESObjectREFR* target, TESForm* item, SInt32 count, bool silent)
	{
		if (!target || !item)
			return;
		
		RemoveItem_Native(nullptr, 0, target, item, count, silent, nullptr);
	}

	void RestoreActorValue(Actor* actor, const char* valueName, float amount)
	{
		if (!actor || !valueName || amount <= 0)
			return;

		BSFixedString valueNameStr(valueName);
		RestoreActorValue_Native((*g_skyrimVM)->GetClassRegistry(), 0, actor, valueNameStr, amount);
	}

	void DamageActorValue(Actor* actor, const char* valueName, float amount)
	{
		if (!actor || !valueName || amount <= 0)
			return;

		BSFixedString valueNameStr(valueName);
		DamageActorValue_Native((*g_skyrimVM)->GetClassRegistry(), 0, actor, valueNameStr, amount);
	}

	bool RequestSaveGame(const char* saveName)
	{
		if (!saveName || saveName[0] == '\0')
		{
			_MESSAGE("[SaveGame] ERROR: Invalid save name provided");
			return false;
		}

		BGSSaveLoadManager* saveLoadManager = BGSSaveLoadManager::GetSingleton();
		if (!saveLoadManager)
		{
			_MESSAGE("[SaveGame] ERROR: BGSSaveLoadManager not available");
			return false;
		}

		// Request the save - it will be processed on the next frame
		saveLoadManager->RequestSave(saveName);
		_MESSAGE("[SaveGame] Requested save game: '%s'", saveName);
		return true;
	}

	// ============================================
	// Spell Casting
	// ============================================

	// CastSpell native function address (from SpellWheelVR - Papyrus Spell.Cast)
	typedef bool(*_CastSpell)(VMClassRegistry* registry, UInt32 stackId, SpellItem* spell, TESObjectREFR* akSource, TESObjectREFR* akTarget);
	RelocAddr<_CastSpell> CastSpell(0x009BB6B0);

	// Task to cast spell on main thread
	class CastSpellOnPlayerTask : public TaskDelegate
	{
	public:
		UInt32 m_formId;

		CastSpellOnPlayerTask(UInt32 formId) : m_formId(formId) {}

		virtual void Run() override
		{
			Actor* player = *g_thePlayer;
			if (!player)
			{
				_MESSAGE("[CastSpell] ERROR: Player not available");
				return;
			}

			TESForm* form = LookupFormByID(m_formId);
			if (!form)
			{
				_MESSAGE("[CastSpell] ERROR: Spell form %08X not found", m_formId);
				return;
			}

			SpellItem* spell = DYNAMIC_CAST(form, TESForm, SpellItem);
			if (!spell)
			{
				_MESSAGE("[CastSpell] ERROR: Form %08X is not a SpellItem", m_formId);
				return;
			}

			// Cast the spell on the player (source = player, target = player for self-cast spells)
			bool result = CastSpell((*g_skyrimVM)->GetClassRegistry(), 0, spell, player, player);
			_MESSAGE("[CastSpell] Cast spell %08X on player, result: %s", m_formId, result ? "success" : "failed");
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	void CastSpellOnPlayer(UInt32 formId)
	{
		if (formId == 0) return;

		if (g_task)
		{
			g_task->AddTask(new CastSpellOnPlayerTask(formId));
		}
		_MESSAGE("[CastSpell] Queued spell cast %08X on player", formId);
	}

	class RemoveImageSpaceModifierTask : public TaskDelegate
	{
	public:
		UInt32 m_formId;

		RemoveImageSpaceModifierTask(UInt32 formId) : m_formId(formId) {}

		virtual void Run() override
		{
			TESForm* form = LookupFormByID(m_formId);
			if (!form) return;

			TESImageSpaceModifier* imad = DYNAMIC_CAST(form, TESForm, TESImageSpaceModifier);
			if (!imad) return;

			RemoveImageSpaceModifier_Native((*g_skyrimVM)->GetClassRegistry(), 0, imad);
			_MESSAGE("[IMAD] Removed ImageSpaceModifier %08X", m_formId);
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	// Task to apply IMAD with specific strength (for fading)
	class ApplyImageSpaceModifierTask : public TaskDelegate
	{
	public:
		UInt32 m_formId;
		float m_strength;

		ApplyImageSpaceModifierTask(UInt32 formId, float strength) : m_formId(formId), m_strength(strength) {}

		virtual void Run() override
		{
			TESForm* form = LookupFormByID(m_formId);
			if (!form) return;

			TESImageSpaceModifier* imad = DYNAMIC_CAST(form, TESForm, TESImageSpaceModifier);
			if (!imad) return;

			ApplyImageSpaceModifier_Native((*g_skyrimVM)->GetClassRegistry(), 0, imad, m_strength);
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	void ApplyImageSpaceModifier(UInt32 formId, float strength, float durationSeconds)
	{
		if (formId == 0) return;

		TESForm* form = LookupFormByID(formId);
		if (!form)
		{
			_MESSAGE("[IMAD] ERROR: Form %08X not found", formId);
			return;
		}

		TESImageSpaceModifier* imad = DYNAMIC_CAST(form, TESForm, TESImageSpaceModifier);
		if (!imad)
		{
			_MESSAGE("[IMAD] ERROR: Form %08X is not an ImageSpaceModifier", formId);
			return;
		}

		// Apply the modifier via task for thread safety
		if (g_task)
		{
			g_task->AddTask(new ApplyImageSpaceModifierTask(formId, strength));
		}
		_MESSAGE("[IMAD] Applied ImageSpaceModifier %08X (Strength: %.2f)", formId, strength);

		// If duration is specified, schedule removal
		if (durationSeconds > 0.0f)
		{
			int delayMs = static_cast<int>(durationSeconds * 1000.0f);
			
			std::thread([formId, delayMs]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
				
				if (g_task)
				{
					g_task->AddTask(new RemoveImageSpaceModifierTask(formId));
				}
			}).detach();
			
			_MESSAGE("[IMAD] Scheduled removal in %.1f seconds", durationSeconds);
		}
	}

	void RemoveImageSpaceModifier(UInt32 formId)
	{
		if (formId == 0) return;

		if (g_task)
		{
			g_task->AddTask(new RemoveImageSpaceModifierTask(formId));
		}
		_MESSAGE("[IMAD] Queued removal of ImageSpaceModifier %08X", formId);
	}

	// ============================================
	// Game Time Advancement
	// ============================================
	
	// Task to advance game time on main thread
	class AdvanceGameTimeTask : public TaskDelegate
	{
	public:
		float m_hours;

		AdvanceGameTimeTask(float hours) : m_hours(hours) {}

		virtual void Run() override
		{
			// GameHour global variable form ID (Skyrim.esm)
			const UInt32 GAME_HOUR_FORM_ID = 0x38;
			
			TESForm* form = LookupFormByID(GAME_HOUR_FORM_ID);
			if (!form)
			{
				_MESSAGE("[GameTime] ERROR: Could not find GameHour global (FormID: %08X)", GAME_HOUR_FORM_ID);
				return;
			}
			
			TESGlobal* gameHour = DYNAMIC_CAST(form, TESForm, TESGlobal);
			if (!gameHour)
			{
				_MESSAGE("[GameTime] ERROR: GameHour is not a TESGlobal");
				return;
			}
			
			// Get current hour (stored in unk34 as a float reinterpreted as UInt32)
			float currentHour = *reinterpret_cast<float*>(&gameHour->unk34);
			float newHour = currentHour + m_hours;
			
			// Handle day rollover (24 hour clock)
			while (newHour >= 24.0f)
			{
				newHour -= 24.0f;
			}
			
			// Set new hour
			*reinterpret_cast<float*>(&gameHour->unk34) = newHour;
			
			_MESSAGE("[GameTime] Advanced time by %.1f hours (%.1f -> %.1f)", m_hours, currentHour, newHour);
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	void AdvanceGameTime(float hours)
	{
		if (hours <= 0.0f) return;
		
		if (g_task)
		{
			g_task->AddTask(new AdvanceGameTimeTask(hours));
		}
		_MESSAGE("[GameTime] Queued time advancement by %.1f hours", hours);
	}

	// Apply IMAD with fade-in and fade-out effect
	// fadeInDuration: seconds to fade from 0 to max strength
	// activeDuration: total time effect is active (includes fade times)
	// maxStrength: maximum strength to fade to (0.0 to 1.0)
	void ApplyImageSpaceModifierCrossFade(UInt32 formId, float fadeInDuration, float activeDuration, float maxStrength)
	{
		if (formId == 0) return;

		TESForm* form = LookupFormByID(formId);
		if (!form)
		{
			_MESSAGE("[IMAD] ERROR: Form %08X not found", formId);
			return;
		}

		TESImageSpaceModifier* imad = DYNAMIC_CAST(form, TESForm, TESImageSpaceModifier);
		if (!imad)
		{
			_MESSAGE("[IMAD] ERROR: Form %08X is not an ImageSpaceModifier", formId);
			return;
		}

		_MESSAGE("[IMAD] Applying ImageSpaceModifier %08X with fade (FadeIn: %.1fs, Duration: %.1fs, MaxStrength: %.2f)", 
			formId, fadeInDuration, activeDuration, maxStrength);

		// Use same fade duration for fade-out
		float fadeOutDuration = fadeInDuration;

		// Start the fade thread
		std::thread([formId, fadeInDuration, activeDuration, fadeOutDuration, maxStrength]() {
			const int FADE_STEPS = 10;  // Number of steps for smooth fade
			const int FADE_STEP_DELAY_MS = static_cast<int>((fadeInDuration * 1000.0f) / FADE_STEPS);

			// === FADE IN ===
			for (int i = 1; i <= FADE_STEPS; i++)
			{
				float strength = (static_cast<float>(i) / static_cast<float>(FADE_STEPS)) * maxStrength;
				if (g_task)
				{
					g_task->AddTask(new ApplyImageSpaceModifierTask(formId, strength));
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(FADE_STEP_DELAY_MS));
			}
			_MESSAGE("[IMAD] Fade-in complete for %08X (strength: %.2f)", formId, maxStrength);

			// === HOLD AT MAX STRENGTH ===
			// Calculate hold time: total duration minus both fade times
			float holdTime = activeDuration - fadeInDuration - fadeOutDuration;
			if (holdTime > 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(holdTime * 1000.0f)));
			}

			// === FADE OUT ===
			const int FADE_OUT_STEP_DELAY_MS = static_cast<int>((fadeOutDuration * 1000.0f) / FADE_STEPS);
			for (int i = FADE_STEPS - 1; i >= 0; i--)
			{
				float strength = (static_cast<float>(i) / static_cast<float>(FADE_STEPS)) * maxStrength;
				if (g_task)
				{
					g_task->AddTask(new ApplyImageSpaceModifierTask(formId, strength));
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(FADE_OUT_STEP_DELAY_MS));
			}
			_MESSAGE("[IMAD] Fade-out complete for %08X", formId);

			// === REMOVE ===
			if (g_task)
			{
				g_task->AddTask(new RemoveImageSpaceModifierTask(formId));
			}
		}).detach();
	}
}