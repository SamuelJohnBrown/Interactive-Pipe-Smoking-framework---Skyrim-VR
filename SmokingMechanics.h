#pragma once

#include "skse64/NiNodes.h"

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Smoking Mechanics
	// Tracks inhales, applies effects based on cached herb, etc.
	// ============================================

	// Global inhale state
	extern bool g_isInhaling;       // True when lit item has been at face for 2+ seconds
	extern int g_inhaleCount;       // Number of inhales since equipping lit item

	// Player movement state
	extern bool g_playerIsMoving;   // True when player is moving

	// Glow node name on lit pipe mesh
	constexpr const char* GLOW_NODE_NAME = "Glow:1";

	// Initialize smoking mechanics
	void InitializeSmokingMechanics();

	// Reset smoking mechanics (called when lit item is unequipped)
	void ResetSmokingMechanics();

	// Reset all effect timers and cooldowns (called on game load)
	void ResetAllEffectTimers();

	// Called every frame to update inhale detection
	// Parameters:
	//   litItemNearFace - is the lit smoke item currently near the face zone
	void UpdateSmokingMechanics(bool litItemNearFace);

	// Update player movement detection (called every frame when lit item equipped)
	void UpdatePlayerMovementDetection();

	// Get the current inhale count
	int GetInhaleCount();

	// Check if currently inhaling
	bool IsInhaling();

	// Check if player is currently moving
	bool IsPlayerMoving();

	// Check if herb is depleted (inhale count >= max)
	bool IsHerbDepleted();

	// ============================================
	// Glow Node Control Functions
	// ============================================

	// Called when lit pipe is equipped (with delay to allow mesh loading)
	void OnLitPipeEquipped();

	// Find and cache the glow node from the player's equipped armor
	void FindAndCacheGlowNode();

	// Show the glow node (scale to default)
	void ShowGlowNode();

	// Hide the glow node (scale to 0)
	void HideGlowNode();

	// Update glow node visibility based on inhale state
	void UpdateGlowNodeVisibility();

	// ============================================
	// Effect Application Functions
	// ============================================

	// Apply Magic Regen effect: +5 magicka, -2 stamina per inhale
	void ApplyMagicRegenEffect();

	// Apply Healing effect: +5 health per inhale
	void ApplyHealingEffect();

	// Apply Stamina Regen effect: +5 stamina, -2 magicka per inhale
	void ApplyStaminaRegenEffect();

	// Apply effects based on the cached smokable category
	void ApplySmokingEffects();
}
