#pragma once

#include <unordered_set>
#include <unordered_map>
#include "skse64/GameTypes.h"

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Smokable Ingredient Categories
	// Used to determine effects when smoked
	// ============================================
	enum class SmokableCategory
	{
		None = 0,
		Healing,        // Restores health
		MagicRegen,     // Restores/regenerates magicka
		StaminaRegen,   // Restores/regenerates stamina
		Recreational,   // Visual effects only (no gameplay effects)
		Special    // Special effects (save game, slow time, etc.)
	};

	// ============================================
	// Smokable Ingredients
	// These are vanilla Skyrim ingredients that can be smoked in pipes
	// All form IDs are from Skyrim.esm (load order 00)
	// ============================================

	// Smokable ingredient form IDs (base form IDs from Skyrim.esm)
	namespace SmokableFormIDs
	{
		// === HEALING ===
		constexpr UInt32 BlueMountainFlower = 0x00077E1C;   // Blue Mountain Flower
		constexpr UInt32 Wheat = 0x000669A5;           // Wheat
		constexpr UInt32 Blisterwort = 0x0004DA25;    // Blisterwort
		constexpr UInt32 ImpStool = 0x0004DA23;       // Imp Stool
		constexpr UInt32 Nirnroot = 0x00059B86;             // Nirnroot
		constexpr UInt32 GlowingMushroom = 0x0007EE01;      // Glowing Mushroom
		constexpr UInt32 SwampFungalPod = 0x0007E8B7;       // Swamp Fungal Pod
		constexpr UInt32 DragonsTongue = 0x000889A2;        // Dragon's Tongue

		// === MAGIC REGEN ===
		constexpr UInt32 RedMountainFlower = 0x00077E1D;  // Red Mountain Flower
		constexpr UInt32 MoraTapinella = 0x000EC870;        // Mora Tapinella
		constexpr UInt32 CreepCluster = 0x000B2183;  // Creep Cluster
		constexpr UInt32 Taproot = 0x0003AD71;          // Taproot
		constexpr UInt32 ElvesEar = 0x00034D31;             // Elves Ear
		constexpr UInt32 GiantLichen = 0x0007E8C1;          // Giant Lichen
		constexpr UInt32 FrostSalts = 0x0003AD5E;      // Frost Salts
		constexpr UInt32 FrostMirriam = 0x00034D32;    // Frost Mirriam
		constexpr UInt32 Snowberries = 0x0001B3BD;     // Snowberries

		// === STAMINA REGEN ===
		constexpr UInt32 PurpleMountainFlower = 0x00077E1E; // Purple Mountain Flower
		constexpr UInt32 HangingMoss = 0x00057F91;          // Hanging Moss
		constexpr UInt32 JuniperBerries = 0x0005076E;       // Juniper Berries
		constexpr UInt32 Lavender = 0x00045C28;  // Lavender
		constexpr UInt32 TundraCotton = 0x0003F7F8;         // Tundra Cotton
		constexpr UInt32 GrassPod = 0x00083E64;             // Grass Pod
		constexpr UInt32 FireSalts = 0x0003AD56;          // Fire Salts
		constexpr UInt32 ScalyPholiota = 0x0006F950;  // Scaly Pholiota
		constexpr UInt32 CanisRoot = 0x0006ABCB;            // Canis Root
		constexpr UInt32 ThistleBranch = 0x000134AA;  // Thistle Branch

		// === RECREATIONAL (Visual effects only - small list) ===
		constexpr UInt32 FlyAmanita = 0x0004DA00;   // Fly Amanita
		constexpr UInt32 NamirasRot = 0x0004DA24;// Namira's Rot
		constexpr UInt32 WhiteCap = 0x0004DA22;  // White Cap
		constexpr UInt32 BleedingCrown = 0x0004DA20;    // Bleeding Crown
		constexpr UInt32 Deathbell = 0x000516C8;        // Deathbell

		// === SPECIAL (Save game, slow time, unique effects) ===
		constexpr UInt32 CrimsonNirnroot = 0x000B701A;// Crimson Nirnroot
		constexpr UInt32 Nightshade = 0x0002F44C; // Nightshade
		constexpr UInt32 SprigganSap = 0x00063B5F;  // Spriggan Sap
	}

	// ============================================
	// Smokable Ingredient Checker
	// ============================================
	class SmokableIngredients
	{
	public:
		// Initialize the smokable ingredients set
		static void Initialize();

		// Check if a form ID is a smokable ingredient
		static bool IsSmokable(UInt32 formId);

		// Check if a form ID is a cannabis ingredient
		static bool IsCannabis(UInt32 formId);

		// Get the category of a smokable ingredient
		static SmokableCategory GetCategory(UInt32 formId);

		// Get the category name as a string
		static const char* GetCategoryName(SmokableCategory category);

		// Get the name of a smokable ingredient (for logging)
		static const char* GetSmokableName(UInt32 formId);

	private:
		static std::unordered_set<UInt32> s_smokableFormIds;
		static std::unordered_set<UInt32> s_cannabisFormIds;
		static std::unordered_map<UInt32, SmokableCategory> s_smokableCategories;
		static bool s_initialized;
	};

}
