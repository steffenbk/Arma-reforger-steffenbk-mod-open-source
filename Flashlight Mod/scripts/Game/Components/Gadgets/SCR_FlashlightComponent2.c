// Strobe pattern entry — add as many as you want in the Strobe Patterns array on the component.
[BaseContainerProps()]
class SCR_FlashlightStrobePattern
{
	[Attribute("Strobe", UIWidgets.EditBox, desc: "Display name shown in the action menu")]
	string m_sName;

	[Attribute("0.3", UIWidgets.Slider, desc: "On/off interval in seconds", params: "0.05 2.0 0.05")]
	float m_fInterval;
}

// Brightness level entry — add as many as you want in the Brightness Levels array on the component.
[BaseContainerProps()]
class SCR_FlashlightBrightnessLevel
{
	[Attribute("High", UIWidgets.EditBox, desc: "Display name shown in the action menu")]
	string m_sName;

	[Attribute("1.0", UIWidgets.Slider, desc: "Multiplier on the lens light value. 1.0 = default, 2.0 = double, 0.3 = dim.", params: "0.05 2.0 0.05")]
	float m_fMultiplier;
}

//------------------------------------------------------------------------------------------------

[EntityEditorProps(category: "GameScripted/Gadgets", description: "Flashlight with death timer, drop, strobe and brightness", color: "0 0 255 255")]
class SCR_FlashlightCustomMadeClass : SCR_FlashlightComponentClass
{
}

class SCR_FlashlightCustomMade : SCR_FlashlightComponent
{
	// Death drop
	[Attribute("1", UIWidgets.CheckBox, desc: "Flashlight has a chance to drop when owner dies", category: "Death Drop")]
	protected bool m_bEnableDeathDrop;

	[Attribute("0.7", UIWidgets.Slider, desc: "Chance of dropping on death (0-1)", params: "0 1 0.05", category: "Death Drop")]
	protected float m_fDeathDropChance;

	// Death timer
	[Attribute("300.0", UIWidgets.EditBox, desc: "Seconds the light stays on after owner's death", params: "10 3600", category: "Death Timer")]
	protected float m_fDeathLightDuration;

	// Debug
	[Attribute("1", UIWidgets.CheckBox, desc: "Enable debug logs", category: "Debug")]
	protected bool m_bEnableDebugLogs;

	// Strobe — add pattern entries here in Workbench. Cycle: OFF → first → second → … → OFF.
	[Attribute(desc: "Strobe patterns. First entry activates first. Add as many as needed.", category: "Strobe")]
	protected ref array<ref SCR_FlashlightStrobePattern> m_aStrobePatterns;

	// Brightness — add level entries here in Workbench. First entry is active on startup.
	[Attribute(desc: "Brightness levels. First entry is the default. Add as many as needed.", category: "Brightness")]
	protected ref array<ref SCR_FlashlightBrightnessLevel> m_aBrightnessLevels;

	// Death timer state
	protected float m_fDeathTimer;
	protected bool  m_bIsDeathTimerActive;
	protected bool  m_bOwnerDeathHandled;
	protected bool  m_bOverrideDeactivation;

	// Strobe state — ticked via CallLater, NOT via ActivateGadgetUpdate (avoids AdjustTransform shift).
	protected bool  m_bIsStrobing;
	protected int   m_iStrobePatternIndex;

	// Brightness state
	protected int   m_iBrightnessLevel;

	//------------------------------------------------------------------------------------------------
	protected void DebugLog(string message)
	{
		if (m_bEnableDebugLogs)
			Print("[FlashlightDebug] " + message, LogLevel.NORMAL);
	}

	// Returns true when the light should be considered "active" (on or in death timer).
	protected bool IsLightActive()
	{
		return m_bActivated || m_bIsDeathTimerActive;
	}

	//==============================================================================================
	// PUBLIC API — called by action scripts
	//==============================================================================================

	bool IsLightOn()
	{
		return IsLightActive();
	}

	bool IsStrobing()
	{
		return m_bIsStrobing;
	}

	// Cycles: OFF → pattern[0] → pattern[1] → … → OFF
	void CycleStrobePattern()
	{
		if (!m_bActivated)
			return;

		if (!m_aStrobePatterns || m_aStrobePatterns.IsEmpty())
			return;

		if (!m_bIsStrobing)
		{
			m_iStrobePatternIndex = 0;
			StartStrobe();
		}
		else if (m_iStrobePatternIndex >= m_aStrobePatterns.Count() - 1)
		{
			StopStrobe();
		}
		else
		{
			m_iStrobePatternIndex++;
			GetGame().GetCallqueue().Remove(StrobeTick);
			GetGame().GetCallqueue().CallLater(StrobeTick, GetCurrentStrobeIntervalMs(), true);
			DebugLog("Strobe pattern: " + GetStrobePatternName());
		}
	}

	string GetStrobePatternName()
	{
		if (!m_bIsStrobing || !m_aStrobePatterns || m_aStrobePatterns.IsEmpty())
			return "OFF";
		if (m_iStrobePatternIndex < 0 || m_iStrobePatternIndex >= m_aStrobePatterns.Count())
			return "OFF";
		return m_aStrobePatterns[m_iStrobePatternIndex].m_sName;
	}

	void CycleBrightness()
	{
		if (!IsLightActive())
			return;

		if (!m_aBrightnessLevels || m_aBrightnessLevels.IsEmpty())
			return;

		m_iBrightnessLevel = (m_iBrightnessLevel + 1) % m_aBrightnessLevels.Count();
		DebugLog("Brightness: " + GetBrightnessName());
		ApplyBrightness();
	}

	string GetBrightnessName()
	{
		if (!m_aBrightnessLevels || m_aBrightnessLevels.IsEmpty())
			return "High";
		if (m_iBrightnessLevel < 0 || m_iBrightnessLevel >= m_aBrightnessLevels.Count())
			return "High";
		return m_aBrightnessLevels[m_iBrightnessLevel].m_sName;
	}

	//==============================================================================================
	// INTERNAL HELPERS
	//==============================================================================================

	protected int GetCurrentStrobeIntervalMs()
	{
		if (!m_aStrobePatterns || m_iStrobePatternIndex < 0 || m_iStrobePatternIndex >= m_aStrobePatterns.Count())
			return 300;
		return Math.Round(m_aStrobePatterns[m_iStrobePatternIndex].m_fInterval * 1000);
	}

	protected float GetBrightnessMultiplier()
	{
		if (!m_aBrightnessLevels || m_aBrightnessLevels.IsEmpty())
			return 1.0;
		if (m_iBrightnessLevel < 0 || m_iBrightnessLevel >= m_aBrightnessLevels.Count())
			return 1.0;
		return m_aBrightnessLevels[m_iBrightnessLevel].m_fMultiplier;
	}

	protected void ApplyBrightness()
	{
		if (!m_Light || !m_aLenseArray || m_aLenseArray.IsEmpty())
			return;

		float mult = GetBrightnessMultiplier();
		SCR_LenseColor lens = m_aLenseArray[m_iCurrentLenseColor];
		Color col = Color.FromVector(lens.m_vLenseColor);
		m_Light.SetColor(col, lens.m_fLightValue * mult);

		// m_fEmissiveIntensity is the base intensity (e.g. 100) — scale it by our multiplier.
		if (m_EmissiveMaterial)
			m_EmissiveMaterial.SetEmissiveMultiplier(m_fEmissiveIntensity * mult);
	}

	// Starts strobe via CallLater — does NOT touch ActivateGadgetUpdate, avoiding AdjustTransform.
	protected void StartStrobe()
	{
		m_bIsStrobing = true;
		DebugLog("Strobe ON: " + GetStrobePatternName());
		if (m_Light)
			m_Light.SetEnabled(true);
		GetGame().GetCallqueue().CallLater(StrobeTick, GetCurrentStrobeIntervalMs(), true);
	}

	// Stops strobe and restores steady light without calling EnableLight (avoids AdjustTransform).
	protected void StopStrobe()
	{
		m_bIsStrobing = false;
		DebugLog("Strobe OFF");
		GetGame().GetCallqueue().Remove(StrobeTick);
		if (m_Light)
			m_Light.SetEnabled(true);
		ApplyBrightness();
	}

	// Called by CallLater — continues as long as the light should be active.
	protected void StrobeTick()
	{
		// Stop if strobe was cancelled or light is fully off (not in death timer either).
		if (!m_bIsStrobing || (!m_bActivated && !m_bIsDeathTimerActive))
		{
			GetGame().GetCallqueue().Remove(StrobeTick);
			m_bIsStrobing = false;
			return;
		}
		if (m_Light)
			m_Light.SetEnabled(!m_Light.IsEnabled());
	}

	//==============================================================================================
	// OVERRIDES
	//==============================================================================================

	override protected void UpdateLightState()
	{
		// Strobe controls the light — only guard when light should actually be active.
		if (m_bIsStrobing && IsLightActive())
			return;

		if (IsLightActive())
			EnableLight();
		else
			DisableLight();
	}

	override protected void DisableLight()
	{
		if (m_bIsDeathTimerActive)
		{
			DebugLog("DisableLight blocked — death timer active");
			EnableLight();
			return;
		}

		// Block disable when strobe is active and light should be on.
		if (m_bIsStrobing && IsLightActive())
		{
			DebugLog("DisableLight blocked — strobe active");
			return;
		}

		if (m_iMode == EGadgetMode.ON_GROUND && m_bActivated)
		{
			DebugLog("DisableLight blocked — ON_GROUND and active");
			return;
		}

		DebugLog("DisableLight executed");
		m_bOverrideDeactivation = false;
		super.DisableLight();
	}

	override protected void EnableLight()
	{
		super.EnableLight();
		ApplyBrightness();

		if (m_bIsDeathTimerActive || (m_iMode == EGadgetMode.ON_GROUND && m_bActivated))
		{
			m_bOverrideDeactivation = true;
			ActivateGadgetUpdate();
		}
	}

	override protected void SwitchLenses(int filter)
	{
		super.SwitchLenses(filter);
		ApplyBrightness();
	}

	override protected void DeactivateGadgetUpdate()
	{
		if (m_bOverrideDeactivation)
		{
			DebugLog("DeactivateGadgetUpdate blocked — override flag set");
			return;
		}

		if ((m_iMode == EGadgetMode.ON_GROUND && m_bActivated) || m_bIsDeathTimerActive)
		{
			DebugLog("DeactivateGadgetUpdate blocked — ON_GROUND or death timer");
			return;
		}

		DebugLog("DeactivateGadgetUpdate executed");
		super.DeactivateGadgetUpdate();
	}

	override protected void ToggleActive(bool state, SCR_EUseContext context)
	{
		if (m_bIsDeathTimerActive)
		{
			DebugLog("Manual toggle — stopping death timer");
			m_bIsDeathTimerActive = false;
			m_fDeathTimer = 0;
			m_bOverrideDeactivation = false;
		}

		// Don't stop strobe here — StrobeTick self-stops when m_bActivated becomes false.
		// This preserves strobe through drop (ModeSwitch re-sets m_bActivated) and death (death timer keeps it alive).

		super.ToggleActive(state, context);
	}

	override void ActivateAction()
	{
		if (m_CharController && m_CharController.GetLifeState() != ECharacterLifeState.ALIVE)
		{
			DebugLog("ActivateAction blocked — owner not alive");
			return;
		}

		super.ActivateAction();
	}

	override protected void OnOwnerLifeStateChanged(ECharacterLifeState previousLifeState, ECharacterLifeState newLifeState)
	{
		if (newLifeState == ECharacterLifeState.ALIVE)
		{
			DebugLog("Owner revived — resetting death timer");
			m_bIsDeathTimerActive = false;
			m_fDeathTimer = 0;
			m_bOwnerDeathHandled = false;
			m_bOverrideDeactivation = false;
			UpdateLightState();
			return;
		}

		if (previousLifeState != ECharacterLifeState.ALIVE)
		{
			super.OnOwnerLifeStateChanged(previousLifeState, newLifeState);
			return;
		}

		bool wasActive = m_bActivated;
		super.OnOwnerLifeStateChanged(previousLifeState, newLifeState);

		if (!wasActive)
			return;

		DebugLog("Owner died with light ON");

		if (m_bEnableDeathDrop && Math.RandomFloat01() <= m_fDeathDropChance)
		{
			DebugLog("Dropping flashlight");
			DropFlashlightOnDeath();
		}

		StartDeathTimer();
	}

	override protected void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (!m_bActivated && !m_bIsDeathTimerActive)
			return;

		BaseCompartmentSlot compSlot = manager.FindCompartment(slotID, mgrID);
		IEntity occupant = compSlot.GetOccupant();
		if (occupant == m_CharacterOwner)
		{
			SetShadowNearPlane(true);
			if (!m_bIsDeathTimerActive && PilotCompartmentSlot.Cast(compSlot))
				ToggleActive(false, SCR_EUseContext.FROM_ACTION);
		}
	}

	override protected void OnCompartmentLeft(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		if (!m_bActivated && !m_bIsDeathTimerActive)
			return;

		BaseCompartmentSlot compSlot = manager.FindCompartment(slotID, mgrID);
		IEntity occupant = compSlot.GetOccupant();
		if (occupant == m_CharacterOwner)
			SetShadowNearPlane(true, true);
	}

	override void OnSlotOccludedStateChanged(bool occluded)
	{
		if (m_bIsDeathTimerActive)
		{
			EnableLight();
			return;
		}

		super.OnSlotOccludedStateChanged(occluded);
	}

	override protected void ModeSwitch(EGadgetMode mode, IEntity charOwner)
	{
		bool wasActive = m_bActivated;
		super.ModeSwitch(mode, charOwner);

		if (mode == EGadgetMode.ON_GROUND && (wasActive || m_bIsDeathTimerActive) && m_Light)
		{
			DebugLog("ModeSwitch ON_GROUND — keeping light on");
			m_bActivated = true;
			m_bOverrideDeactivation = true;
			EnableLight();
			m_Light.SetEnabled(true);
			ActivateGadgetUpdate();
			// Explicit call to ensure brightness persists through the mode transition.
			ApplyBrightness();
		}
	}

	override protected void ModeClear(EGadgetMode mode)
	{
		if (mode == EGadgetMode.ON_GROUND && (m_bActivated || m_bIsDeathTimerActive))
			m_bLastLightState = true;

		super.ModeClear(mode);
	}

	override void Update(float timeSlice)
	{
		if (m_bIsDeathTimerActive)
		{
			m_fDeathTimer += timeSlice;

			float flooredTime = Math.Floor(m_fDeathTimer);
			if (m_bEnableDebugLogs && flooredTime > 0 && Math.Mod(flooredTime, 30) < 0.1)
				DebugLog("Death timer: " + flooredTime + "s");

			if (m_fDeathTimer >= m_fDeathLightDuration)
			{
				DebugLog("Death timer expired");
				m_bIsDeathTimerActive = false;
				m_bActivated = false;
				m_bOverrideDeactivation = false;
				m_bIsStrobing = false;
				GetGame().GetCallqueue().Remove(StrobeTick);
				if (m_Light) m_Light.SetEnabled(false);
				if (m_EmissiveMaterial) m_EmissiveMaterial.SetEmissiveMultiplier(0);
				DeactivateGadgetUpdate();
				return;
			}

			// Only force steady light if strobe is not running (strobe manages light directly).
			if (!m_bIsStrobing && m_Light && !m_Light.IsEnabled())
				EnableLight();

			return;
		}

		if (m_iMode == EGadgetMode.ON_GROUND)
		{
			if (m_bActivated && !m_bIsStrobing && m_Light && !m_Light.IsEnabled())
				EnableLight();
			return;
		}

		super.Update(timeSlice);
	}

	override bool RplSave(ScriptBitWriter writer)
	{
		if (!super.RplSave(writer))
			return false;

		writer.WriteBool(m_bIsDeathTimerActive);
		writer.WriteFloat(m_fDeathTimer);
		writer.WriteBool(m_bOwnerDeathHandled);
		writer.WriteBool(m_bOverrideDeactivation);
		writer.WriteBool(m_bIsStrobing);
		writer.WriteInt(m_iStrobePatternIndex);
		writer.WriteInt(m_iBrightnessLevel);

		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		if (!super.RplLoad(reader))
			return false;

		reader.ReadBool(m_bIsDeathTimerActive);
		reader.ReadFloat(m_fDeathTimer);
		reader.ReadBool(m_bOwnerDeathHandled);
		reader.ReadBool(m_bOverrideDeactivation);
		reader.ReadBool(m_bIsStrobing);
		reader.ReadInt(m_iStrobePatternIndex);
		reader.ReadInt(m_iBrightnessLevel);

		if (m_bIsDeathTimerActive || (m_iMode == EGadgetMode.ON_GROUND && m_bActivated))
		{
			m_bOverrideDeactivation = true;
			ActivateGadgetUpdate();
		}

		if (m_bIsStrobing)
			GetGame().GetCallqueue().CallLater(StrobeTick, GetCurrentStrobeIntervalMs(), true);

		return true;
	}

	override bool OnTicksOnRemoteProxy()
	{
		if (!m_bIsStrobing && (m_bIsDeathTimerActive || (m_bActivated && m_iMode == EGadgetMode.ON_GROUND)))
		{
			if (m_Light && !m_Light.IsEnabled())
			{
				EnableLight();
				m_bOverrideDeactivation = true;
				ActivateGadgetUpdate();
			}
		}
		return true;
	}

	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);

		m_fDeathTimer = 0;
		m_bIsDeathTimerActive = false;
		m_bOwnerDeathHandled = false;
		m_bOverrideDeactivation = false;
		m_bIsStrobing = false;
		m_iStrobePatternIndex = 0;
		m_iBrightnessLevel = 0;

		DebugLog("Initialised — death drop chance: " + m_fDeathDropChance);
	}

	override protected void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(StrobeTick);
		super.OnDelete(owner);
	}

	//==============================================================================================
	// PRIVATE
	//==============================================================================================

	protected void DropFlashlightOnDeath()
	{
		if (m_bOwnerDeathHandled)
			return;

		m_bOwnerDeathHandled = true;

		if (m_iMode != EGadgetMode.IN_SLOT && m_iMode != EGadgetMode.IN_HAND)
		{
			DebugLog("Drop skipped — not in slot or hand");
			return;
		}

		if (!m_CharacterOwner)
			return;

		vector position = m_CharacterOwner.GetOrigin();
		position[1] = position[1] + 0.1;

		ModeSwitch(EGadgetMode.ON_GROUND, null);

		vector mat[4];
		GetOwner().GetWorldTransform(mat);
		mat[3] = position;
		GetOwner().SetWorldTransform(mat);

		DebugLog("Dropped at: " + position.ToString());
	}

	protected void StartDeathTimer()
	{
		// Do NOT stop strobe here — preserve strobe through death.
		DebugLog("Starting death timer — " + m_fDeathLightDuration + "s");
		m_bIsDeathTimerActive = true;
		m_fDeathTimer = 0;
		m_bOverrideDeactivation = true;

		// Only enable steady light if strobe isn't already managing the light.
		if (!m_bIsStrobing)
			EnableLight();

		ActivateGadgetUpdate();
	}
}
