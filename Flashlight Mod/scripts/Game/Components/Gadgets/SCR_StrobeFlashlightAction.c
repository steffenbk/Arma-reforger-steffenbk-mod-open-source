// SCR_StrobeFlashlightAction.c
//
// Cycles through strobe patterns defined on SCR_FlashlightCustomMade.
// Add to the ActionsManagerComponent on the flashlight prefab.
// Only shown when inspecting the flashlight while the light is on.
//
// Cycle order: OFF → Pattern 1 → Pattern 2 → … → OFF

class SCR_StrobeFlashlightAction : ScriptedUserAction
{
	protected SCR_FlashlightCustomMade m_FlashlightComp;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_FlashlightComp = SCR_FlashlightCustomMade.Cast(pOwnerEntity.FindComponent(SCR_FlashlightCustomMade));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_FlashlightComp || !m_FlashlightComp.IsLightOn())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		return controller && controller.GetInspectEntity() == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_FlashlightComp != null;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_FlashlightComp)
			return;

		m_FlashlightComp.CycleStrobePattern();
	}

	//------------------------------------------------------------------------------------------------
	// Shows current strobe state — "Strobe: OFF", "Strobe: Slow", "Strobe: Fast", etc.
	override bool GetActionNameScript(out string outName)
	{
		if (!m_FlashlightComp)
		{
			outName = "Strobe: OFF";
			return true;
		}

		outName = "Strobe: " + m_FlashlightComp.GetStrobePatternName();
		return true;
	}
}
