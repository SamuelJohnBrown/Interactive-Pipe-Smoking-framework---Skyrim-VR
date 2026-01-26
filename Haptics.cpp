#include "Haptics.h"
#include "config.h"

#include <chrono>
#include <algorithm>

// Prevent Windows min/max macros from conflicting with std::min/max
#undef min
#undef max

namespace InteractivePipeSmokingVR
{
	// ============================================
	// Global Haptics Managers
	// ============================================
	HapticsManager* g_hapticsLeft = nullptr;
	HapticsManager* g_hapticsRight = nullptr;

	// ============================================
	// Helper to get current time in seconds
	// ============================================
	static double GetTime()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto now = std::chrono::high_resolution_clock::now();
		return std::chrono::duration<double>(now - startTime).count();
	}

	// ============================================
	// Simple lerp helper
	// ============================================
	static float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	// ============================================
	// HapticsManager Implementation
	// ============================================

	HapticsManager::HapticsManager(BSVRInterface::BSControllerHand hand)
		: m_hand(hand)
		, m_running(true)
		, m_thread(&HapticsManager::Loop, this)
	{
		_MESSAGE("[Haptics] Created HapticsManager for %s hand", 
			hand == BSVRInterface::kControllerHand_Left ? "LEFT" : "RIGHT");
	}

	HapticsManager::~HapticsManager()
	{
		Shutdown();
	}

	void HapticsManager::Shutdown()
	{
		if (m_running)
		{
			m_running = false;
			if (m_thread.joinable())
			{
				m_thread.join();
			}
			_MESSAGE("[Haptics] Shutdown HapticsManager for %s hand",
				m_hand == BSVRInterface::kControllerHand_Left ? "LEFT" : "RIGHT");
		}
	}

	void HapticsManager::TriggerHapticPulse(float duration)
	{
		if (g_openVR && *g_openVR && duration > 0)
		{
			BSOpenVR* openVR = *g_openVR;
			openVR->TriggerHapticPulse(m_hand, duration);
		}
	}

	void HapticsManager::QueueHapticEvent(float startStrength, float endStrength, float duration)
	{
		HapticEvent hapticEvent;
		hapticEvent.startStrength = startStrength;
		hapticEvent.endStrength = endStrength;
		hapticEvent.duration = duration;
		hapticEvent.isNew = true;
		hapticEvent.startTime = 0;

		{
			std::scoped_lock lock(m_eventsLock);
			m_events.push_back(std::move(hapticEvent));
		}
	}

	void HapticsManager::QueueHapticPulse(float strength)
	{
		// Queue a short pulse (about 2 frames at 90fps = ~22ms)
		QueueHapticEvent(strength, strength, 0.022f);
	}

	void HapticsManager::Loop()
	{
		while (m_running)
		{
			{
				std::scoped_lock lock(m_eventsLock);

				size_t numEvents = m_events.size();
				if (numEvents > 0)
				{
					double currentTime = GetTime();

					// Play the last event that was added
					HapticEvent& lastEvent = m_events[numEvents - 1];

					if (lastEvent.isNew)
					{
						lastEvent.isNew = false;
						lastEvent.startTime = currentTime;
					}

					float strength;
					if (lastEvent.duration == 0)
					{
						strength = lastEvent.startStrength;
					}
					else
					{
						// Lerp from start to end strength over duration
						double elapsedTime = currentTime - lastEvent.startTime;
						float t = static_cast<float>(std::min(1.0, elapsedTime / lastEvent.duration));
						strength = Lerp(lastEvent.startStrength, lastEvent.endStrength, t);
					}

					TriggerHapticPulse(strength);

					// Cleanup events that are past their duration
					auto end = std::remove_if(m_events.begin(), m_events.end(),
						[currentTime](HapticEvent& evnt) { return currentTime - evnt.startTime >= evnt.duration; }
					);
					m_events.erase(end, m_events.end());
				}
			}

			// TriggerHapticPulse can only be called once every 5ms
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}

	// ============================================
	// Global Helper Functions
	// ============================================

	void InitializeHaptics()
	{
		if (g_hapticsLeft == nullptr)
		{
			g_hapticsLeft = new HapticsManager(BSVRInterface::kControllerHand_Left);
		}
		if (g_hapticsRight == nullptr)
		{
			g_hapticsRight = new HapticsManager(BSVRInterface::kControllerHand_Right);
		}
		_MESSAGE("[Haptics] Initialized haptics managers for both hands");
	}

	void ShutdownHaptics()
	{
		if (g_hapticsLeft != nullptr)
		{
			delete g_hapticsLeft;
			g_hapticsLeft = nullptr;
		}
		if (g_hapticsRight != nullptr)
		{
			delete g_hapticsRight;
			g_hapticsRight = nullptr;
		}
		_MESSAGE("[Haptics] Shutdown haptics managers");
	}

	void TriggerHapticFeedback(bool leftHand, bool rightHand, float strength, float duration)
	{
		if (leftHand && g_hapticsLeft)
		{
			g_hapticsLeft->QueueHapticEvent(strength, strength, duration);
		}
		if (rightHand && g_hapticsRight)
		{
			g_hapticsRight->QueueHapticEvent(strength, strength, duration);
		}
	}

	void TriggerHapticPulse(bool leftHand, bool rightHand, float strength)
	{
		if (leftHand && g_hapticsLeft)
		{
			g_hapticsLeft->QueueHapticPulse(strength);
		}
		if (rightHand && g_hapticsRight)
		{
			g_hapticsRight->QueueHapticPulse(strength);
		}
	}

	void TriggerHapticFeedbackBothHands(float strength, float duration)
	{
		TriggerHapticFeedback(true, true, strength, duration);
	}

	void TriggerHapticPulseBothHands(float strength)
	{
		TriggerHapticPulse(true, true, strength);
	}
}
