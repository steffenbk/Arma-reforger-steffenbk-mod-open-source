class SCR_ProjectileCameraManagerClass: ScriptComponentClass
{
}

class SCR_ProjectileCameraManager: ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable camera follow system")]
	bool m_bEnabled;
	
	[Attribute("0", UIWidgets.CheckBox, "Show debug info")]
	bool m_bDebugMode;
	
	[Attribute("15", UIWidgets.Slider, "Camera offset distance", "5 50 1")]
	float m_fCameraOffset;
	
	[Attribute("1", UIWidgets.Slider, "Pitch follow amount (0=none, 1=full)", "0 1 0.1")]
	float m_fPitchMultiplier;
	
	[Attribute("100", UIWidgets.Slider, "Stop following below this altitude (meters)", "0 500 10")]
	float m_fMinFollowAltitude;
	
	protected IEntity m_CurrentProjectile;
	protected CameraManager m_CameraManager;
	protected CameraBase m_OriginalCamera;
	protected IEntity m_FollowCameraEntity;
	protected vector m_vLastPos;
	protected bool m_bProjectileDeleted = false;
	protected bool m_bAltitudeFrozen = false;
	protected bool m_bHasReachedAltitude = false;
	protected bool m_bArmed = false;
	protected int m_iLocalPlayerId = -1;
	protected float m_fCurrentActivationDelay = 0;
	protected float m_fCurrentImpactDelay = 3;
	
	override void OnPostInit(IEntity owner)
	{
		PlayerController pc = PlayerController.Cast(owner);
		if (pc)
			m_iLocalPlayerId = pc.GetPlayerId();
		
		Print(string.Format(">>> SCR_ProjectileCameraManager initialized for Player ID: %1", m_iLocalPlayerId), LogLevel.NORMAL);
	}
	
	static SCR_ProjectileCameraManager GetInstance()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;
		
		return SCR_ProjectileCameraManager.Cast(pc.FindComponent(SCR_ProjectileCameraManager));
	}
	
	void ToggleArmed()
	{
		m_bArmed = !m_bArmed;
		Print(string.Format(">>> Player %1 Camera Follow Armed: %2", m_iLocalPlayerId, m_bArmed), LogLevel.NORMAL);
	}
	
	bool IsArmed()
	{
		return m_bArmed;
	}
	
	int GetPlayerId()
	{
		return m_iLocalPlayerId;
	}
	
	void RegisterProjectile(IEntity projectile, float activationDelay, float impactDelay)
	{
		if (!m_bEnabled)
			return;
		
		if (!m_bArmed)
			return;
		
		Print(string.Format(">>> Player %1 registering projectile (Activation: %2s, Impact delay: %3s)", m_iLocalPlayerId, activationDelay, impactDelay), LogLevel.NORMAL);
		m_CurrentProjectile = projectile;
		m_vLastPos = projectile.GetOrigin();
		m_bProjectileDeleted = false;
		m_bAltitudeFrozen = false;
		m_bHasReachedAltitude = false;
		m_bArmed = false;
		m_fCurrentActivationDelay = activationDelay;
		m_fCurrentImpactDelay = impactDelay;
		
		int delayMs = m_fCurrentActivationDelay * 1000;
		GetGame().GetCallqueue().CallLater(ActivateCamera, delayMs, false);
	}
	
	void UnregisterProjectile(IEntity projectile)
	{
		if (m_CurrentProjectile == projectile)
		{
			Print(string.Format(">>> Player %1 projectile destroyed - freezing camera", m_iLocalPlayerId), LogLevel.NORMAL);
			
			m_bProjectileDeleted = true;
			GetGame().GetCallqueue().Remove(ActivateCamera);
			
			int impactDelayMs = m_fCurrentImpactDelay * 1000;
			GetGame().GetCallqueue().CallLater(DeactivateCamera, impactDelayMs, false);
			
			m_CurrentProjectile = null;
		}
	}
	
	protected void ActivateCamera()
	{
		if (!m_CurrentProjectile)
			return;
		
		m_CameraManager = GetGame().GetCameraManager();
		if (!m_CameraManager)
			return;
		
		m_OriginalCamera = m_CameraManager.CurrentCamera();
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		m_FollowCameraEntity = GetGame().SpawnEntityPrefab(Resource.Load("{2DF95AB3CCDF4FA5}Prefabs/Characters/Core/DefaultPlayerCamera.et"), GetGame().GetWorld(), params);
		
		if (!m_FollowCameraEntity)
			return;
		
		CameraBase followCam = CameraBase.Cast(m_FollowCameraEntity);
		if (followCam)
			m_CameraManager.SetCamera(followCam);
		
		GetGame().GetCallqueue().CallLater(UpdateCamera, 0, true);
		Print(string.Format(">>> Player %1 camera activated", m_iLocalPlayerId), LogLevel.NORMAL);
	}
	
	protected void DeactivateCamera()
	{
		GetGame().GetCallqueue().Remove(UpdateCamera);
		
		if (m_CameraManager && m_OriginalCamera)
			m_CameraManager.SetCamera(m_OriginalCamera);
		
		if (m_FollowCameraEntity)
		{
			delete m_FollowCameraEntity;
			m_FollowCameraEntity = null;
		}
		
		m_bProjectileDeleted = false;
		m_bAltitudeFrozen = false;
		m_bHasReachedAltitude = false;
		
		Print(string.Format(">>> Player %1 camera deactivated", m_iLocalPlayerId), LogLevel.NORMAL);
	}
	
	protected void UpdateCamera()
	{
		if (!m_FollowCameraEntity)
		{
			DeactivateCamera();
			return;
		}
		
		if (m_bProjectileDeleted)
		{
			return;
		}
		
		if (m_bAltitudeFrozen)
		{
			return;
		}
		
		if (!m_CurrentProjectile)
		{
			return;
		}
		
		vector targetPos = m_CurrentProjectile.GetOrigin();
		
		if (targetPos[1] < -100)
		{
			Print(">>> WARNING: Projectile position invalid (Y < -100), stopping updates", LogLevel.WARNING);
			m_bProjectileDeleted = true;
			return;
		}
		
		float terrainY = GetGame().GetWorld().GetSurfaceY(targetPos[0], targetPos[2]);
		float altitude = targetPos[1] - terrainY;
		
		if (altitude < -50)
		{
			Print(">>> WARNING: Projectile far underground, stopping updates", LogLevel.WARNING);
			m_bProjectileDeleted = true;
			return;
		}
		
		vector velocity = targetPos - m_vLastPos;
		float verticalVelocity = velocity[1];
		float speed = velocity.Length();
		
		Physics phys = m_CurrentProjectile.GetPhysics();
		vector physVel = vector.Zero;
		float physSpeed = 0;
		if (phys)
		{
			physVel = phys.GetVelocity();
			physSpeed = physVel.Length();
		}
		
		vector projectileAngles = m_CurrentProjectile.GetYawPitchRoll();
		
		if (m_bDebugMode)
		{
			Print(string.Format("========== PLAYER %1 PROJECTILE DEBUG ==========", m_iLocalPlayerId), LogLevel.NORMAL);
			Print(string.Format("Position: %1", targetPos), LogLevel.NORMAL);
			Print(string.Format("Altitude AGL: %1 m", altitude), LogLevel.NORMAL);
			Print(string.Format("Frame Speed: %1 m/s", speed), LogLevel.NORMAL);
			Print(string.Format("Physics Speed: %1 m/s", physSpeed), LogLevel.NORMAL);
			Print(string.Format("Vertical Velocity: %1 m/s", verticalVelocity), LogLevel.NORMAL);
			Print(string.Format("Projectile Angles - Yaw: %1, Pitch: %2, Roll: %3", projectileAngles[0], projectileAngles[1], projectileAngles[2]), LogLevel.NORMAL);
			Print(string.Format("Descending: %1", verticalVelocity < 0), LogLevel.NORMAL);
			Print("======================================", LogLevel.NORMAL);
		}
		
		if (verticalVelocity < 0)
		{
			if (!m_bHasReachedAltitude)
				m_bHasReachedAltitude = true;
			
			if (altitude < m_fMinFollowAltitude)
			{
				Print(string.Format(">>> Player %1 altitude threshold reached - freezing camera", m_iLocalPlayerId), LogLevel.NORMAL);
				m_bAltitudeFrozen = true;
				return;
			}
		}
		
		m_vLastPos = targetPos;
		
		vector cameraAngles = projectileAngles;
		cameraAngles[1] = projectileAngles[1] * m_fPitchMultiplier;
		cameraAngles[2] = 0;
		
		vector forwardDir = cameraAngles.AnglesToVector();
		vector cameraPos = targetPos - (forwardDir * m_fCameraOffset);
		
		if (cameraPos[1] < terrainY)
		{
			Print(">>> WARNING: Camera would be underground, clamping to terrain + 2m", LogLevel.WARNING);
			cameraPos[1] = terrainY + 2;
		}
		
		vector mat[4];
		Math3D.AnglesToMatrix(cameraAngles, mat);
		mat[3] = cameraPos;
		
		m_FollowCameraEntity.SetTransform(mat);
	}
}