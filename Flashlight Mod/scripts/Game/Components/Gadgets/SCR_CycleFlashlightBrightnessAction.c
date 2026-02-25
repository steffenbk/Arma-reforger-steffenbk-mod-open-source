// SCR_CycleFlashlightBrightnessAction.c
//
// Cycles flashlight brightness through Low → Medium → High on a flashlight
// that has SCR_FlashlightCustomMade.
// Add this action to the ActionsManagerComponent on the flashlight prefab.
// Only shown when inspecting the flashlight and the light is on.

class SCR_CycleFlashlightBrightnessAction : ScriptedUserAction
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

		m_FlashlightComp.CycleBrightness();
	}

	//------------------------------------------------------------------------------------------------
	// Shows current brightness level so the player knows their active setting.
	override bool GetActionNameScript(out string outName)
	{
		if (!m_FlashlightComp)
		{
			outName = "Brightness: High";
			return true;
		}

		outName = "Brightness: " + m_FlashlightComp.GetBrightnessName();
		return true;
	}
}
