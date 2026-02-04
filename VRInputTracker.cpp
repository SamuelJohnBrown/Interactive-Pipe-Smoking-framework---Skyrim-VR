#include "VRInputTracker.h"
#include "Engine.h"
#include "EquipState.h"
#include "Haptics.h"
#include "SmokingMechanics.h"
#include "config.h"
#include "skse64/GameReferences.h"
#include "skse64/GameObjects.h"
#include "skse64/NiNodes.h"
#include "skse64/PluginAPI.h"
#include <cmath>
#include <chrono>

namespace InteractivePipeSmokingVR
{
	// External task interface from main.cpp
	extern SKSETaskInterface* g_task;

	// ============================================
	// Global Tracker Instance
	// ============================================
	VRInputTracker* g_vrInputTracker = nullptr;

	// ============================================
	// Node name constants
	// ============================================
	const char* VRInputTracker::kLeftHandName = "NPC L Hand [LHnd]";
	const char* VRInputTracker::kRightHandName = "NPC R Hand [RHnd]";
	const char* VRInputTracker::kHMDNodeName = "NPC Head [Head]";

	// ============================================
	// Spell Keyword Helper
	// ============================================
	static bool SpellHasKeyword(SpellItem* spell, const char* keywordEditorId)
	{
		if (!spell || !keywordEditorId)
			return false;

		// Iterate through spell effects
		for (UInt32 i = 0; i < spell->effectItemList.count; ++i)
		{
			MagicItem::EffectItem* effectItem = nullptr;
			spell->effectItemList.GetNthItem(i, effectItem);
			
			if (!effectItem || !effectItem->mgef)
				continue;

			EffectSetting* effect = effectItem->mgef;
			
			// Check keywords on the effect
			if (effect->keywordForm.numKeywords > 0 && effect->keywordForm.keywords)
			{
				for (UInt32 k = 0; k < effect->keywordForm.numKeywords; ++k)
				{
					BGSKeyword* keyword = effect->keywordForm.keywords[k];
					if (keyword && keyword->keyword.data)
					{
						if (strstr(keyword->keyword.data, keywordEditorId) != nullptr)
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	// ============================================
	// Periodic Update Task - runs once, updates, then schedules next update after delay
	// ============================================
	class VRInputTrackerUpdateTask : public TaskDelegate
	{
	public:
		virtual void Run() override
		{
			if (g_vrInputTracker && g_vrInputTracker->IsTracking())
			{
				g_vrInputTracker->Update();
				
				// Schedule next update after a short delay (don't immediately re-queue)
				g_vrInputTracker->ScheduleNextUpdate();
			}
		}

		virtual void Dispose() override
		{
			delete this;
		}
	};

	// ============================================
	// VR Input Tracker Implementation
	// ============================================

	VRInputTracker::VRInputTracker()
		: m_isTracking(false)
		, m_isPaused(false)
		, m_isInitialized(false)
		, m_hmdPosition(0, 0, 0)
		, m_faceTargetPosition(0, 0, 0)
		, m_leftControllerPosition(0, 0, 0)
		, m_rightControllerPosition(0, 0, 0)
		, m_leftControllerUpVector(0, 0, 1)
		, m_rightControllerUpVector(0, 0, 1)
		, m_leftNearFace(false)
		, m_rightNearFace(false)
		, m_prevLeftNearFace(false)
		, m_prevRightNearFace(false)
		, m_controllersTouching(false)
		, m_prevControllersTouching(false)
		, m_fireSpellLeftHand(false)
		, m_fireSpellRightHand(false)
		, m_prevFireSpellLeftHand(false)
		, m_prevFireSpellRightHand(false)
		, m_smokeItemInLeftHand(false)
		, m_smokeItemInRightHand(false)
		, m_herbPipeInLeftHand(false)
		, m_herbPipeInRightHand(false)
		, m_unlitRolledSmokeInLeftHand(false)
		, m_unlitRolledSmokeInRightHand(false)
		, m_litItemInLeftHand(false)
		, m_litItemInRightHand(false)
		, m_lightingConditionMet(false)
		, m_prevLightingConditionMet(false)
		, m_lightingTriggered(false)
		, m_burningSoundStarted(false)
		, m_herbPipeHandFlipped(false)
		, m_prevHerbPipeHandFlipped(false)
		, m_herbPipeFlippedLongEnough(false)
		, m_herbPipeEmptiedTriggered(false)
		, m_smokeItemHandNearFace(false)
		, m_prevSmokeItemHandNearFace(false)
		, m_pendingNearClipRestore(false)
		, m_updatePending(false)
		, m_handSwapHapticTriggered(false)
		, m_handSwapSecondHapticTriggered(false)
		, m_handSwapConditionMet(false)
		, m_grabbedItemNearSmokableHand(false)
		, m_prevGrabbedItemNearSmokableHand(false)
	{
	}

	VRInputTracker::~VRInputTracker()
	{
		Shutdown();
	}

	void VRInputTracker::Initialize()
	{
		if (m_isInitialized)
			return;

		m_isInitialized = true;
		_MESSAGE("[VRInputTracker] Initialized");
		_MESSAGE("[VRInputTracker] Face zone offset: X=%.2f Y=%.2f Z=%.2f Radius=%.2f",
			configFaceZoneOffsetX, configFaceZoneOffsetY, configFaceZoneOffsetZ, configFaceZoneRadius);
		_MESSAGE("[VRInputTracker] Controller touch radius: %.2f", configControllerTouchRadius);
	}

	void VRInputTracker::Shutdown()
	{
		if (!m_isInitialized)
			return;

		StopTracking();
		m_isInitialized = false;
		_MESSAGE("[VRInputTracker] Shutdown");
	}

	void VRInputTracker::StartTracking()
	{
		if (m_isTracking)
			return;

		m_isTracking = true;
		m_isInitialized = true;  // Ensure initialized flag is set when starting
		m_isPaused = false;
		m_updatePending = false;
		m_leftNearFace = false;
		m_rightNearFace = false;
		m_prevLeftNearFace = false;
		m_prevRightNearFace = false;
		m_controllersTouching = false;
		m_prevControllersTouching = false;
		m_controllersNearForPipeFilling = false;
		m_controllersNearForSmokeRolling = false;
		m_controllersNearForPipeLighting = false;
		m_controllersNearForRolledSmokeLighting = false;
		m_fireSpellLeftHand = false;
		m_fireSpellRightHand = false;
		m_prevFireSpellLeftHand = false;
		m_prevFireSpellRightHand = false;
		m_lightingConditionMet = false;
		m_prevLightingConditionMet = false;
		m_lightingTriggered = false;
		m_burningSoundStarted = false;
		m_litItemInLeftHand = false;
		m_litItemInRightHand = false;
		m_herbPipeHandFlipped = false;
		m_prevHerbPipeHandFlipped = false;
		m_herbPipeFlippedLongEnough = false;
		m_herbPipeEmptiedTriggered = false;
		m_litPipeHandFlipped = false;
		m_prevLitPipeHandFlipped = false;
		m_litPipeFlippedLongEnough = false;
		m_litPipeEmptiedTriggered = false;
		m_smokeItemHandNearFace = false;
		m_prevSmokeItemHandNearFace = false;
		m_pendingNearClipRestore = false;
		m_handSwapHapticTriggered = false;
		m_handSwapSecondHapticTriggered = false;
		m_handSwapConditionMet = false;
		m_grabbedItemNearSmokableHand = false;
		m_prevGrabbedItemNearSmokableHand = false;
		_MESSAGE("[VRInputTracker] Started tracking");

		// Queue the first update
		ScheduleNextUpdate();
	}

	void VRInputTracker::StopTracking()
	{
		if (!m_isTracking)
			return;

		m_isTracking = false;
		m_isPaused = false;
		m_updatePending = false;
		_MESSAGE("[VRInputTracker] Stopped tracking");
	}

	void VRInputTracker::PauseTracking()
	{
		if (!m_isTracking || m_isPaused)
			return;

		m_isPaused = true;
		_MESSAGE("[VRInputTracker] Paused tracking (menu open)");
	}

	void VRInputTracker::ResumeTracking()
	{
		if (!m_isTracking || !m_isPaused)
			return;

		m_isPaused = false;
		_MESSAGE("[VRInputTracker] Resumed tracking (menu closed)");

		// Schedule an update to resume the tracking loop
		ScheduleNextUpdate();
	}

	void VRInputTracker::ScheduleNextUpdate()
	{
		// Prevent multiple pending updates
		if (m_updatePending || !m_isTracking || m_isPaused)
			return;

		m_updatePending = true;

		// Use a thread to delay then queue the task (avoids infinite task loop)
		std::thread([this]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 updates per second
			
			if (m_isTracking && !m_isPaused && g_task)
			{
				m_updatePending = false;
				g_task->AddTask(new VRInputTrackerUpdateTask());
			}
			else
			{
				m_updatePending = false;
			}
		}).detach();
	}

	NiAVObject* VRInputTracker::FindNodeByName(NiAVObject* root, const char* name)
	{
		if (!root || !name)
			return nullptr;

		// Compare exact name
		if (root->m_name && _stricmp(root->m_name, name) == 0)
			return root;

		NiNode* node = root->GetAsNiNode();
		if (!node)
			return nullptr;

		for (UInt32 i = 0; i < node->m_children.m_size; ++i)
		{
			NiAVObject* child = node->m_children.m_data[i];
			if (!child)
				continue;
			if (NiAVObject* found = FindNodeByName(child, name))
				return found;
		}

		return nullptr;
	}

	NiAVObject* VRInputTracker::GetPlayerHandNode(bool rightHand)
	{
		if (!g_thePlayer || !(*g_thePlayer))
			return nullptr;

		TESObjectREFR* playerRefr = *g_thePlayer;
		NiNode* root = playerRefr->GetNiNode();
		if (!root)
			return nullptr;

		const char* nodeName = rightHand ? kRightHandName : kLeftHandName;
		return FindNodeByName(root, nodeName);
	}

	NiAVObject* VRInputTracker::GetPlayerHMDNode()
	{
		if (!g_thePlayer || !(*g_thePlayer))
			return nullptr;

		TESObjectREFR* playerRefr = *g_thePlayer;
		NiNode* root = playerRefr->GetNiNode();
		if (!root)
			return nullptr;

		return FindNodeByName(root, kHMDNodeName);
	}

	void VRInputTracker::Update()
	{
		if (!m_isTracking || !m_isInitialized || m_isPaused)
			return;

		// NOTE: UpdateHeldSmokableScale is called from PostVrikPostHiggsCallback instead
		// to ensure our scale is applied AFTER HIGGS processes the held object

		// Get hand and head nodes
		NiAVObject* leftHand = GetPlayerHandNode(false);
		NiAVObject* rightHand = GetPlayerHandNode(true);
		NiAVObject* hmdNode = GetPlayerHMDNode();

		// Update HMD position and calculate face target position with offset
		if (hmdNode)
		{
			m_hmdPosition = hmdNode->m_worldTransform.pos;

			// Get the head's rotation matrix to apply offsets in local space
			NiMatrix33& headRot = hmdNode->m_worldTransform.rot;

			// Calculate offset in world space using head's local coordinate system
			// Local X = right, Local Y = forward (facing direction), Local Z = up
			NiPoint3 localOffset;
			localOffset.x = configFaceZoneOffsetX;
			localOffset.y = configFaceZoneOffsetY;
			localOffset.z = configFaceZoneOffsetZ;

			// Transform local offset to world space using head rotation
			NiPoint3 worldOffset;
			worldOffset.x = headRot.data[0][0] * localOffset.x + headRot.data[0][1] * localOffset.y + headRot.data[0][2] * localOffset.z;
			worldOffset.y = headRot.data[1][0] * localOffset.x + headRot.data[1][1] * localOffset.y + headRot.data[1][2] * localOffset.z;
			worldOffset.z = headRot.data[2][0] * localOffset.x + headRot.data[2][1] * localOffset.y + headRot.data[2][2] * localOffset.z;

			// Face target is HMD position plus the rotated offset
			m_faceTargetPosition.x = m_hmdPosition.x + worldOffset.x;
			m_faceTargetPosition.y = m_hmdPosition.y + worldOffset.y;
			m_faceTargetPosition.z = m_hmdPosition.z + worldOffset.z;
		}

		// Update left controller position and rotation
		if (leftHand)
		{
			m_leftControllerPosition = leftHand->m_worldTransform.pos;
			
			// Extract the "up" vector from the rotation matrix (Z column in local space)
			// This tells us which way the controller's "up" is pointing in world space
			NiMatrix33& leftRot = leftHand->m_worldTransform.rot;
			m_leftControllerUpVector.x = leftRot.data[0][2];
			m_leftControllerUpVector.y = leftRot.data[1][2];
			m_leftControllerUpVector.z = leftRot.data[2][2];
		}

		// Update right controller position and rotation
		if (rightHand)
		{
			m_rightControllerPosition = rightHand->m_worldTransform.pos;
			
			// Extract the "up" vector from the rotation matrix (Z column in local space)
			// This tells us which way the controller's "up" is pointing in world space
			NiMatrix33& rightRot = rightHand->m_worldTransform.rot;
			m_rightControllerUpVector.x = rightRot.data[0][2];
			m_rightControllerUpVector.y = rightRot.data[1][2];
			m_rightControllerUpVector.z = rightRot.data[2][2];
		}

		// Update all detections
		UpdateNearFaceDetection();
		UpdateControllersTouchingDetection();
		UpdateFireSpellDetection();
		UpdateNearClipForSmokeItem();
		UpdateHerbPipeRotationDetection();
		UpdateLitPipeRotationDetection();
		UpdateLightingConditionDetection();
		UpdateHandSwapDetection();
		UpdateGrabbedItemNearSmokableHandDetection();

		// Update smoking mechanics (inhale detection for lit items)
		if (m_litItemInLeftHand || m_litItemInRightHand)
		{
			// Check if the lit item hand is near face
			bool litItemNearFace = false;
			if (m_litItemInLeftHand && m_leftNearFace)
				litItemNearFace = true;
			if (m_litItemInRightHand && m_rightNearFace)
				litItemNearFace = true;

			UpdateSmokingMechanics(litItemNearFace);
		}
	}

	float VRInputTracker::CalculateDistance(const NiPoint3& a, const NiPoint3& b) const
	{
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float dz = a.z - b.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}

	float VRInputTracker::GetLeftControllerToFaceDistance() const
	{
		return CalculateDistance(m_leftControllerPosition, m_faceTargetPosition);
	}

	float VRInputTracker::GetRightControllerToFaceDistance() const
	{
		return CalculateDistance(m_rightControllerPosition, m_faceTargetPosition);
	}

	float VRInputTracker::GetControllerToControllerDistance() const
	{
		return CalculateDistance(m_leftControllerPosition, m_rightControllerPosition);
	}

	void VRInputTracker::UpdateNearFaceDetection()
	{
		// Store previous state
		m_prevLeftNearFace = m_leftNearFace;
		m_prevRightNearFace = m_rightNearFace;

		// Calculate distances to the face target position (offset from HMD)
		float leftDistance = GetLeftControllerToFaceDistance();
		float rightDistance = GetRightControllerToFaceDistance();

		// Use configurable radius
		m_leftNearFace = (leftDistance <= configFaceZoneRadius);
		m_rightNearFace = (rightDistance <= configFaceZoneRadius);

		// Log state changes for left controller
		if (m_leftNearFace && !m_prevLeftNearFace)
		{
			// Left entered - no log needed
		}
		else if (!m_leftNearFace && m_prevLeftNearFace)
		{
			// Left exited - no log needed
		}

		if (m_rightNearFace && !m_prevRightNearFace)
		{
			// Right entered - no log needed
		}
		else if (!m_rightNearFace && m_prevRightNearFace)
		{
			// Right exited - no log needed
		}
	}

	void VRInputTracker::UpdateControllersTouchingDetection()
	{
		// Store previous state
		m_prevControllersTouching = m_controllersTouching;

		// Calculate distance between controllers
		float controllerDistance = GetControllerToControllerDistance();

		// Use larger radius for rolled smoke lighting with fire spell
		// This only affects the lighting detection - other actions (pipe filling, smoke rolling) use normal radius
		float effectiveRadius = configControllerTouchRadius;
		
		bool hasUnlitRolledSmoke = m_unlitRolledSmokeInLeftHand || m_unlitRolledSmokeInRightHand;
		bool hasFireSpell = m_fireSpellLeftHand || m_fireSpellRightHand;
		
		if (hasUnlitRolledSmoke && hasFireSpell)
		{
			// Use larger radius for rolled smoke lighting
			effectiveRadius = configRolledSmokeLightingRadius;
		}

		m_controllersTouching = (controllerDistance <= effectiveRadius);

		// Separate check for pipe filling with its own radius
		m_controllersNearForPipeFilling = (controllerDistance <= configPipeFillingRadius);

		// Separate check for smoke rolling with its own radius
		m_controllersNearForSmokeRolling = (controllerDistance <= configSmokeRollingRadius);

		// Separate check for pipe lighting with its own radius
		m_controllersNearForPipeLighting = (controllerDistance <= configPipeLightingRadius);

		// Separate check for rolled smoke lighting with its own (larger) radius
		m_controllersNearForRolledSmokeLighting = (controllerDistance <= configRolledSmokeLightingRadius);

		// Log when controllers start/stop touching
		if (m_controllersTouching && !m_prevControllersTouching)
		{
			m_controllersTouchStartTime = std::chrono::steady_clock::now();
		}
		else if (!m_controllersTouching && m_prevControllersTouching)
		{
			// Reset the global flag when controllers stop touching
		 g_controllersTouchingLongEnough = false;
		}
		// Update global flag for controllers touching long enough
		if (m_controllersTouching)
		{
			int touchDurationMs = GetControllersTouchingDurationMs();
			if (touchDurationMs >= configControllerTouchDurationMs && !g_controllersTouchingLongEnough)
			{
				g_controllersTouchingLongEnough = true;
			}
		}
	}

	int VRInputTracker::GetControllersTouchingDurationMs() const
	{
		if (!m_controllersTouching)
			return 0;

		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_controllersTouchStartTime);
		return static_cast<int>(duration.count());
	}

	void VRInputTracker::UpdateFireSpellDetection()
	{
		m_prevFireSpellLeftHand = m_fireSpellLeftHand;
		m_prevFireSpellRightHand = m_fireSpellRightHand;

		Actor* player = *g_thePlayer;
		if (!player)
		{
			m_fireSpellLeftHand = false;
			m_fireSpellRightHand = false;
			return;
		}

		// Check left hand spell
		SpellItem* leftSpell = player->leftHandSpell;
		m_fireSpellLeftHand = (leftSpell != nullptr) && SpellHasKeyword(leftSpell, "MagicDamageFire");

		// Check right hand spell
		SpellItem* rightSpell = player->rightHandSpell;
		m_fireSpellRightHand = (rightSpell != nullptr) && SpellHasKeyword(rightSpell, "MagicDamageFire");

		// Log when fire spell state changes
		if (m_fireSpellLeftHand && !m_prevFireSpellLeftHand)
		{
			const char* spellName = leftSpell ? leftSpell->fullName.name.data : "Unknown";
			_MESSAGE("[VRInputTracker] FIRE spell EQUIPPED in LEFT hand: %s (FormID: %08X)", spellName, leftSpell ? leftSpell->formID : 0);
		}
		else if (!m_fireSpellLeftHand && m_prevFireSpellLeftHand)
		{
			_MESSAGE("[VRInputTracker] FIRE spell UNEQUIPPED from LEFT hand");
		}

		if (m_fireSpellRightHand && !m_prevFireSpellRightHand)
		{
			const char* spellName = rightSpell ? rightSpell->fullName.name.data : "Unknown";
			_MESSAGE("[VRInputTracker] FIRE spell EQUIPPED in RIGHT hand: %s (FormID: %08X)", spellName, rightSpell ? rightSpell->formID : 0);
		}
		else if (!m_fireSpellRightHand && m_prevFireSpellRightHand)
		{
			_MESSAGE("[VRInputTracker] FIRE spell UNEQUIPPED from RIGHT hand");
		}
	}

	void VRInputTracker::SetSmokeItemEquippedHand(bool leftHand, bool rightHand)
	{
		// Check if we're unequipping all smoke items
		bool wasEquipped = m_smokeItemInLeftHand || m_smokeItemInRightHand;
		bool nowEquipped = leftHand || rightHand;

		m_smokeItemInLeftHand = leftHand;
		m_smokeItemInRightHand = rightHand;
		_MESSAGE("[VRInputTracker] Smoke item equipped - Left: %d, Right: %d", leftHand ? 1 : 0, rightHand ? 1 : 0);

		// If smoke item was unequipped, force restore near clip distance
		if (wasEquipped && !nowEquipped)
		{
			ForceRestoreNearClipDistance();
		}
	}

	void VRInputTracker::SetHerbPipeEquippedHand(bool leftHand, bool rightHand)
	{
		m_herbPipeInLeftHand = leftHand;
		m_herbPipeInRightHand = rightHand;
		
		// Reset flipped state when herb pipe equip state changes
		m_herbPipeHandFlipped = false;
		m_prevHerbPipeHandFlipped = false;
		m_herbPipeFlippedLongEnough = false;
		m_herbPipeEmptiedTriggered = false;
		
		_MESSAGE("[VRInputTracker] Herb pipe equipped - Left: %d, Right: %d", leftHand ? 1 : 0, rightHand ? 1 : 0);
	}

	void VRInputTracker::SetUnlitRolledSmokeEquippedHand(bool leftHand, bool rightHand)
	{
		m_unlitRolledSmokeInLeftHand = leftHand;
		m_unlitRolledSmokeInRightHand = rightHand;
		
		_MESSAGE("[VRInputTracker] Unlit rolled smoke equipped - Left: %d, Right: %d", leftHand ? 1 : 0, rightHand ? 1 : 0);
	}

	void VRInputTracker::SetLitItemEquippedHand(bool leftHand, bool rightHand)
	{
		bool wasLitItemEquipped = m_litItemInLeftHand || m_litItemInRightHand;
		bool isLitItemEquipped = leftHand || rightHand;

		m_litItemInLeftHand = leftHand;
		m_litItemInRightHand = rightHand;
		
		// Reset flipped state when lit item equip state changes
		m_litPipeHandFlipped = false;
		m_prevLitPipeHandFlipped = false;
		m_litPipeFlippedLongEnough = false;
		m_litPipeEmptiedTriggered = false;
		
		_MESSAGE("[VRInputTracker] Lit item equipped - Left: %d, Right: %d", leftHand ? 1 : 0, rightHand ? 1 : 0);

		// If lit item was just unequipped, reset smoking mechanics
		if (wasLitItemEquipped && !isLitItemEquipped)
		{
			ResetSmokingMechanics();
		}
	}

	int VRInputTracker::GetLightingConditionDurationMs() const
	{
		if (!m_lightingConditionMet)
			return 0;

		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lightingConditionStartTime);
		return static_cast<int>(duration.count());
	}

	void VRInputTracker::ForceRestoreNearClipDistance()
	{
		// Cancel any pending restore timer
		if (m_pendingNearClipRestore)
		{
			_MESSAGE("[VRInputTracker] Cancelling pending near clip restore (smoke item unequipped)");
			m_pendingNearClipRestore = false;
		}

		// Force restore near clip distance to original value
		if (vrikInterface && g_vrikNearClipDistanceCached)
		{
			vrikInterface->setSettingDouble("nearClipDistance", static_cast<double>(g_originalVrikNearClipDistance));
			_MESSAGE("[VRInputTracker] Force restored nearClipDistance to: %.1f (smoke item unequipped)", g_originalVrikNearClipDistance);
		}

		// Reset near face tracking state
		m_smokeItemHandNearFace = false;
		m_prevSmokeItemHandNearFace = false;
	}

	int VRInputTracker::GetHerbPipeFlippedDurationMs() const
	{
		if (!m_herbPipeHandFlipped)
			return 0;

		auto now = std::chrono::steady_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_herbPipeFlippedStartTime);
		return static_cast<int>(duration.count());
	}

	bool VRInputTracker::IsSmokeItemHandNearFace() const
	{
		return m_smokeItemHandNearFace;
	}

	void VRInputTracker::UpdateNearClipForSmokeItem()
	{
		// Store previous state
		m_prevSmokeItemHandNearFace = m_smokeItemHandNearFace;

		// Check if the hand with the smoke item is near face
		m_smokeItemHandNearFace = false;
		if (m_smokeItemInLeftHand && m_leftNearFace)
		{
			m_smokeItemHandNearFace = true;
		}
		if (m_smokeItemInRightHand && m_rightNearFace)
		{
			m_smokeItemHandNearFace = true;
		}

		// Handle entering face zone
		if (m_smokeItemHandNearFace && !m_prevSmokeItemHandNearFace)
		{
			// Smoke item hand ENTERED face zone - set near clip to 5 immediately
			// Also cancel any pending restore
			m_pendingNearClipRestore = false;
			if (vrikInterface)
			{
				vrikInterface->setSettingDouble("nearClipDistance", static_cast<double>(NEAR_CLIP_SMOKING_NEAR_FACE));
			}
		}
		// Handle exiting face zone - start the timer
		else if (!m_smokeItemHandNearFace && m_prevSmokeItemHandNearFace && !m_pendingNearClipRestore)
		{
			// Smoke item hand EXITED face zone - start delayed restore
			m_pendingNearClipRestore = true;
			m_nearClipRestoreTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(configNearClipRestoreDelayMs);
		}

		// If hand re-enters face zone while pending restore, cancel the restore
		if (m_smokeItemHandNearFace && m_pendingNearClipRestore)
		{
			m_pendingNearClipRestore = false;
		}

		// Check if pending restore timer has elapsed (only if still outside face zone)
		if (m_pendingNearClipRestore && !m_smokeItemHandNearFace)
		{
			auto now = std::chrono::steady_clock::now();
			if (now >= m_nearClipRestoreTime)
			{
				m_pendingNearClipRestore = false;
				if (vrikInterface)
				{
					vrikInterface->setSettingDouble("nearClipDistance", static_cast<double>(g_originalVrikNearClipDistance));
				}
			}
		}
	}

	void VRInputTracker::UpdateHerbPipeRotationDetection()
	{
		// Only track if a herb pipe is equipped
		if (!m_herbPipeInLeftHand && !m_herbPipeInRightHand)
		{
			m_herbPipeHandFlipped = false;
			m_prevHerbPipeHandFlipped = false;
			m_herbPipeFlippedLongEnough = false;
			m_herbPipeEmptiedTriggered = false;
			g_herbPipeFlippedLongEnough = false;
			return;
		}

		// Store previous state
		m_prevHerbPipeHandFlipped = m_herbPipeHandFlipped;

		// Get the up vector of the hand holding the herb pipe
		NiPoint3 upVector;
		if (m_herbPipeInLeftHand)
		{
			upVector = m_leftControllerUpVector;
		}
		else
		{
			upVector = m_rightControllerUpVector;
		}

		// The controller is "flipped" when its up vector points downward (negative Z in world space)
		// When holding normally, the controller's local Z (up) points roughly upward (positive world Z)
		// When flipped 180 degrees, the controller's local Z points downward (negative world Z)
		// 
		// We check if the Z component of the up vector is negative (pointing down)
		// A threshold of -0.5 means the controller is tilted more than ~60 degrees from horizontal
		// A threshold of -0.7 means the controller is tilted more than ~45 degrees past horizontal (closer to fully inverted)
		const float flipThreshold = -0.5f;
		m_herbPipeHandFlipped = (upVector.z < flipThreshold);

		// Log when flipped state changes
		if (m_herbPipeHandFlipped && !m_prevHerbPipeHandFlipped)
		{
			m_herbPipeFlippedStartTime = std::chrono::steady_clock::now();
			m_herbPipeEmptiedTriggered = false; // Reset trigger flag when starting a new flip
			_MESSAGE("[VRInputTracker] Herb pipe hand FLIPPED (upVector.z=%.2f, threshold=%.2f) - timer started",
				upVector.z, flipThreshold);
		}
		else if (!m_herbPipeHandFlipped && m_prevHerbPipeHandFlipped)
		{
			_MESSAGE("[VRInputTracker] Herb pipe hand UNFLIPPED (upVector.z=%.2f) - timer reset", upVector.z);
			m_herbPipeFlippedLongEnough = false;
			m_herbPipeEmptiedTriggered = false;
			g_herbPipeFlippedLongEnough = false;
		}

		// Check if flipped long enough (2 seconds = 2000ms)
		const int flippedDurationThresholdMs = 2000;
		if (m_herbPipeHandFlipped)
		{
			int flippedDurationMs = GetHerbPipeFlippedDurationMs();
			bool wasLongEnough = m_herbPipeFlippedLongEnough;
			m_herbPipeFlippedLongEnough = (flippedDurationMs >= flippedDurationThresholdMs);
			g_herbPipeFlippedLongEnough = m_herbPipeFlippedLongEnough;

			// Log when the threshold is crossed and trigger emptying (only once per flip)
			if (m_herbPipeFlippedLongEnough && !wasLongEnough && !m_herbPipeEmptiedTriggered)
			{
				_MESSAGE("[VRInputTracker] Herb pipe hand FLIPPED LONG ENOUGH (%d ms >= %d ms threshold) - EMPTYING PIPE",
					flippedDurationMs, flippedDurationThresholdMs);
				m_herbPipeEmptiedTriggered = true;
				HandleHerbPipeEmptied();
			}
		}
		else
		{
			m_herbPipeFlippedLongEnough = false;
			g_herbPipeFlippedLongEnough = false;
		}
	}

	void VRInputTracker::HandleHerbPipeEmptied()
	{
		_MESSAGE("[PipeEmpty] *** HERB PIPE EMPTIED - Dumping contents ***");
		
		// Determine which pipe type is equipped and clear the correct cache
		Actor* player = *g_thePlayer;
		if (player)
		{
			// Check which herb pipe is equipped to clear the correct cache
			// We check both hands since we don't know which game hand it's in
			TESForm* leftItem = player->GetEquippedObject(true);
			TESForm* rightItem = player->GetEquippedObject(false);
			
			bool isWoodenPipe = false;
			bool isBonePipe = false;
			
			if (leftItem)
			{
				if (IsHerbWoodenPipeWeapon(leftItem->formID))
					isWoodenPipe = true;
				else if (IsHerbBonePipeWeapon(leftItem->formID))
					isBonePipe = true;
			}
			if (rightItem && !isWoodenPipe && !isBonePipe)
			{
				if (IsHerbWoodenPipeWeapon(rightItem->formID))
					isWoodenPipe = true;
				else if (IsHerbBonePipeWeapon(rightItem->formID))
				 isBonePipe = true;
			}
			
			if (isWoodenPipe)
			{
				g_filledWoodenPipeSmokableFormId = 0;
				g_filledWoodenPipeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[PipeEmpty]   -> Cleared WOODEN PIPE smokable cache");
			}
			else if (isBonePipe)
			{
				g_filledBonePipeSmokableFormId = 0;
				g_filledBonePipeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[PipeEmpty]   -> Cleared BONE PIPE smokable cache");
			}
			else
			{
				_MESSAGE("[PipeEmpty]   -> WARNING: Could not determine pipe type to clear cache");
			}
		}

		// Trigger haptic feedback to confirm emptying
		TriggerHapticFeedback(m_herbPipeInLeftHand, m_herbPipeInRightHand, 0.3f, 0.2f);
		_MESSAGE("[PipeEmpty]   -> Haptic feedback triggered on %s hand", m_herbPipeInLeftHand ? "LEFT" : "RIGHT");

		// Convert VR controller hands to game hands for UnequipHerbPipeAndEquipEmpty
		// In left-handed mode: left VR controller = right game hand, right VR controller = left game hand
		bool gameLeftHand, gameRightHand;
		if (IsLeftHandedMode())
		{
			gameLeftHand = m_herbPipeInRightHand;   // Right VR = Left game
			gameRightHand = m_herbPipeInLeftHand;   // Left VR = Right game
			_MESSAGE("[PipeEmpty]   -> Left-handed mode: VR(%s) -> Game(%s)",
				m_herbPipeInLeftHand ? "LEFT" : "RIGHT",
				gameLeftHand ? "LEFT" : "RIGHT");
		}
		else
		{
			gameLeftHand = m_herbPipeInLeftHand;    // Left VR = Left game
			gameRightHand = m_herbPipeInRightHand;  // Right VR = Right game
		}

		// Unequip herb pipe and equip empty pipe (using game hands)
		if (g_equipStateManager)
		{
			g_equipStateManager->UnequipHerbPipeAndEquipEmpty(gameLeftHand, gameRightHand);
		}
	}

	void VRInputTracker::UpdateLitPipeRotationDetection()
	{
		// Only track if a lit pipe is equipped (not rolled smoke - pipes only)
		if (!m_litItemInLeftHand && !m_litItemInRightHand)
		{
			m_litPipeHandFlipped = false;
			m_prevLitPipeHandFlipped = false;
			m_litPipeFlippedLongEnough = false;
			m_litPipeEmptiedTriggered = false;
			return;
		}

		// Store previous state
		m_prevLitPipeHandFlipped = m_litPipeHandFlipped;

		// Get the up vector of the hand holding the lit pipe
		NiPoint3 upVector;
		if (m_litItemInLeftHand)
		{
			upVector = m_leftControllerUpVector;
		}
		else
		{
			upVector = m_rightControllerUpVector;
		}

		// The controller is "flipped" when its up vector points downward (negative Z in world space)
		const float flipThreshold = -0.5f;
		m_litPipeHandFlipped = (upVector.z < flipThreshold);

		// Log when flipped state changes
		if (m_litPipeHandFlipped && !m_prevLitPipeHandFlipped)
		{
			m_litPipeFlippedStartTime = std::chrono::steady_clock::now();
			m_litPipeEmptiedTriggered = false;
			_MESSAGE("[VRInputTracker] Lit pipe hand FLIPPED (upVector.z=%.2f, threshold=%.2f) - timer started",
				upVector.z, flipThreshold);
		}
		else if (!m_litPipeHandFlipped && m_prevLitPipeHandFlipped)
		{
			_MESSAGE("[VRInputTracker] Lit pipe hand UNFLIPPED (upVector.z=%.2f) - timer reset", upVector.z);
			m_litPipeFlippedLongEnough = false;
			m_litPipeEmptiedTriggered = false;
		}

		// Check if flipped long enough (2 seconds = 2000ms)
		const int flippedDurationThresholdMs = 2000;
		if (m_litPipeHandFlipped)
		{
			auto now = std::chrono::steady_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_litPipeFlippedStartTime);
			int flippedDurationMs = static_cast<int>(duration.count());
			
			bool wasLongEnough = m_litPipeFlippedLongEnough;
			m_litPipeFlippedLongEnough = (flippedDurationMs >= flippedDurationThresholdMs);

			// Trigger emptying when threshold is crossed (only once per flip)
			if (m_litPipeFlippedLongEnough && !wasLongEnough && !m_litPipeEmptiedTriggered)
			{
				_MESSAGE("[VRInputTracker] Lit pipe hand FLIPPED LONG ENOUGH (%d ms >= %d ms threshold) - EMPTYING LIT PIPE",
					flippedDurationMs, flippedDurationThresholdMs);
				m_litPipeEmptiedTriggered = true;
				HandleLitPipeEmptied();
			}
		}
		else
		{
			m_litPipeFlippedLongEnough = false;
		}
	}

	void VRInputTracker::HandleLitPipeEmptied()
	{
		_MESSAGE("[PipeEmpty] *** LIT PIPE EMPTIED - Dumping contents ***");
		
		// Determine which lit pipe type is equipped and clear the correct cache
		Actor* player = *g_thePlayer;
		if (player)
		{
			// Check which lit pipe is equipped to clear the correct cache
			TESForm* leftItem = player->GetEquippedObject(true);
			TESForm* rightItem = player->GetEquippedObject(false);
			
			bool isWoodenPipeLit = false;
			bool isBonePipeLit = false;
			bool isRolledSmokeLit = false;
			
			if (leftItem)
			{
				if (IsWoodenPipeLitWeapon(leftItem->formID))
				 isWoodenPipeLit = true;
				else if (IsBonePipeLitWeapon(leftItem->formID))
				 isBonePipeLit = true;
				else if (IsRolledSmokeLitWeapon(leftItem->formID))
				 isRolledSmokeLit = true;
			}
			if (rightItem && !isWoodenPipeLit && !isBonePipeLit && !isRolledSmokeLit)
			{
				if (IsWoodenPipeLitWeapon(rightItem->formID))
				 isWoodenPipeLit = true;
				else if (IsBonePipeLitWeapon(rightItem->formID))
				 isBonePipeLit = true;
				else if (IsRolledSmokeLitWeapon(rightItem->formID))
				 isRolledSmokeLit = true;
			}
			
			if (isWoodenPipeLit)
			{
				g_filledWoodenPipeSmokableFormId = 0;
				g_filledWoodenPipeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[PipeEmpty]   -> Cleared WOODEN PIPE smokable cache");
			}
			else if (isBonePipeLit)
			{
				g_filledBonePipeSmokableFormId = 0;
				g_filledBonePipeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[PipeEmpty]   -> Cleared BONE PIPE smokable cache");
			}
			else if (isRolledSmokeLit)
			{
				g_filledRolledSmokeSmokableFormId = 0;
				g_filledRolledSmokeSmokableCategory = SmokableCategory::None;
				_MESSAGE("[PipeEmpty]   -> Cleared ROLLED SMOKE smokable cache");
			}
			else
			{
				_MESSAGE("[PipeEmpty]   -> WARNING: Could not determine lit pipe type to clear cache");
			}
		}
		
		// Clear the active smokable as well
		g_activeSmokableFormId = 0;
		g_activeSmokableCategory = SmokableCategory::None;
		_MESSAGE("[PipeEmpty]   -> Cleared active smokable");

		// Trigger haptic feedback to confirm emptying
		TriggerHapticFeedback(m_litItemInLeftHand, m_litItemInRightHand, 0.3f, 0.2f);
		_MESSAGE("[PipeEmpty]   -> Haptic feedback triggered on %s hand", m_litItemInLeftHand ? "LEFT" : "RIGHT");

		// Note: DepleteLitPipeToEmpty finds the equipped item itself by checking both game hands,
		// so no VR-to-game hand conversion is needed here

		// Use EquipStateManager to swap lit pipe to empty pipe
		if (g_equipStateManager)
		{
			g_equipStateManager->DepleteLitPipeToEmpty();
		}
	}

	void VRInputTracker::UpdateLightingConditionDetection()
	{
		// Store previous state
		m_prevLightingConditionMet = m_lightingConditionMet;

		// Check if we have a fire spell equipped in either hand
		bool hasFireSpell = m_fireSpellLeftHand || m_fireSpellRightHand;

		// Check if we have a lightable item (herb pipe or unlit rolled smoke) in either hand
		bool hasHerbPipe = m_herbPipeInLeftHand || m_herbPipeInRightHand;
		bool hasUnlitRolledSmoke = m_unlitRolledSmokeInLeftHand || m_unlitRolledSmokeInRightHand;
		bool hasLightableItem = hasHerbPipe || hasUnlitRolledSmoke;

		// Lighting condition uses different radius based on item type:
		// - Herb pipes use configPipeLightingRadius
		// - Rolled smokes use configRolledSmokeLightingRadius (larger)
		bool controllersNearEnough = false;
		if (hasHerbPipe)
		{
			controllersNearEnough = m_controllersNearForPipeLighting;
		}
		else if (hasUnlitRolledSmoke)
		{
			controllersNearEnough = m_controllersNearForRolledSmokeLighting;
		}

		// Lighting condition: fire spell + lightable item + controllers near enough
		m_lightingConditionMet = hasFireSpell && hasLightableItem && controllersNearEnough;

		// Log and start timer when lighting condition is first met
		if (m_lightingConditionMet && !m_prevLightingConditionMet)
		{
			m_lightingConditionStartTime = std::chrono::steady_clock::now();
			m_lightingTriggered = false; // Reset trigger flag when starting new lighting attempt
			m_burningSoundStarted = false; // Reset sound flag when starting new lighting attempt

			const char* fireHand = m_fireSpellLeftHand ? "LEFT" : "RIGHT";
			const char* itemType = "Unknown";
			const char* itemHand = "Unknown";

			if (m_herbPipeInLeftHand)
			{
				itemType = "Herb Pipe";
				itemHand = "LEFT";
			}
			else if (m_herbPipeInRightHand)
			{
				itemType = "Herb Pipe";
				itemHand = "RIGHT";
			}
			else if (m_unlitRolledSmokeInLeftHand)
			{
				itemType = "Unlit Rolled Smoke";
				itemHand = "LEFT";
			}
			else if (m_unlitRolledSmokeInRightHand)
			{
				itemType = "Unlit Rolled Smoke";
				itemHand = "RIGHT";
			}

			_MESSAGE("[Lighting] *** LIGHTING CONDITION MET! ***");
			_MESSAGE("[Lighting]   -> Fire spell in %s hand", fireHand);
			_MESSAGE("[Lighting]   -> %s in %s hand", itemType, itemHand);
			_MESSAGE("[Lighting]   -> Using %s lighting radius: %.1f", 
				hasHerbPipe ? "PIPE" : "ROLLED SMOKE",
				hasHerbPipe ? configPipeLightingRadius : configRolledSmokeLightingRadius);
			_MESSAGE("[Lighting]   -> Haptic feedback started - sound will play after 0.6 seconds, light after 3 seconds!");
		}
		else if (!m_lightingConditionMet && m_prevLightingConditionMet)
		{
			_MESSAGE("[Lighting] Lighting condition NO LONGER met - haptic feedback and sound stopped");
			m_lightingTriggered = false;
			m_burningSoundStarted = false;

			// Stop burning sound when lighting condition ends
			StopBurningSound();
		}

		// Timing thresholds
		const int soundDelayMs = 600;       // 0.6 seconds before sound starts
		const int lightingDurationThresholdMs = 3000; // 3 seconds to trigger lighting

		if (m_lightingConditionMet)
		{
			// Get current duration
			int durationMs = GetLightingConditionDurationMs();

			// Trigger continuous weak haptic feedback on the hand with the lightable item (starts immediately)
			bool lightableInLeft = m_herbPipeInLeftHand || m_unlitRolledSmokeInLeftHand;
			bool lightableInRight = m_herbPipeInRightHand || m_unlitRolledSmokeInRightHand;
			TriggerHapticPulse(lightableInLeft, lightableInRight, 0.08f); // Weak continuous feedback

			// Start burning sound after 1.3 seconds
			if (durationMs >= soundDelayMs && !m_burningSoundStarted)
			{
				_MESSAGE("[Lighting] 1.3 seconds reached - starting burning sound!");
				PlayRandomBurningSound();
				m_burningSoundStarted = true;
			}

			// Check if held long enough to trigger lighting (3 seconds)
			if (durationMs >= lightingDurationThresholdMs && !m_lightingTriggered)
			{
				_MESSAGE("[Lighting] *** LIGHTING TRIGGERED! (%d ms >= %d ms threshold) ***", durationMs, lightingDurationThresholdMs);
				m_lightingTriggered = true;

				// Stop the burning sound now that lighting is complete
				StopBurningSound();
				m_burningSoundStarted = false;

				// Trigger stronger haptic feedback to confirm lighting
				TriggerHapticFeedback(lightableInLeft, lightableInRight, 0.5f, 0.3f);

				// Determine what to light and call EquipStateManager to handle the swap
				// Convert VR controller hands to game hands for the equip state manager
				// In left-handed mode: left VR controller = right game hand, right VR controller = left game hand
				if (g_equipStateManager)
				{
					if (m_herbPipeInLeftHand || m_herbPipeInRightHand)
					{
						bool gameLeftHand, gameRightHand;
						if (IsLeftHandedMode())
						{
							gameLeftHand = m_herbPipeInRightHand;   // Right VR = Left game
							gameRightHand = m_herbPipeInLeftHand;   // Left VR = Right game
							_MESSAGE("[Lighting] Left-handed mode: VR(%s) -> Game(%s)",
								m_herbPipeInLeftHand ? "LEFT" : "RIGHT",
								gameLeftHand ? "LEFT" : "RIGHT");
						}
						else
						{
							gameLeftHand = m_herbPipeInLeftHand;    // Left VR = Left game
							gameRightHand = m_herbPipeInRightHand;  // Right VR = Right game
						}
						g_equipStateManager->LightHerbPipe(gameLeftHand, gameRightHand);
					}
					else if (m_unlitRolledSmokeInLeftHand || m_unlitRolledSmokeInRightHand)
					{
						bool gameLeftHand, gameRightHand;
						if (IsLeftHandedMode())
						{
							gameLeftHand = m_unlitRolledSmokeInRightHand;   // Right VR = Left game
							gameRightHand = m_unlitRolledSmokeInLeftHand;   // Left VR = Right game
							_MESSAGE("[Lighting] Left-handed mode: VR(%s) -> Game(%s)",
								m_unlitRolledSmokeInLeftHand ? "LEFT" : "RIGHT",
								gameLeftHand ? "LEFT" : "RIGHT");
						}
						else
						{
							gameLeftHand = m_unlitRolledSmokeInLeftHand;    // Left VR = Left game
							gameRightHand = m_unlitRolledSmokeInRightHand;  // Right VR = Right game
						}
						g_equipStateManager->LightRolledSmoke(gameLeftHand, gameRightHand);
					}
				}
			}
		}
	}

	void VRInputTracker::UpdateHandSwapDetection()
	{
		// Hand swap detection:
		// - One hand has a smokable (empty pipe, herb pipe, lit pipe, unlit/lit smoke)
		// - Other hand is empty (no weapon/spell equipped AND no item grabbed)
		// - Controllers touching for 2 seconds with 2 haptic pulses triggers the swap
		// - BLOCKED if either hand is in face zone

		// Check if either hand is in the face zone - block hand swap during smoking
		if (m_leftNearFace || m_rightNearFace)
		{
			if (m_handSwapConditionMet)
			{
				_MESSAGE("[HandSwap] Hand in face zone - swap cancelled");
			}
			m_handSwapHapticTriggered = false;
			m_handSwapSecondHapticTriggered = false;
			m_handSwapConditionMet = false;
			return;
		}

		// Check if we have a smokable in either hand
		bool smokableInLeft = m_smokeItemInLeftHand;
		bool smokableInRight = m_smokeItemInRightHand;

		// Need exactly one hand with smokable
		bool hasSmokableInOneHand = (smokableInLeft && !smokableInRight) || (!smokableInLeft && smokableInRight);

		if (!hasSmokableInOneHand)
		{
			// Reset all hand swap state when conditions aren't met
			m_handSwapHapticTriggered = false;
			m_handSwapSecondHapticTriggered = false;
			m_handSwapConditionMet = false;
			return;
		}

		// Check if the OTHER hand is empty (no weapon equipped)
		Actor* player = *g_thePlayer;
		if (!player)
		{
			m_handSwapHapticTriggered = false;
			m_handSwapSecondHapticTriggered = false;
			m_handSwapConditionMet = false;
			return;
		}

		bool otherHandEmpty = false;
		bool smokableHandIsLeft = smokableInLeft;

		// Get what's equipped in the other hand
		// Note: In left-handed mode, game hands are inverted relative to VR controllers
		// m_smokeItemInLeftHand/Right refers to VR controller, not game hand
		// We need to check the game's equipped items
		
		// Check the hand that DOESN'T have the smokable
		if (smokableHandIsLeft)
		{
			// Smokable in left VR controller, check right game hand
			// In left-handed mode: right VR controller = left game hand
			// In right-handed mode: right VR controller = right game hand
			bool checkLeftGameHand = IsLeftHandedMode();
			TESForm* otherHandItem = player->GetEquippedObject(checkLeftGameHand);
			otherHandEmpty = (otherHandItem == nullptr);
		}
		else
		{
			// Smokable in right VR controller, check left game hand
			// In left-handed mode: left VR controller = right game hand
			// In right-handed mode: left VR controller = left game hand
			bool checkLeftGameHand = !IsLeftHandedMode();
			TESForm* otherHandItem = player->GetEquippedObject(checkLeftGameHand);
			otherHandEmpty = (otherHandItem == nullptr);
		}

		if (!otherHandEmpty)
		{
			// Other hand has something equipped, reset hand swap state
			m_handSwapHapticTriggered = false;
			m_handSwapSecondHapticTriggered = false;
			m_handSwapConditionMet = false;
			return;
		}

		// Check if the other hand has a grabbed item (HIGGS)
		// This prevents hand swap when holding an ingredient or other object
		if (higgsInterface)
		{
			// The "other" VR controller is the one without the smokable
			bool otherVRControllerIsLeft = !smokableHandIsLeft;
			TESObjectREFR* grabbedObject = higgsInterface->GetGrabbedObject(otherVRControllerIsLeft);
			
			if (grabbedObject != nullptr)
			{
				// Other hand has something grabbed, reset hand swap state
				if (m_handSwapConditionMet)
				{
					_MESSAGE("[HandSwap] Other hand has grabbed item - swap cancelled");
				}
				m_handSwapHapticTriggered = false;
				m_handSwapSecondHapticTriggered = false;
				m_handSwapConditionMet = false;
				return;
			}
		}

		// All basic conditions met (smokable in one hand, other hand truly empty)
		// Now track touching duration
		
		// Timing thresholds
		const int firstHapticDelayMs = 0;      // First haptic immediately when touching starts
		const int secondHapticDelayMs = 1000;  // Second haptic at 1 second
		const int swapTriggerDelayMs = 2000;   // Swap triggers at 2 seconds

		// Check if controllers just started touching (rising edge)
		if (m_controllersTouching && !m_prevControllersTouching)
		{
			// Controllers just started touching - start the timer
			m_handSwapStartTime = std::chrono::steady_clock::now();
			m_handSwapConditionMet = true;
			m_handSwapHapticTriggered = false;
			m_handSwapSecondHapticTriggered = false;

			_MESSAGE("[HandSwap] *** HANDS TOUCHING - SWAP TIMER STARTED ***");
			_MESSAGE("[HandSwap]   -> Smokable in %s VR controller", smokableHandIsLeft ? "LEFT" : "RIGHT");
			_MESSAGE("[HandSwap]   -> Other hand is EMPTY");
			_MESSAGE("[HandSwap]   -> Hold for 2 seconds to swap...");
		}

		// Check if controllers stopped touching
		if (!m_controllersTouching && m_handSwapConditionMet)
		{
			_MESSAGE("[HandSwap] Controllers separated - swap cancelled");
			m_handSwapHapticTriggered = false;
			m_handSwapSecondHapticTriggered = false;
			m_handSwapConditionMet = false;
			return;
		}

		// If condition is met and controllers are still touching, check timing
		if (m_handSwapConditionMet && m_controllersTouching)
		{
			auto now = std::chrono::steady_clock::now();
			int durationMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - m_handSwapStartTime).count());

			// First haptic pulse (immediately)
			if (!m_handSwapHapticTriggered && durationMs >= firstHapticDelayMs)
			{
				TriggerHapticFeedback(true, true, 0.8f, 0.15f);  // Strong pulse on BOTH hands
				m_handSwapHapticTriggered = true;
				_MESSAGE("[HandSwap] First haptic pulse triggered (0ms)");
			}

			// Second haptic pulse (at 1 second)
			if (!m_handSwapSecondHapticTriggered && durationMs >= secondHapticDelayMs)
			{
				TriggerHapticFeedback(true, true, 0.8f, 0.15f);  // Strong pulse on BOTH hands
				m_handSwapSecondHapticTriggered = true;
				_MESSAGE("[HandSwap] Second haptic pulse triggered (1000ms)");
			}

			// Trigger swap (at 2 seconds)
			if (durationMs >= swapTriggerDelayMs)
			{
				_MESSAGE("[HandSwap] *** 2 SECONDS REACHED - TRIGGERING SWAP! ***");

				// Unequip the dummy item from the current hand
				// The armor will be auto-handled by the equip event handlers
				if (g_equipStateManager)
				{
					_MESSAGE("[HandSwap] Unequipping smokable from %s VR controller...", smokableHandIsLeft ? "LEFT" : "RIGHT");
					g_equipStateManager->UnequipCurrentSmokable(smokableHandIsLeft);
				}

				// Reset state after triggering
				m_handSwapHapticTriggered = false;
				m_handSwapSecondHapticTriggered = false;
				m_handSwapConditionMet = false;
			}
		}
	}

	void VRInputTracker::UpdateGrabbedItemNearSmokableHandDetection()
	{
		// Detect when a grabbed item (in the non-smokable hand) enters the zone near the smokable hand
		// This is used to re-apply cached VRIK finger settings when HIGGS might interfere

		m_prevGrabbedItemNearSmokableHand = m_grabbedItemNearSmokableHand;

		// Need a smoke item equipped in exactly one hand (not both)
		bool smokableInLeft = m_smokeItemInLeftHand;
		bool smokableInRight = m_smokeItemInRightHand;
		
		// If smokable in both hands or neither, skip this detection
		if ((smokableInLeft && smokableInRight) || (!smokableInLeft && !smokableInRight))
		{
			m_grabbedItemNearSmokableHand = false;
			return;
		}

		// Check if HIGGS interface is available
		if (!higgsInterface)
		{
			m_grabbedItemNearSmokableHand = false;
			return;
		}

		// Determine which hand has the smokable and which is the "other" hand
		// smokableInLeft XOR smokableInRight is guaranteed true here
		bool smokableHandIsLeft = smokableInLeft;
		bool otherHandIsLeft = !smokableHandIsLeft;  // The hand WITHOUT the smokable

		// Check if the OTHER hand (non-smokable hand) has a grabbed item
		TESObjectREFR* grabbedObject = higgsInterface->GetGrabbedObject(otherHandIsLeft);

		if (!grabbedObject)
		{
			m_grabbedItemNearSmokableHand = false;
			return;
		}

		// We have a smokable in one hand and a grabbed item in the other
		// Check if the controllers are close together (grabbed item near smokable hand)
		float controllerDistance = GetControllerToControllerDistance();
		
		// Use the same touch radius as controller touching detection
		bool isNear = (controllerDistance <= configControllerTouchRadius);
		m_grabbedItemNearSmokableHand = isNear;

		// Log when state changes
		if (m_grabbedItemNearSmokableHand && !m_prevGrabbedItemNearSmokableHand)
		{
			_MESSAGE("[GrabbedItemZone] Grabbed item ENTERED smokable hand zone (distance=%.2f)", controllerDistance);
			_MESSAGE("[GrabbedItemZone]   -> Smokable in %s VR controller, grabbed item in %s VR controller",
				smokableHandIsLeft ? "LEFT" : "RIGHT",
				otherHandIsLeft ? "LEFT" : "RIGHT");
		}
		else if (!m_grabbedItemNearSmokableHand && m_prevGrabbedItemNearSmokableHand)
		{
			_MESSAGE("[GrabbedItemZone] Grabbed item LEFT smokable hand zone (distance=%.2f)", controllerDistance);
		}

		// While grabbed item is near smokable hand, continuously re-apply cached finger positions
		// This prevents HIGGS from resetting the smokable hand's finger pose
		if (m_grabbedItemNearSmokableHand)
		{
			CachedFingerPositions* cached = GetCachedFingerPositions(smokableHandIsLeft);
			if (cached && cached->isSet && vrikInterface)
			{
				vrikInterface->setFingerRange(smokableHandIsLeft,
					cached->thumb1, cached->thumb2,
					cached->index1, cached->index2,
					cached->middle1, cached->middle2,
					cached->ring1, cached->ring2,
					cached->pinky1, cached->pinky2);
				// Only log on first entry, not every frame
				if (!m_prevGrabbedItemNearSmokableHand)
				{
					_MESSAGE("[GrabbedItemZone]   -> Re-applying cached finger positions for %s VR controller", 
						smokableHandIsLeft ? "LEFT" : "RIGHT");
				}
			}
		}
	}

	// ============================================
	// Global Functions
	// ============================================

	void InitializeVRInputTracker()
	{
		if (g_vrInputTracker == nullptr)
		{
			g_vrInputTracker = new VRInputTracker();
			g_vrInputTracker->Initialize();
			_MESSAGE("VRInputTracker: Global instance created");
		}
	}

	void ShutdownVRInputTracker()
	{
		if (g_vrInputTracker != nullptr)
		{
			delete g_vrInputTracker;
			g_vrInputTracker = nullptr;
			_MESSAGE("VRInputTracker: Global instance destroyed");
		}
	}
}

