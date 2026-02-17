class SCR_ToggleProjectileCameraAction: ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_ProjectileCameraManager manager = SCR_ProjectileCameraManager.GetInstance();
		if (manager)
			manager.ToggleArmed();
	}
	
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}
	
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}
	
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
	
	override bool GetActionNameScript(out string outName)
	{
		SCR_ProjectileCameraManager manager = SCR_ProjectileCameraManager.GetInstance();
		if (manager)
		{
			if (manager.IsArmed())
				outName = "Camera Follow: ARMED";
			else
				outName = "Camera Follow: OFF";
			
			return true;
		}
		
		return false;
	}
}