// Shared helper: returns true if tracking is enabled+active and wrote a new camTM.
// adsScale gradually reduces sensitivity while aiming.
void HT_ApplyTracking(float pDt, SCR_CharacterControllerComponent controller, out vector camTM[4])
{
	HT_HeadTrackerState state = HT_HeadTrackerState.GetInstance();
	if (!state || !state.IsTracking() || !controller)
		return;

	float adsScale = 1.0;
	float adsT = controller.GetADSTime();
	if (adsT > 0)
		adsScale = Math.Lerp(1.0, 0.25, adsT);

	if (state.ApplyToCamera(pDt, camTM, adsScale))
		controller.SetFreeLook(true, true, false);
}

modded class CharacterCamera1stPerson
{
	override void OnActivate(ScriptedCameraItem pPrevCamera, ScriptedCameraItemResult pPrevCameraResult)
	{
		super.OnActivate(pPrevCamera, pPrevCameraResult);
		HT_HeadTrackerState.Reset();
		HT_HeadTrackerState.GetOrCreate();
	}

	override void OnUpdate(float pDt, out ScriptedCameraItemResult pOutResult)
	{
		super.OnUpdate(pDt, pOutResult);

		HT_HeadTrackerState state = HT_HeadTrackerState.GetInstance();
		if (!state)
			return;

		state.Update(pDt);

		InputManager inputMgr = GetGame().GetInputManager();
		inputMgr.ActivateContext("HT_Context");
		if (inputMgr.GetActionValue("HT_Toggle") > 0.5)
		{
			state.m_bEnabled = !state.m_bEnabled;
			if (state.m_bEnabled)
				SCR_HintManagerComponent.ShowCustomHint("Head tracking enabled", "Head Tracker", 2.0);
			else
				SCR_HintManagerComponent.ShowCustomHint("Head tracking disabled", "Head Tracker", 2.0);
		}

		if (inputMgr.GetActionValue("HT_Zero") > 0.5)
		{
			state.PostCommand("/calibrate");
			SCR_HintManagerComponent.ShowCustomHint("Recentered", "Head Tracker", 1.5);
		}

		if (inputMgr.GetActionValue("HT_StartStop") > 0.5)
		{
			state.PostCommand("/toggle");
			SCR_HintManagerComponent.ShowCustomHint("Toggled webcam tracker", "Head Tracker", 1.5);
		}

		HT_ApplyTracking(pDt, SCR_CharacterControllerComponent.Cast(m_ControllerComponent), pOutResult.m_CameraTM);
	}
}
