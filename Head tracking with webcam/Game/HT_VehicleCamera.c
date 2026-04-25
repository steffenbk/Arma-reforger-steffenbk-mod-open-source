modded class CharacterCamera1stPersonVehicle
{
	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);

		HT_HeadTrackerState state = HT_HeadTrackerState.GetInstance();
		if (!state || !state.m_bEnableInVehicles)
			return;

		state.Update(pDt);
		HT_ApplyTracking(pDt, SCR_CharacterControllerComponent.Cast(m_ControllerComponent), pOutResult.m_CameraTM);
	}
}

modded class CharacterCamera1stPersonTurret
{
	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);

		HT_HeadTrackerState state = HT_HeadTrackerState.GetInstance();
		if (!state || !state.m_bEnableInVehicles)
			return;

		state.Update(pDt);
		HT_ApplyTracking(pDt, SCR_CharacterControllerComponent.Cast(m_ControllerComponent), pOutResult.m_CameraTM);
	}
}
