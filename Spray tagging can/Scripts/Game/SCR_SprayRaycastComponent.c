[ComponentEditorProps(category: "GameScripted", description: "Raycast spray paint - spawns decal on weapon fire, deletes projectile")]
class SCR_SprayRaycastComponentClass : ScriptComponentClass {}

class SCR_SprayRaycastComponent : ScriptComponent
{
	[Attribute("{9EE38D3540663BCF}Assets/Decals/Blood/Decal_Blood10.emat", UIWidgets.ResourcePickerThumbnail, "Paint decal material (.emat)", "emat")]
	protected ResourceName m_sDecalMaterial;

	[Attribute("20", UIWidgets.Slider, "Max spray range in meters", "0.5 50 0.5")]
	protected float m_fMaxRange;

	[Attribute("1.8", UIWidgets.Slider, "Decal size in meters", "0.05 2 0.05")]
	protected float m_fDecalSize;

	[Attribute("60", UIWidgets.EditBox, "Decal lifetime in seconds")]
	protected float m_fDecalLifetime;

	protected IEntity m_Owner;
	protected EventHandlerManagerComponent m_EventHandler;

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		m_Owner = owner;
		Print("[SprayRaycast] OnPostInit on: " + owner.ToString(), LogLevel.NORMAL);

		// Hook weapon fire via EventHandlerManagerComponent
		m_EventHandler = EventHandlerManagerComponent.Cast(owner.FindComponent(EventHandlerManagerComponent));
		if (m_EventHandler)
		{
			m_EventHandler.RegisterScriptHandler("OnMuzzleFired", this, OnMuzzleFired);
			Print("[SprayRaycast] Registered OnMuzzleFired handler via EventHandlerManagerComponent", LogLevel.NORMAL);
		}
		else
		{
			Print("[SprayRaycast] No EventHandlerManagerComponent - trying deferred SCR_MuzzleEffectComponent hook", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(DeferredInit, 500, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void DeferredInit()
	{
		if (!m_Owner)
			return;

		Print("[SprayRaycast] DeferredInit running...", LogLevel.NORMAL);

		SCR_MuzzleEffectComponent muzzleEffect = SCR_MuzzleEffectComponent.Cast(
			m_Owner.FindComponent(SCR_MuzzleEffectComponent));

		if (muzzleEffect)
		{
			muzzleEffect.GetOnWeaponFired().Insert(OnWeaponFiredInvoker);
			Print("[SprayRaycast] Hooked via SCR_MuzzleEffectComponent invoker", LogLevel.NORMAL);
		}
		else
		{
			Print("[SprayRaycast] FAILED: No SCR_MuzzleEffectComponent found!", LogLevel.ERROR);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Called by EventHandlerManagerComponent
	protected void OnMuzzleFired(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		Print("[SprayRaycast] >>> OnMuzzleFired event! <<<", LogLevel.NORMAL);
		DoSprayRaycast();
	}

	//------------------------------------------------------------------------------------------------
	// Called by SCR_MuzzleEffectComponent invoker
	protected void OnWeaponFiredInvoker(IEntity effectEntity, BaseMuzzleComponent muzzle, IEntity projectileEntity)
	{
		Print("[SprayRaycast] >>> OnWeaponFiredInvoker! <<<", LogLevel.NORMAL);

		// Delete the projectile so there's no bullet impact
		if (projectileEntity)
		{
			Print("[SprayRaycast] Deleting projectile", LogLevel.NORMAL);
			delete projectileEntity;
		}

		DoSprayRaycast();
	}

	//------------------------------------------------------------------------------------------------
	protected void DoSprayRaycast()
	{
		IEntity charEntity = GetCharacterOwner();
		if (!charEntity)
		{
			Print("[SprayRaycast] No character owner!", LogLevel.WARNING);
			return;
		}

		CharacterControllerComponent charCtrl = CharacterControllerComponent.Cast(
			charEntity.FindComponent(CharacterControllerComponent));
		if (!charCtrl)
		{
			Print("[SprayRaycast] No CharacterControllerComponent!", LogLevel.ERROR);
			return;
		}

		CharacterAimingComponent aimComp = charCtrl.GetAimingComponent();
		if (!aimComp)
		{
			Print("[SprayRaycast] No AimingComponent!", LogLevel.ERROR);
			return;
		}

		vector aimDir = aimComp.GetAimingDirectionWorld();

		// Eye position
		vector charMat[4];
		charEntity.GetWorldTransform(charMat);
		vector eyePos = charMat[3] + vector.Up * 1.6;
		vector rayEnd = eyePos + aimDir * m_fMaxRange;

		World world = charEntity.GetWorld();
		if (!world)
			return;

		TraceParam trace = new TraceParam();
		trace.Start = eyePos;
		trace.End = rayEnd;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = charEntity;

		float hitFraction = world.TraceMove(trace, null);

		Print(string.Format("[SprayRaycast] Trace: fraction=%1 hitEnt=%2", hitFraction, trace.TraceEnt), LogLevel.NORMAL);

		if (hitFraction >= 1.0)
		{
			Print("[SprayRaycast] Nothing hit", LogLevel.NORMAL);
			return;
		}

		vector hitPos = eyePos + (rayEnd - eyePos) * hitFraction;
		IEntity hitEntity = trace.TraceEnt;

		if (hitEntity)
		{
			SpawnDecal(world, hitEntity, hitPos, aimDir);
		}
		else
		{
			Print("[SprayRaycast] Hit terrain but no entity ref", LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnDecal(World world, IEntity hitEntity, vector hitPos, vector aimDir)
	{
		if (!m_sDecalMaterial || m_sDecalMaterial.IsEmpty())
		{
			Print("[SprayRaycast] No decal material!", LogLevel.ERROR);
			return;
		}

		world.CreateDecal(hitEntity, hitPos, aimDir, 0.01, 0.5, 0, m_fDecalSize, 1.0, m_sDecalMaterial, m_fDecalLifetime, 0xFFFFFFFF);
		Print(string.Format("[SprayRaycast] DECAL SPAWNED at %1", hitPos), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity GetCharacterOwner()
	{
		IEntity parent = m_Owner.GetParent();
		while (parent)
		{
			CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
				parent.FindComponent(CharacterControllerComponent));
			if (ctrl)
				return parent;

			parent = parent.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(DeferredInit);
		super.OnDelete(owner);
	}
}
