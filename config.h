#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <skse64/NiProperties.h>
#include <skse64/NiNodes.h>

#include "skse64\GameSettings.h"
#include "Utility.hpp"

#include <skse64/GameData.h>

#include "higgsinterface001.h"
#include "vrikinterface001.h"
#include "SkyrimVRESLAPI.h"

namespace InteractivePipeSmokingVR {

	const UInt32 MOD_VERSION = 0x10000;
	const std::string MOD_VERSION_STR = "1.0.0";
	extern int leftHandedMode;

	extern int logging;

	// Face zone offset settings (relative to HMD/head position)
	// Positive X = right, Positive Y = forward, Positive Z = up
	extern float configFaceZoneOffsetX;  // Left/Right offset
	extern float configFaceZoneOffsetY;  // Forward/Back offset (positive = towards face)
	extern float configFaceZoneOffsetZ;  // Up/Down offset (negative = down towards lips)
	extern float configFaceZoneRadius;   // Detection radius

	// Controller touch detection
	extern float configControllerTouchRadius;  // Distance threshold for controllers "touching"

	// Near clip restore delay (milliseconds)
	extern int configNearClipRestoreDelayMs;

	// Smokable ingredient scale when grabbed with empty pipe equipped (0.35 = 35% of original, 65% reduction)
	extern float configSmokableGrabbedScale;

	// Controller touch duration required for pipe filling (milliseconds)
	extern int configControllerTouchDurationMs;

	// HIGGS MouthRadius when holding smokable with empty pipe (smaller = more precise)
	extern float configHiggsMouthRadiusSmokable;

	// Smoking Effect Settings
	extern float configEffectHealingHealth;
	extern float configEffectHealingStaminaCost;
	extern float configEffectMagicRegenMagicka;
	extern float configEffectMagicRegenStaminaCost;
	extern float configEffectStaminaRegenStamina;
	extern float configEffectStaminaRegenMagickaCost;
	extern int configSpecialInhalesToTrigger;
	extern int configRecreationalInhalesToTrigger;
	extern float configRecreationalEffectStrength;
	extern float configRecreationalEffectDuration;
	extern int configRecreationalMaxInhales;
	extern int configMagicRegenInhalesToCast;
	extern int configHealingInhalesToCast;
	extern int configMaxInhalesPerHerb;

	void loadConfig();
	
	void Log(const int msgLogLevel, const char* fmt, ...);
	enum eLogLevels
	{
		LOGLEVEL_ERR = 0,
		LOGLEVEL_WARN,
		LOGLEVEL_INFO,
	};


#define LOG(fmt, ...) Log(LOGLEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) Log(LOGLEVEL_ERR, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) Log(LOGLEVEL_INFO, fmt, ##__VA_ARGS__)


}