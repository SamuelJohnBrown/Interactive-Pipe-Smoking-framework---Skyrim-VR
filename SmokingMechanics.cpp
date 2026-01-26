#include "SmokingMechanics.h"
#include "Engine.h"
#include "EquipState.h"
#include "Helper.h"
#include "SmokableIngredients.h"

#include "config.h"

#include "skse64/GameReferences.h"
#include "skse64/NiNodes.h"
#include "skse64/GameRTTI.h"

#include <chrono>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Global Inhale State
	// ============================================
	bool g_isInhaling = false;
	int g_inhaleCount = 0;

	// ============================================
	// Special Effect State (save game cooldown)
	// ============================================
	static std::chrono::steady_clock::time_point s_lastSpecialSaveTime;
	static bool s_specialSaveInitialized = false;
	
	// Cooldown for special save effect (4 minutes = 240 seconds)
	static const int SPECIAL_SAVE_COOLDOWN_SECONDS = 240;
	
	// Track inhales specifically for special effect trigger
	static int s_specialInhaleCount = 0;

	// ============================================
	// Magic Regen Effect State (spell casting)
	// ============================================
	// Track inhales for magic regen spell casting
	static int s_magicRegenInhaleCount = 0;

	// ============================================
	// Healing Effect State (spell casting)
	// ============================================
	// Track inhales for healing spell casting
	static int s_healingInhaleCount = 0;

	// ============================================
	// Recreational Effect State (Visuals)
	// ============================================
	// Track inhales specifically for recreational effect trigger
	static int s_recreationalInhaleCount = 0;
	
	// Current IMAD strength (increases with each inhale)
	static float s_currentRecreationalStrength = 0.0f;
	static bool s_recreationalEffectActive = false;
	
	// Track active IMAD count (for max limit)
	static std::atomic<int> s_activeRecreationalIMADCount{0};

	// Cooldown for recreational effect (65 seconds)
	static const int RECREATIONAL_COOLDOWN_SECONDS = 65;
	static std::chrono::steady_clock::time_point s_lastRecreationalTime;
	static bool s_recreationalInitialized = false;

	// ============================================
	// Player Movement State
	// ============================================
	bool g_playerIsMoving = false;
	static bool s_prevPlayerIsMoving = false;
	static NiPoint3 s_lastPlayerPosition = { 0, 0, 0 };
	static bool s_positionInitialized = false;

	// Movement detection threshold (units per update cycle)
	// Player needs to move more than this distance to be considered "moving"
	static const float MOVEMENT_THRESHOLD = 5.0f;

	// ============================================
	// Glow Node State
	// ============================================
	static NiAVObject* s_cachedGlowNode = nullptr;
	static float s_glowNodeDefaultScale = 1.0f;
	static bool s_glowNodeVisible = false;
	static bool s_prevInhaling = false;

	// Helper to check if the cached glow node is still valid
	static bool IsGlowNodeValid()
	{
		if (!s_cachedGlowNode)
			return false;

		// Check if the node pointer is still valid by checking its vtable
		// This is a basic sanity check - the node might have been deleted
		__try
		{
			// Try to access the node's name - if it crashes, the node is invalid
			const char* name = s_cachedGlowNode->m_name;
			if (name && strcmp(name, GLOW_NODE_NAME) == 0)
				return true;
			return false;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			// Node is invalid, clear the cache
			s_cachedGlowNode = nullptr;
			return false;
		}
	}

	// ============================================
	// Internal Inhale State
	// ============================================
	static bool s_litItemNearFace = false;
	static bool s_prevLitItemNearFace = false;
	static bool s_inhalePending = false;
	static bool s_herbDepletionTriggered = false;  // Prevent multiple depletion triggers
	static bool s_firstUpdateAfterInit = true;     // Prevent false triggers on first update
	static std::chrono::steady_clock::time_point s_faceZoneEntryTime;

	// Duration threshold for inhale (1.5 seconds)
	static const int INHALE_DURATION_MS = 1500;

	// ============================================
	// Play Inhale Sound (one of 2 random burning sounds)
	// ============================================
	static void PlayInhaleSound()
	{
		// Use the same burning sounds as the lighting mechanic
		UInt32 soundFormId = 0;
		if (g_burningSound1FullFormId != 0 && g_burningSound2FullFormId != 0)
		{
			// Use simple rand() to pick one (0 or 1)
			int selection = rand() % 2;
			soundFormId = (selection == 0) ? g_burningSound1FullFormId : g_burningSound2FullFormId;
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
		}
	}

	// ============================================
	// Handle Herb Depletion - swap lit pipe to empty pipe
	// ============================================
	static void HandleHerbDepletion()
	{
		if (s_herbDepletionTriggered)
			return;

		s_herbDepletionTriggered = true;
		_MESSAGE("[Inhale] Herb depleted after %d inhales - swapping to empty pipe", g_inhaleCount);

		// Clear the active smokable (type-specific cache will be cleared on unequip)
		g_activeSmokableFormId = 0;
		g_activeSmokableCategory = SmokableCategory::None;

		// Use EquipStateManager to handle the swap
		if (g_equipStateManager)
		{
			g_equipStateManager->DepleteLitPipeToEmpty();
		}
	}

	// ============================================
	// Initialize Smoking Mechanics
	// ============================================
	void InitializeSmokingMechanics()
	{
		g_isInhaling = false;
		g_inhaleCount = 0;
		s_litItemNearFace = false;
		s_prevLitItemNearFace = false;
		s_inhalePending = false;
		s_herbDepletionTriggered = false;
		s_firstUpdateAfterInit = true;  // Mark that next update is first after init

		// Reset special effect counter (but NOT the cooldown timer - that persists)
		s_specialInhaleCount = 0;
		
		// Reset magic regen inhale counter
		s_magicRegenInhaleCount = 0;
		
		// Reset healing inhale counter
		s_healingInhaleCount = 0;
		
		// Reset recreational effect counter
		s_recreationalInhaleCount = 0;
		// Maintain cooldown state (s_lastRecreationalTime and s_recreationalInitialized)

		// Reset movement detection
		g_playerIsMoving = false;
		s_prevPlayerIsMoving = false;
		s_positionInitialized = false;
		s_lastPlayerPosition = { 0, 0, 0 };

		_MESSAGE("[SmokingMechanics] Initialized - all state reset");
	}

	// ============================================
	// Reset Smoking Mechanics (when lit item unequipped)
	// ============================================
	void ResetSmokingMechanics()
	{
		if (g_inhaleCount > 0)
		{
			_MESSAGE("[SmokingMechanics] Session ended - Total inhales: %d", g_inhaleCount);
		}

		g_isInhaling = false;
		g_inhaleCount = 0;
		s_litItemNearFace = false;
		s_prevLitItemNearFace = false;
		s_inhalePending = false;
		s_herbDepletionTriggered = false;
		s_firstUpdateAfterInit = true;  // Mark that next update is first after reset

		// Reset special effect counter (but NOT the cooldown timer - that persists across sessions)
		s_specialInhaleCount = 0;

		// Reset magic regen inhale counter
		s_magicRegenInhaleCount = 0;

		// Reset healing inhale counter
		s_healingInhaleCount = 0;

		// Reset recreational effect counter
		s_recreationalInhaleCount = 0;

		// Reset movement detection
		g_playerIsMoving = false;
		s_prevPlayerIsMoving = false;
		s_positionInitialized = false;
		s_lastPlayerPosition = { 0, 0, 0 };

		// Reset glow node state
		s_cachedGlowNode = nullptr;
		s_glowNodeVisible = false;
		s_prevInhaling = false;
	}

	// ============================================
	// Reset All Effect Timers (called on game load)
	// ============================================
	void ResetAllEffectTimers()
	{
		_MESSAGE("[SmokingMechanics] Resetting all effect timers and cooldowns (game load)");

		// Reset all inhale counters
		s_specialInhaleCount = 0;
		s_magicRegenInhaleCount = 0;
		s_healingInhaleCount = 0;
		s_recreationalInhaleCount = 0;

		// Reset special effect cooldown timer
		s_specialSaveInitialized = false;
		// s_lastSpecialSaveTime will be set fresh on next use

		// Reset recreational effect state
		s_recreationalEffectActive = false;
		s_currentRecreationalStrength = 0.0f;
		s_activeRecreationalIMADCount = 0;
		s_recreationalInitialized = false;
		// s_lastRecreationalTime will be set fresh on next use

		_MESSAGE("[SmokingMechanics] All effect timers and cooldowns reset");
	}

	// ============================================
	// Get Duration at Face Zone (in milliseconds)
	// ============================================
	static int GetFaceZoneDurationMs()
	{
		if (!s_litItemNearFace)
			return 0;

		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_faceZoneEntryTime);
		return static_cast<int>(duration.count());
	}

	// ============================================
	// Calculate distance between two points
	// ============================================
	static float CalculateDistance(const NiPoint3& a, const NiPoint3& b)
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float dz = a.z - b.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	// ============================================
	// Update Player Movement Detection
	// ============================================
	void UpdatePlayerMovementDetection()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		// Get player's current position
		NiPoint3 currentPosition = player->pos;

		// Initialize position on first call
		if (!s_positionInitialized)
		{
			s_lastPlayerPosition = currentPosition;
			s_positionInitialized = true;
			return;
		}

		// Calculate distance moved since last update
		float distanceMoved = CalculateDistance(currentPosition, s_lastPlayerPosition);

		// Store previous moving state
		s_prevPlayerIsMoving = g_playerIsMoving;

		// Determine if player is moving (moved more than threshold)
		g_playerIsMoving = (distanceMoved > MOVEMENT_THRESHOLD);

		// Log state changes
		if (g_playerIsMoving && !s_prevPlayerIsMoving)
		{
			_MESSAGE("[Movement] Player started moving");
		}
		else if (!g_playerIsMoving && s_prevPlayerIsMoving)
		{
			_MESSAGE("[Movement] Player stopped moving (stationary)");
		}

		// Update last position
		s_lastPlayerPosition = currentPosition;
	}

	// ============================================
	// Update Smoking Mechanics (called every frame)
	// ============================================
	void UpdateSmokingMechanics(bool litItemNearFace)
	{
		// On first update after init/reset, just set current state without triggering transitions
		if (s_firstUpdateAfterInit)
		{
			s_litItemNearFace = litItemNearFace;
			s_prevLitItemNearFace = litItemNearFace;  // Set both to same value to prevent false transitions
			s_firstUpdateAfterInit = false;
			_MESSAGE("[SmokingMechanics] First update after init - synced state (nearFace=%d)", litItemNearFace ? 1 : 0);
			return;
		}

		s_prevLitItemNearFace = s_litItemNearFace;
		s_litItemNearFace = litItemNearFace;

		// Update player movement detection
		UpdatePlayerMovementDetection();

		// Update glow node visibility based on inhale state
		UpdateGlowNodeVisibility();

		// Check if herb is depleted - if so, trigger swap
		if (IsHerbDepleted() && !s_herbDepletionTriggered)
		{
			HandleHerbDepletion();
			return;  // Stop processing, pipe is being swapped
		}

		// Detect entry into face zone - start inhale
		if (s_litItemNearFace && !s_prevLitItemNearFace)
		{
			s_faceZoneEntryTime = std::chrono::steady_clock::now();
			g_isInhaling = false;
			s_inhalePending = false;

			// Play burning sound immediately when inhale starts
			PlayInhaleSound();
		}

		// Check if at face long enough to trigger inhale
		if (s_litItemNearFace && !s_inhalePending)
		{
			int durationMs = GetFaceZoneDurationMs();
			if (durationMs >= INHALE_DURATION_MS)
			{
				s_inhalePending = true;
				g_isInhaling = true;
			}
		}

		// Detect exit from face zone - complete the inhale (exhale)
		if (!s_litItemNearFace && s_prevLitItemNearFace)
		{
			if (s_inhalePending)
			{
				// Inhale complete - now exhaling!
				g_inhaleCount++;

				// Log inhale with active smokable info
				if (g_activeSmokableFormId != 0)
				{
					const char* smokableName = SmokableIngredients::GetSmokableName(g_activeSmokableFormId);
					const char* categoryName = SmokableIngredients::GetCategoryName(g_activeSmokableCategory);
					_MESSAGE("[Inhale] #%d/%d - %s (%s)", g_inhaleCount, configMaxInhalesPerHerb, smokableName, categoryName);
				}
				else
				{
					_MESSAGE("[Inhale] #%d/%d - No active smokable (random effect)", g_inhaleCount, configMaxInhalesPerHerb);
				}

				// Play smoke exhale visual effect at player
				PlaySmokeExhaleEffect();

				// Apply effects based on the active smokable category
				ApplySmokingEffects();

				s_inhalePending = false;
				g_isInhaling = false;
			}
		}
	}

	// ============================================
	// Getters
	// ============================================
	int GetInhaleCount()
	{
		return g_inhaleCount;
	}

	bool IsInhaling()
	{
		return g_isInhaling;
	}

	bool IsPlayerMoving()
	{
		return g_playerIsMoving;
	}

	bool IsHerbDepleted()
	{
		return g_inhaleCount >= configMaxInhalesPerHerb;
	}

	// ============================================
	// Glow Node Control Functions
	// ============================================

	// Helper to find a node by name recursively
	static NiAVObject* FindNodeByNameRecursive(NiAVObject* root, const char* nodeName)
	{
		if (!root || !nodeName)
			return nullptr;

		// Check if this node matches
		if (root->m_name && strcmp(root->m_name, nodeName) == 0)
			return root;

		// If it's a NiNode, search children
		NiNode* node = root->GetAsNiNode();
		if (node)
		{
			for (UInt32 i = 0; i < node->m_children.m_size; ++i)
			{
				NiAVObject* child = node->m_children.m_data[i];
				if (child)
				{
					NiAVObject* found = FindNodeByNameRecursive(child, nodeName);
					if (found)
						return found;
				}
			}
		}

		return nullptr;
	}

	void FindAndCacheGlowNode()
	{
		s_cachedGlowNode = nullptr;

		Actor* player = *g_thePlayer;
		if (!player)
		{
			_MESSAGE("[GlowNode] Player not available");
			return;
		}

		// Get the player's 3D root node
		NiNode* playerRoot = player->GetNiNode();
		if (!playerRoot)
		{
			_MESSAGE("[GlowNode] Player root node not available");
			return;
		}

		// The glow node is on the lit pipe armor mesh, which is attached to the player skeleton
		// We need to search the entire player skeleton hierarchy for the node
		// The armor mesh should be attached somewhere under the player's skeleton
		
		// First try to find by searching the whole player hierarchy
		NiAVObject* glowNode = FindNodeByNameRecursive(playerRoot, GLOW_NODE_NAME);
		
		// If not found on player root, try the first person skeleton
		if (!glowNode)
		{
			PlayerCharacter* playerChar = DYNAMIC_CAST(player, Actor, PlayerCharacter);
			if (playerChar && playerChar->firstPersonSkeleton)
			{
				glowNode = FindNodeByNameRecursive(playerChar->firstPersonSkeleton, GLOW_NODE_NAME);
				if (glowNode)
				{
					_MESSAGE("[GlowNode] Found '%s' on first person skeleton", GLOW_NODE_NAME);
				}
			}
		}

		// If still not found, try the loaded 3D state
		if (!glowNode && player->loadedState && player->loadedState->node)
		{
			glowNode = FindNodeByNameRecursive(player->loadedState->node, GLOW_NODE_NAME);
			if (glowNode)
			{
				_MESSAGE("[GlowNode] Found '%s' on loaded state node", GLOW_NODE_NAME);
			}
		}

		if (glowNode)
		{
			s_cachedGlowNode = glowNode;
			s_glowNodeDefaultScale = glowNode->m_localTransform.scale;
			_MESSAGE("[GlowNode] Found and cached '%s' node (default scale: %.2f)", GLOW_NODE_NAME, s_glowNodeDefaultScale);
		}
		else
		{
			_MESSAGE("[GlowNode] WARNING: Could not find '%s' node in player hierarchy", GLOW_NODE_NAME);
		}
	}

	void ShowGlowNode()
	{
		if (!IsGlowNodeValid())
			return;

		if (s_glowNodeVisible)
			return;  // Already visible

		s_cachedGlowNode->m_localTransform.scale = s_glowNodeDefaultScale;
		
		// Update world transforms
		NiAVObject::ControllerUpdateContext ctx;
		ctx.flags = 0;
		ctx.delta = 0;
		s_cachedGlowNode->UpdateWorldData(&ctx);

		s_glowNodeVisible = true;
		_MESSAGE("[GlowNode] Glow shown (scale: %.2f)", s_glowNodeDefaultScale);
	}

	void HideGlowNode()
	{
		if (!IsGlowNodeValid())
			return;

		if (!s_glowNodeVisible)
			return;  // Already hidden

		s_cachedGlowNode->m_localTransform.scale = 0.0f;
		
		// Update world transforms
		NiAVObject::ControllerUpdateContext ctx;
		ctx.flags = 0;
		ctx.delta = 0;
		s_cachedGlowNode->UpdateWorldData(&ctx);

		s_glowNodeVisible = false;
		_MESSAGE("[GlowNode] Glow hidden (scale: 0)");
	}

	void UpdateGlowNodeVisibility()
	{
		// Only process if we have a valid glow node
		if (!IsGlowNodeValid())
		{
			s_prevInhaling = g_isInhaling;
			return;
		}

		// Show glow when inhaling, hide when not
		if (g_isInhaling && !s_prevInhaling)
		{
			// Just started inhaling - show glow
			ShowGlowNode();
		}
		else if (!g_isInhaling && s_prevInhaling)
		{
			// Just stopped inhaling (exhaling now) - hide glow
			HideGlowNode();
		}

		s_prevInhaling = g_isInhaling;
	}

	// Thread function to find glow node after delay (with retry)
	static void DelayedFindGlowNodeThread(int delayMs)
	{
		// Initial delay to allow armor mesh to load
		std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
		
		// Try to find the node, with retries
		const int MAX_RETRIES = 5;
		const int RETRY_DELAY_MS = 200;

		for (int attempt = 0; attempt < MAX_RETRIES; ++attempt)
		{
			// Find and cache the glow node
			FindAndCacheGlowNode();

			// If found, hide it and we're done
			if (s_cachedGlowNode)
			{
				HideGlowNode();
				_MESSAGE("[GlowNode] Successfully found glow node on attempt %d", attempt + 1);
				return;
			}

			// Wait before retry
			if (attempt < MAX_RETRIES - 1)
			{
				_MESSAGE("[GlowNode] Node not found, retrying in %dms (attempt %d/%d)", RETRY_DELAY_MS, attempt + 1, MAX_RETRIES);
				std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
			}
		}

		_MESSAGE("[GlowNode] WARNING: Failed to find glow node after %d attempts", MAX_RETRIES);
	}

	void OnLitPipeEquipped()
	{
		_MESSAGE("[GlowNode] Lit pipe equipped - scheduling glow node search in 500ms");
		
		// Reset glow state
		s_cachedGlowNode = nullptr;
		s_glowNodeVisible = true;  // Assume visible initially so HideGlowNode will work
		s_prevInhaling = false;

		// Find glow node after 500ms delay to allow armor mesh to fully load
		std::thread findThread(DelayedFindGlowNodeThread, 500);
		findThread.detach();
	}

	// ============================================
	// Effect Application Functions
	// ============================================

	void ApplyMagicRegenEffect()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		// Apply standard per-inhale effect
		RestoreActorValue(player, "Magicka", configEffectMagicRegenMagicka);
		DamageActorValue(player, "Stamina", configEffectMagicRegenStaminaCost);
		_MESSAGE("[Effect] Applied MAGIC_REGEN: +%.1f Magicka, -%.1f Stamina", configEffectMagicRegenMagicka, configEffectMagicRegenStaminaCost);

		// Track inhales for spell casting
		s_magicRegenInhaleCount++;
		_MESSAGE("[Effect] MAGIC_REGEN inhale #%d/%d", s_magicRegenInhaleCount, configMagicRegenInhalesToCast);

		// Check if we've reached the threshold to cast spell
		if (s_magicRegenInhaleCount >= configMagicRegenInhalesToCast)
		{
			// Reset inhale counter
			s_magicRegenInhaleCount = 0;

			// Cast spell on player (Skyrim.esm 0x0004DEE8)
			const UInt32 MAGIC_REGEN_SPELL_FORM_ID = 0x0004DEE8;
			
			_MESSAGE("[Effect] MAGIC_REGEN: Casting spell %08X on player", MAGIC_REGEN_SPELL_FORM_ID);
			CastSpellOnPlayer(MAGIC_REGEN_SPELL_FORM_ID);
		}
	}

	void ApplyHealingEffect()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		// Apply standard per-inhale effect
		RestoreActorValue(player, "Health", configEffectHealingHealth);
		DamageActorValue(player, "Stamina", configEffectHealingStaminaCost);
		_MESSAGE("[Effect] Applied HEALING: +%.1f Health, -%.1f Stamina", configEffectHealingHealth, configEffectHealingStaminaCost);

		// Track inhales for spell casting
		s_healingInhaleCount++;
		_MESSAGE("[Effect] HEALING inhale #%d/%d", s_healingInhaleCount, configHealingInhalesToCast);

		// Check if we've reached the threshold to cast spell
		if (s_healingInhaleCount >= configHealingInhalesToCast)
		{
			// Reset inhale counter
			s_healingInhaleCount = 0;

			// Cast spell on player (Skyrim.esm 0x0007E8DD)
			const UInt32 HEALING_SPELL_FORM_ID = 0x0007E8DD;
			
			_MESSAGE("[Effect] HEALING: Casting spell %08X on player", HEALING_SPELL_FORM_ID);
			CastSpellOnPlayer(HEALING_SPELL_FORM_ID);
		}
	}

	void ApplyStaminaRegenEffect()
	{
		Actor* player = *g_thePlayer;
		if (!player)
			return;

		RestoreActorValue(player, "Stamina", configEffectStaminaRegenStamina);
		DamageActorValue(player, "Magicka", configEffectStaminaRegenMagickaCost);
		_MESSAGE("[Effect] Applied STAMINA_REGEN: +%.1f Stamina, -%.1f Magicka", configEffectStaminaRegenStamina, configEffectStaminaRegenMagickaCost);
	}

	void ApplyRecreationalEffect()
	{
		// Apply Recreational Effect: Apply a random IMAD on each inhale, stacking on top of previous ones
		// Each IMAD has its own independent duration timer
		// Strength is configRecreationalEffectStrength (default 0.09) per inhale
		// Maximum active IMADs controlled by configRecreationalMaxInhales
		
		// Available IMAD base IDs (from the same ESP)
		const UInt32 RECREATIONAL_IMAD_BASE_IDS[] = {
			0x014C3C,  // SmokeNirnISFX 1
			0x014C3C   // SmokeNirnISFX 2 - add different base IDs here
		};
		const int NUM_IMADS = sizeof(RECREATIONAL_IMAD_BASE_IDS) / sizeof(RECREATIONAL_IMAD_BASE_IDS[0]);
		
		// Pick a random IMAD
		int randomIndex = rand() % NUM_IMADS;
		UInt32 selectedBaseId = RECREATIONAL_IMAD_BASE_IDS[randomIndex];
		
		UInt32 fullFormId = GetFullFormIdMine(ESP_NAME, selectedBaseId);
		
		if (fullFormId == 0)
		{
			_MESSAGE("[Effect] RECREATIONAL: Could not resolve IMAD form %08X", selectedBaseId);
			return;
		}

		s_recreationalInhaleCount++;
		
		// Check if we've reached max active IMADs
		int currentActiveCount = s_activeRecreationalIMADCount.load();
		if (currentActiveCount >= configRecreationalMaxInhales)
		{
			_MESSAGE("[Effect] RECREATIONAL: Inhale #%d - already at max active IMADs (%d), waiting for one to expire", 
				s_recreationalInhaleCount, configRecreationalMaxInhales);
			return;
		}
		
		// Increment active count
		s_activeRecreationalIMADCount++;
		s_recreationalEffectActive = true;
		
		// Apply the IMAD at configured strength
		_MESSAGE("[Effect] RECREATIONAL: Inhale #%d - applying IMAD %08X at strength %.2f (active: %d/%d)", 
			s_recreationalInhaleCount, fullFormId, configRecreationalEffectStrength, 
			s_activeRecreationalIMADCount.load(), configRecreationalMaxInhales);
		ApplyImageSpaceModifier(fullFormId, configRecreationalEffectStrength, 0.0f);  // 0 duration = indefinite
		
		// Advance game time by 1 hour on each inhale
		AdvanceGameTime(1.0f);
		
		// Schedule removal for THIS specific IMAD after configured duration
		std::thread([fullFormId]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(configRecreationalEffectDuration * 1000.0f)));
			
			// Remove this specific IMAD
			RemoveImageSpaceModifier(fullFormId);
			
			// Decrement active count
			int remaining = --s_activeRecreationalIMADCount;
			
			_MESSAGE("[Effect] RECREATIONAL: IMAD %08X expired after %.1f seconds (remaining active: %d)", 
				fullFormId, configRecreationalEffectDuration, remaining);
			
			// If no more active IMADs, reset state
			if (remaining <= 0)
			{
				s_recreationalEffectActive = false;
				s_recreationalInhaleCount = 0;
				_MESSAGE("[Effect] RECREATIONAL: All effects expired, state reset");
			}
		}).detach();
	}

	void ApplyRandomEffect()
	{
		// Pick a random effect from Healing, MagicRegen, or StaminaRegen
		int roll = rand() % 3;
		switch (roll)
		{
			case 0:
				ApplyHealingEffect();
				break;
			case 1:
				ApplyMagicRegenEffect();
				break;
			case 2:
				ApplyStaminaRegenEffect();
				break;
		}
	}

	void ApplySpecialEffect()
	{
		// Special effect: After inhales threshold, save the game (with 4 min cooldown)
		s_specialInhaleCount++;
		_MESSAGE("[Effect] SPECIAL inhale #%d/%d", s_specialInhaleCount, configSpecialInhalesToTrigger);

		// Check if we've reached the threshold
		if (s_specialInhaleCount >= configSpecialInhalesToTrigger)
		{
			// Reset inhale counter
			s_specialInhaleCount = 0;

			// Check cooldown
			auto now = std::chrono::steady_clock::now();
			
			if (s_specialSaveInitialized)
			{
				auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - s_lastSpecialSaveTime);
				int secondsRemaining = SPECIAL_SAVE_COOLDOWN_SECONDS - static_cast<int>(elapsed.count());
				
				if (secondsRemaining > 0)
				{
					int minutesRemaining = secondsRemaining / 60;
					int secsRemaining = secondsRemaining % 60;
					_MESSAGE("[Effect] SPECIAL: Save on cooldown - %d:%02d remaining", minutesRemaining, secsRemaining);
					return;
				}
			}

			// Cooldown passed or first time - save the game!
		 s_lastSpecialSaveTime = now;
		 s_specialSaveInitialized = true;

			// Generate a save name with timestamp
			auto timeNow = std::chrono::system_clock::now();
			auto timeT = std::chrono::system_clock::to_time_t(timeNow);
			struct tm timeInfo;
			localtime_s(&timeInfo, &timeT);
			
			char saveName[64];
			snprintf(saveName, sizeof(saveName), "PipeSmoke_%04d%02d%02d_%02d%02d%02d",
				timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
				timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

			_MESSAGE("[Effect] SPECIAL: *** TRIGGERING SAVE GAME *** Name: '%s'", saveName);
			
			if (RequestSaveGame(saveName))
			{
				_MESSAGE("[Effect] SPECIAL: Save game request successful!");
			}
			else
			{
				_MESSAGE("[Effect] SPECIAL: Save game request FAILED!");
			}
		}
		else
		{
			_MESSAGE("[Effect] SPECIAL: %d more inhales needed for save", configSpecialInhalesToTrigger - s_specialInhaleCount);
		}
	}

	void ApplySmokingEffects()
	{
		// Apply effects based on active smokable category
		switch (g_activeSmokableCategory)
		{
			case SmokableCategory::Healing:
				ApplyHealingEffect();
				break;

			case SmokableCategory::MagicRegen:
				ApplyMagicRegenEffect();
				break;

			case SmokableCategory::StaminaRegen:
				ApplyStaminaRegenEffect();
				break;

			case SmokableCategory::Recreational:
				// Visual effects only - no stat changes
				ApplyRecreationalEffect();
				break;

			case SmokableCategory::Special:
				ApplySpecialEffect();
				break;

			case SmokableCategory::None:
			default:
				// No active smokable - apply random effect
				_MESSAGE("[Effect] No active smokable - applying random effect");
				ApplyRandomEffect();
				break;
		}
	}
}
