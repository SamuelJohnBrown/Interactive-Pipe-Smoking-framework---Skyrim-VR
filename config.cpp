#include "config.h"

namespace InteractivePipeSmokingVR {
		
	int logging = 0;
    int leftHandedMode = 0;

	// Face zone offset settings (relative to HMD/head position)
	// These offsets move the detection point from the head node to approximate lip position
	// Positive X = right, Positive Y = forward, Positive Z = up
	float configFaceZoneOffsetX = 0.0f;    // Left/Right offset (0 = centered)
	float configFaceZoneOffsetY = 10.0f;   // Forward offset (positive = in front of face, towards lips)
	float configFaceZoneOffsetZ = -5.0f;   // Down offset (negative = below head center, towards lips)
	float configFaceZoneRadius = 15.0f;    // Detection radius in game units

	// Controller touch detection
	float configControllerTouchRadius = 10.0f;  // Distance threshold for controllers "touching"

	// Near clip restore delay (milliseconds) - how long to wait after leaving face zone before restoring near clip
	int configNearClipRestoreDelayMs = 2000;  // 2 seconds default

	// Smokable ingredient scale when grabbed with empty pipe equipped (0.35 = 35% of original, 65% reduction)
	float configSmokableGrabbedScale = 0.50f;

	// Controller touch duration required for pipe filling (milliseconds)
	int configControllerTouchDurationMs = 1000;  // 1 seconds default

	// HIGGS MouthRadius when holding smokable with empty pipe (smaller = more precise)
	float configHiggsMouthRadiusSmokable = 3.0f;  // Default smaller radius for precision

	// Smoking Effect Settings
	float configEffectHealingHealth = 7.0f;
	float configEffectHealingStaminaCost = 2.0f;
	float configEffectMagicRegenMagicka = 5.0f;
	float configEffectMagicRegenStaminaCost = 2.0f;
	float configEffectStaminaRegenStamina = 7.0f;
	float configEffectStaminaRegenMagickaCost = 2.0f;
	int configSpecialInhalesToTrigger = 5;
	int configRecreationalInhalesToTrigger = 5;
	float configRecreationalEffectStrength = 0.09f;  // IMAD strength increment per inhale (default 0.09)
	float configRecreationalEffectDuration = 60.0f;  // IMAD duration in seconds
	int configRecreationalMaxInhales = 5;  // Maximum number of inhales that increase strength
	int configMagicRegenInhalesToCast = 5;  // Number of inhales before casting spell
	int configHealingInhalesToCast = 5;  // Number of inhales before casting healing spell
	int configMaxInhalesPerHerb = 25;  // Maximum inhales before herb depletes

    void loadConfig() 
    {
		std::string runtimeDirectory = GetRuntimeDirectory();

        if (!runtimeDirectory.empty()) 
		{
			std::string filepath = runtimeDirectory + "Data\\SKSE\\Plugins\\InteractiveHerbSmokingVR.ini";
			std::ifstream file(filepath);

			if (!file.is_open()) 
            {
				transform(filepath.begin(), filepath.end(), filepath.begin(), ::tolower);
				file.open(filepath);
     }

			if (file.is_open()) 
			{
				std::string line;
				std::string currentSection;

				while (std::getline(file, line)) 
				{
					trim(line);
					skipComments(line);

					if (line.empty()) continue;

					if (line[0] == '[') 
					{
						// New section
						size_t endBracket = line.find(']');
						if (endBracket != std::string::npos) 
						{
							currentSection = line.substr(1, endBracket - 1);
							trim(currentSection);       
						}
					}
					else if (currentSection == "Settings") 
					{
						std::string variableName;
						std::string variableValueStr = GetConfigSettingsStringValue(line, variableName);

						if (variableName == "Logging") 
						{
							logging = std::stoi(variableValueStr);
						}
						else if (variableName == "FaceZoneOffsetX")
						{
							configFaceZoneOffsetX = std::stof(variableValueStr);
						}
						else if (variableName == "FaceZoneOffsetY")
						{
							configFaceZoneOffsetY = std::stof(variableValueStr);
						}
						else if (variableName == "FaceZoneOffsetZ")
						{
							configFaceZoneOffsetZ = std::stof(variableValueStr);
						}
						else if (variableName == "FaceZoneRadius")
						{
							configFaceZoneRadius = std::stof(variableValueStr);
						}
						else if (variableName == "ControllerTouchRadius")
						{
							configControllerTouchRadius = std::stof(variableValueStr);
						}
						else if (variableName == "NearClipRestoreDelayMs")
						{
							configNearClipRestoreDelayMs = std::stoi(variableValueStr);
						}
						else if (variableName == "SmokableGrabbedScale")
						{
							configSmokableGrabbedScale = std::stof(variableValueStr);
						}
						else if (variableName == "ControllerTouchDurationMs")
						{
							configControllerTouchDurationMs = std::stoi(variableValueStr);
						}
						else if (variableName == "HiggsMouthRadiusSmokable")
						{
							configHiggsMouthRadiusSmokable = std::stof(variableValueStr);
						}
						else if (variableName == "EffectHealingHealth")
						{
							configEffectHealingHealth = std::stof(variableValueStr);
						}
						else if (variableName == "EffectHealingStaminaCost")
						{
							configEffectHealingStaminaCost = std::stof(variableValueStr);
						}
						else if (variableName == "EffectMagicRegenMagicka")
						{
							configEffectMagicRegenMagicka = std::stof(variableValueStr);
						}
						else if (variableName == "EffectMagicRegenStaminaCost")
						{
							configEffectMagicRegenStaminaCost = std::stof(variableValueStr);
						}
						else if (variableName == "EffectStaminaRegenStamina")
						{
							configEffectStaminaRegenStamina = std::stof(variableValueStr);
						}
						else if (variableName == "EffectStaminaRegenMagickaCost")
						{
							configEffectStaminaRegenMagickaCost = std::stof(variableValueStr);
						}
						else if (variableName == "SpecialInhalesToTrigger")
						{
							configSpecialInhalesToTrigger = std::stoi(variableValueStr);
						}
						else if (variableName == "RecreationalInhalesToTrigger")
						{
							configRecreationalInhalesToTrigger = std::stoi(variableValueStr);
						}
						else if (variableName == "RecreationalEffectStrength")
						{
							configRecreationalEffectStrength = std::stof(variableValueStr);
						}
						else if (variableName == "RecreationalEffectDuration")
						{
							configRecreationalEffectDuration = std::stof(variableValueStr);
						}
						else if (variableName == "RecreationalMaxInhales")
						{
							configRecreationalMaxInhales = std::stoi(variableValueStr);
						}
						else if (variableName == "MagicRegenInhalesToCast")
						{
							configMagicRegenInhalesToCast = std::stoi(variableValueStr);
						}
						else if (variableName == "HealingInhalesToCast")
						{
							configHealingInhalesToCast = std::stoi(variableValueStr);
						}
						else if (variableName == "MaxInhalesPerHerb")
						{
							configMaxInhalesPerHerb = std::stoi(variableValueStr);
						}
					}  
				} 
			}
			_MESSAGE("Config loaded.");
			return;
		}
		return;
    }

	void Log(const int msgLogLevel, const char* fmt, ...)
	{
		if (msgLogLevel > logging)
		{
			return;
		}

		va_list args;
		char logBuffer[4096];

		va_start(args, fmt);
		vsprintf_s(logBuffer, sizeof(logBuffer), fmt, args);
		va_end(args);

		_MESSAGE(logBuffer);
	}

}