class SCR_ProjectileCameraComponentClass: ScriptComponentClass
{
}

class SCR_ProjectileCameraComponent: ScriptComponent
{
	[Attribute("0", UIWidgets.Slider, "Delay before camera activates (seconds)", "0 30 0.1")]
	float m_fActivationDelay;
	
	[Attribute("3", UIWidgets.Slider, "Delay after impact before returning camera (seconds)", "0 10 0.1")]
	float m_fImpactDelay;
	
	protected bool m_bRegistered = false;
	protected int m_iFrameCount = 0;
	protected vector m_vLastPos;
	protected float m_fLastSpeed = 0;
	
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.FRAME);
		m_vLastPos = owner.GetOrigin();
	}
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (m_bRegistered)
		{
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}
		
		m_iFrameCount++;
		
		if (m_iFrameCount < 10)
		{
			m_vLastPos = owner.GetOrigin();
			return;
		}
		
		SCR_ProjectileCameraManager manager = SCR_ProjectileCameraManager.GetInstance();
		if (!manager)
			return;
		
		if (!manager.IsArmed())
			return;
		
		vector currentPos = owner.GetOrigin();
		vector velocity = currentPos - m_vLastPos;
		float frameSpeed = velocity.Length() / timeSlice;
		
		Physics phys = owner.GetPhysics();
		float physSpeed = 0;
		if (phys)
		{
			vector physVel = phys.GetVelocity();
			physSpeed = physVel.Length();
		}
		
		float speed = Math.Max(frameSpeed, physSpeed);
		
		m_vLastPos = currentPos;
		
		if (speed > 30 && m_fLastSpeed > 30)
		{
			Print(string.Format(">>> Camera Follow: Projectile speed confirmed at %1 m/s - Activating", speed), LogLevel.NORMAL);
			manager.RegisterProjectile(owner, m_fActivationDelay, m_fImpactDelay);
			m_bRegistered = true;
			ClearEventMask(owner, EntityEvent.FRAME);
		}
		
		m_fLastSpeed = speed;
	}
	
	override void OnDelete(IEntity owner)
	{
		if (!m_bRegistered)
			return;
		
		SCR_ProjectileCameraManager manager = SCR_ProjectileCameraManager.GetInstance();
		if (manager)
			manager.UnregisterProjectile(owner);
	}
}