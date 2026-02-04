#pragma once

#include "Helper.h"
#include "skse64/NiTypes.h"
#include "skse64/NiNodes.h"
#include <atomic>
#include <thread>
#include <chrono>

namespace InteractivePipeSmokingVR
{
	// ============================================
	// VR Input Tracker
	// Tracks controller and HMD positions for proximity detection
	// Also tracks equipped fire spells
	// ============================================

	// Distance threshold for "near face" detection (in game units)
	constexpr float NEAR_FACE_DISTANCE_THRESHOLD = 15.0f;

	// Near clip distance when smoking item is near face
	constexpr float NEAR_CLIP_SMOKING_NEAR_FACE = 5.0f;

	class VRInputTracker
	{
	public:
		VRInputTracker();
		~VRInputTracker();

		// Initialize/Shutdown tracking
		void Initialize();
		void Shutdown();

		// Start/Stop tracking (called when smoke items are equipped/unequipped)
		void StartTracking();
		void StopTracking();

		// Pause/Resume tracking (called when pause menus open/close)
		void PauseTracking();
		void ResumeTracking();

		// Check if tracking is active
		bool IsTracking() const { return m_isTracking && !m_isPaused; }

		// Check if tracking is paused (for menu detection)
		bool IsPaused() const { return m_isPaused; }

		// Update positions - called from game thread (e.g., from a hook or periodic check)
		void Update();

		// Schedule the next update (called after each update completes)
		void ScheduleNextUpdate();

		// Get current positions
		NiPoint3 GetHMDPosition() const { return m_hmdPosition; }
		NiPoint3 GetFaceTargetPosition() const { return m_faceTargetPosition; }
		NiPoint3 GetLeftControllerPosition() const { return m_leftControllerPosition; }
		NiPoint3 GetRightControllerPosition() const { return m_rightControllerPosition; }

		// Check if controller is near face
		bool IsLeftControllerNearFace() const { return m_leftNearFace; }
		bool IsRightControllerNearFace() const { return m_rightNearFace; }

		// Check if controllers are touching
		bool AreControllersTouching() const { return m_controllersTouching; }

		// Check if controllers are near enough for pipe filling (uses separate radius)
		bool AreControllersNearForPipeFilling() const { return m_controllersNearForPipeFilling; }

		// Check if controllers are near enough for smoke rolling (uses separate radius)
		bool AreControllersNearForSmokeRolling() const { return m_controllersNearForSmokeRolling; }

		// Check if controllers are near enough for pipe lighting (uses separate radius)
		bool AreControllersNearForPipeLighting() const { return m_controllersNearForPipeLighting; }

		// Check if controllers are near enough for rolled smoke lighting (uses larger radius)
		bool AreControllersNearForRolledSmokeLighting() const { return m_controllersNearForRolledSmokeLighting; }

		// Get how long controllers have been touching (in milliseconds), returns 0 if not touching
		int GetControllersTouchingDurationMs() const;

		// Check if fire spell is equipped
		bool IsFireSpellInLeftHand() const { return m_fireSpellLeftHand; }
		bool IsFireSpellInRightHand() const { return m_fireSpellRightHand; }

		// Get distance from controller to face target
		float GetLeftControllerToFaceDistance() const;
		float GetRightControllerToFaceDistance() const;

		// Get distance between controllers
		float GetControllerToControllerDistance() const;

		// Set which hand has a smoke item equipped
		void SetSmokeItemEquippedHand(bool leftHand, bool rightHand);

		// Check if smoke item hand is near face
		bool IsSmokeItemHandNearFace() const;

		// Set which hand has a herb pipe equipped (for rotation tracking)
		void SetHerbPipeEquippedHand(bool leftHand, bool rightHand);

		// Set which hand has an unlit rolled smoke equipped (for lighting detection)
		void SetUnlitRolledSmokeEquippedHand(bool leftHand, bool rightHand);

		// Set which hand has a lit item equipped (for smoking mechanics)
		void SetLitItemEquippedHand(bool leftHand, bool rightHand);

		// Check if herb pipe hand is flipped (facing down) long enough
		bool IsHerbPipeHandFlippedLongEnough() const { return m_herbPipeFlippedLongEnough; }

		// Get how long the herb pipe hand has been flipped (in milliseconds)
		int GetHerbPipeFlippedDurationMs() const;

		// Force restore near clip distance (called when smoke item is unequipped)
		void ForceRestoreNearClipDistance();

		// Check if lighting condition is met (fire spell + herb pipe/unlit smoke + hands touching)
		bool IsLightingConditionMet() const { return m_lightingConditionMet; }

		// Get how long the lighting condition has been met (in milliseconds)
		int GetLightingConditionDurationMs() const;

	private:
		std::atomic<bool> m_isTracking;
		std::atomic<bool> m_isPaused;  // True when paused due to menu being open
		bool m_isInitialized;
		std::atomic<bool> m_updatePending;

		// Current positions
		NiPoint3 m_hmdPosition;
		NiPoint3 m_faceTargetPosition;  // HMD position with offset applied (targets lips)
		NiPoint3 m_leftControllerPosition;
		NiPoint3 m_rightControllerPosition;

		// Controller rotation (up vectors)
		NiPoint3 m_leftControllerUpVector;
		NiPoint3 m_rightControllerUpVector;

		// Near face state
		bool m_leftNearFace;
		bool m_rightNearFace;
		bool m_prevLeftNearFace;
		bool m_prevRightNearFace;

		// Controllers touching state
		bool m_controllersTouching;
		bool m_prevControllersTouching;
		bool m_controllersNearForPipeFilling;  // Separate check with pipe filling radius
		bool m_controllersNearForSmokeRolling;  // Separate check with smoke rolling radius
		bool m_controllersNearForPipeLighting;  // Separate check with pipe lighting radius
		bool m_controllersNearForRolledSmokeLighting;  // Separate check with rolled smoke lighting radius
		std::chrono::steady_clock::time_point m_controllersTouchStartTime;

		// Fire spell equipped state
		bool m_fireSpellLeftHand;
		bool m_fireSpellRightHand;
		bool m_prevFireSpellLeftHand;
		bool m_prevFireSpellRightHand;

		// Smoke item equipped hand tracking
		bool m_smokeItemInLeftHand;
		bool m_smokeItemInRightHand;

		// Herb pipe equipped hand tracking (for rotation detection)
		bool m_herbPipeInLeftHand;
		bool m_herbPipeInRightHand;

		// Unlit rolled smoke equipped tracking
		bool m_unlitRolledSmokeInLeftHand;
		bool m_unlitRolledSmokeInRightHand;

		// Lit item equipped tracking (for smoking mechanics)
		bool m_litItemInLeftHand;
		bool m_litItemInRightHand;

		// Lighting condition state (fire spell + lightable item + hands touching)
		bool m_lightingConditionMet;
		bool m_prevLightingConditionMet;
		bool m_lightingTriggered; // Prevents multiple triggers
		bool m_burningSoundStarted; // Track if burning sound has started
		std::chrono::steady_clock::time_point m_lightingConditionStartTime;

		// Herb pipe flipped state (controller facing down)
		bool m_herbPipeHandFlipped;
		bool m_prevHerbPipeHandFlipped;
		bool m_herbPipeFlippedLongEnough;
		bool m_herbPipeEmptiedTriggered; // Prevents multiple triggers while flipped
		std::chrono::steady_clock::time_point m_herbPipeFlippedStartTime;

		// Lit pipe flipped state (controller facing down - for dumping contents)
		bool m_litPipeHandFlipped;
		bool m_prevLitPipeHandFlipped;
		bool m_litPipeFlippedLongEnough;
		bool m_litPipeEmptiedTriggered; // Prevents multiple triggers while flipped
		std::chrono::steady_clock::time_point m_litPipeFlippedStartTime;

		// Track if smoke item hand was near face (for near clip adjustment)
		bool m_smokeItemHandNearFace;
		bool m_prevSmokeItemHandNearFace;

		// Delayed restoration of near clip distance
		bool m_pendingNearClipRestore;
		std::chrono::steady_clock::time_point m_nearClipRestoreTime;

		// Node name constants
		static const char* kLeftHandName;
		static const char* kRightHandName;
		static const char* kHMDNodeName;

		// Helper to find a node by name in the player's skeleton
		NiAVObject* FindNodeByName(NiAVObject* root, const char* name);
		NiAVObject* GetPlayerHandNode(bool rightHand);
		NiAVObject* GetPlayerHMDNode();

		// Helper to calculate distance between two points
		float CalculateDistance(const NiPoint3& a, const NiPoint3& b) const;

		// Update near face detection
		void UpdateNearFaceDetection();

		// Update controllers touching detection
		void UpdateControllersTouchingDetection();

		// Update fire spell detection
		void UpdateFireSpellDetection();

		// Update near clip distance based on smoke item hand near face
		void UpdateNearClipForSmokeItem();

		// Update herb pipe rotation detection (flipped = facing down)
		void UpdateHerbPipeRotationDetection();

		// Update lit pipe rotation detection (flipped = facing down for dumping)
		void UpdateLitPipeRotationDetection();

		// Update lighting condition detection
		void UpdateLightingConditionDetection();

		// Handle herb pipe emptying (called when flipped long enough)
		void HandleHerbPipeEmptied();

		// Handle lit pipe emptying (called when flipped long enough)
		void HandleLitPipeEmptied();

		// Update hand swap detection (smokable in one hand, other empty, hands touching)
		void UpdateHandSwapDetection();

		// Update grabbed item near smokable hand detection
		void UpdateGrabbedItemNearSmokableHandDetection();

		// Track hand swap state
		bool m_handSwapHapticTriggered;
		bool m_handSwapSecondHapticTriggered;
		bool m_handSwapConditionMet;
		std::chrono::steady_clock::time_point m_handSwapStartTime;

		// Track grabbed item near smokable hand state
		bool m_grabbedItemNearSmokableHand;
		bool m_prevGrabbedItemNearSmokableHand;
	};

	// ============================================
	// Global Tracker Instance
	// ============================================
	extern VRInputTracker* g_vrInputTracker;

	// Initialize/Shutdown the global tracker
	void InitializeVRInputTracker();
	void ShutdownVRInputTracker();
}
