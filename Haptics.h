#pragma once

#include <vector>
#include <mutex>
#include <thread>
#include <atomic>

#include "skse64/GameVR.h"

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Haptic Event Structure
	// ============================================
	struct HapticEvent
	{
		float startStrength;
		float endStrength;
		double duration;
		double startTime;
		bool isNew;
	};

	// ============================================
	// Haptics Manager - handles haptic feedback for one hand
	// ============================================
	class HapticsManager
	{
	public:
		HapticsManager(BSVRInterface::BSControllerHand hand);
		~HapticsManager();

		// Queue a haptic event with start/end strength and duration
		void QueueHapticEvent(float startStrength, float endStrength, float duration);

		// Queue a simple haptic pulse with given strength (duration = 2 frames)
		void QueueHapticPulse(float strength);

		// Stop the haptics thread
		void Shutdown();

	private:
		void TriggerHapticPulse(float duration);
		void Loop();

		BSVRInterface::BSControllerHand m_hand;
		std::vector<HapticEvent> m_events;
		std::mutex m_eventsLock;
		std::thread m_thread;
		std::atomic<bool> m_running;
	};

	// ============================================
	// Global Haptics Managers (one per hand)
	// ============================================
	extern HapticsManager* g_hapticsLeft;
	extern HapticsManager* g_hapticsRight;

	// ============================================
	// Helper Functions
	// ============================================

	// Initialize haptics managers for both hands
	void InitializeHaptics();

	// Shutdown haptics managers
	void ShutdownHaptics();

	// Trigger a haptic pulse on specified hand(s)
	// strength: 0.0 to 1.0 (intensity)
	// duration: in seconds
	void TriggerHapticFeedback(bool leftHand, bool rightHand, float strength, float duration);

	// Trigger a short haptic pulse on specified hand(s)
	// strength: 0.0 to 1.0 (intensity)
	void TriggerHapticPulse(bool leftHand, bool rightHand, float strength);

	// Trigger haptic feedback on both hands
	void TriggerHapticFeedbackBothHands(float strength, float duration);

	// Trigger a short pulse on both hands
	void TriggerHapticPulseBothHands(float strength);
}
