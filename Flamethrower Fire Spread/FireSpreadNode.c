// FireSpreadNode.c
//
// Attach to a generic entity prefab (the "fire node").
// Plays a looping particle effect and repeatedly applies a damage prefab
// at its own position for its lifetime, then self-deletes.
//
// Multiplayer:
//   • Entity must have RplComponent set to Broadcast so it replicates to clients.
//   • Particle is spawned locally on every machine in EOnInit — no RPC needed.
//   • Damage prefab spawning and lifetime timer are server-only (IsProxy guard).

[ComponentEditorProps(category: "WW2Vehicles/Projectile", description: "Fire node: plays a looping particle effect and applies a damage prefab at its position.")]
class FireSpreadNodeClass : ScriptComponentClass {}

class FireSpreadNode : ScriptComponent
{

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Particle effect (.ptc) to display at this node.", params: "ptc")]
	protected ResourceName m_rParticleEffect;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Decal prefab (.et) spawned once when this node is placed.", params: "et")]
	protected ResourceName m_rDecalPrefab;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Damage prefab (.et) applied repeatedly at this node's position.", params: "et")]
	protected ResourceName m_rDamagePrefab;

	[Attribute("0.5", UIWidgets.EditBox, "Interval (seconds) between damage prefab applications.")]
	protected float m_fDamageInterval;

	[Attribute("8.0", UIWidgets.EditBox, "Lifetime (seconds) before this node deletes itself.")]
	protected float m_fTotalDuration;

	[Attribute("2.0", UIWidgets.EditBox, "Particle effect duration (seconds). Used to loop the effect. Match to your .ptc length.")]
	protected float m_fParticleLoopInterval;

	protected float m_fElapsed;
	protected bool  m_bDone;
	protected bool  m_bIsProxy;		// cached once in EOnInit — avoids FindComponent every frame
	protected ParticleEffectEntity m_Particle;
	protected ref Resource m_DamageRes;	// cached in EOnInit — avoids Resource.Load every damage tick
	protected ref array<IEntity> m_aDamageEnts = new array<IEntity>();

	// Weak reference to the spawner that created this node.
	// Used to notify it when this node dies so it can update its alive count.
	// Zeroed by the spawner's OnDelete to prevent dangling pointer.
	FireSpreadSpawner m_OwnerSpawner;
	protected bool m_bNotifiedDeath;

	void SetOwnerSpawner(FireSpreadSpawner spawner)
	{
		m_OwnerSpawner = spawner;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnInit(IEntity owner)
	{
		if (SCR_Global.IsEditMode())
			return;

		// Cache proxy state once — used in EOnFrame to avoid per-frame FindComponent.
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		m_bIsProxy = rpl && rpl.IsProxy();

		// Visuals — every machine.
		PlayParticle(owner);
		if (m_fParticleLoopInterval > 0)
			GetGame().GetCallqueue().CallLater(RestartParticle, m_fParticleLoopInterval * 1000, true);

		SpawnDecal(owner);

		// Proxies don't run lifetime or damage logic — stop wasting frame ticks on them.
		if (m_bIsProxy)
		{
			ClearEventMask(owner, EntityEvent.FRAME);
			return;
		}

		// Cache the damage prefab resource once — avoids filesystem hit every damage tick.
		if (!m_rDamagePrefab.IsEmpty())
		{
			m_DamageRes = Resource.Load(m_rDamagePrefab);
			if (!m_DamageRes || !m_DamageRes.IsValid())
			{
				Print("[FireSpreadNode] Failed to pre-load damage prefab: " + m_rDamagePrefab, LogLevel.ERROR);
				m_DamageRes = null;
			}
			else
				GetGame().GetCallqueue().CallLater(ApplyDamage, m_fDamageInterval * 1000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnFrame(IEntity owner, float timeSlice)
	{
		// m_bIsProxy is set in EOnInit; proxies have FRAME cleared so this never runs on them.
		if (m_bDone)
			return;

		m_fElapsed += timeSlice;
		if (m_fElapsed >= m_fTotalDuration)
		{
			m_bDone = true;
			GetGame().GetCallqueue().Remove(ApplyDamage);
			GetGame().GetCallqueue().Remove(RestartParticle);
			DeleteDamageEnts();
			NotifySpawnerDeath();
			SCR_EntityHelper.DeleteEntityAndChildren(owner);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyDamage()
	{
		IEntity owner = GetOwner();
		if (!owner || !m_DamageRes || !m_DamageRes.IsValid())
			return;

		vector transform[4];
		owner.GetWorldTransform(transform);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform     = transform;

		IEntity spawned = GetGame().SpawnEntityPrefab(m_DamageRes, owner.GetWorld(), params);
		if (spawned)
			m_aDamageEnts.Insert(spawned);
		else
			Print("[FireSpreadNode] SpawnEntityPrefab returned null for damage prefab.", LogLevel.ERROR);

		// Prune stale null entries so the array doesn't grow indefinitely.
		for (int i = m_aDamageEnts.Count() - 1; i >= 0; i--)
		{
			if (!m_aDamageEnts[i])
				m_aDamageEnts.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PlayParticle(IEntity owner)
	{
		if (m_rParticleEffect.IsEmpty())
			return;

		ParticleEffectEntitySpawnParams ptcParams = new ParticleEffectEntitySpawnParams();
		ptcParams.TransformMode = ETransformMode.WORLD;
		ptcParams.PlayOnSpawn   = true;
		ptcParams.Parent        = owner;
		ptcParams.AutoTransform = true;
		owner.GetWorldTransform(ptcParams.Transform);

		m_Particle = ParticleEffectEntity.SpawnParticleEffect(m_rParticleEffect, ptcParams);
	}

	//------------------------------------------------------------------------------------------------
	protected void SpawnDecal(IEntity owner)
	{
		if (m_rDecalPrefab.IsEmpty())
			return;

		Resource res = Resource.Load(m_rDecalPrefab);
		if (!res || !res.IsValid())
		{
			Print("[FireSpreadNode] Failed to load decal prefab: " + m_rDecalPrefab, LogLevel.ERROR);
			return;
		}

		vector transform[4];
		owner.GetWorldTransform(transform);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform     = transform;

		GetGame().SpawnEntityPrefab(res, owner.GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected void RestartParticle()
	{
		if (m_bDone)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Delete old particle before spawning new one to avoid orphans.
		if (m_Particle)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Particle);
			m_Particle = null;
		}

		PlayParticle(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void NotifySpawnerDeath()
	{
		if (m_bNotifiedDeath)
			return;
		m_bNotifiedDeath = true;
		if (m_OwnerSpawner)
			m_OwnerSpawner.OnNodeDied(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteDamageEnts()
	{
		foreach (IEntity ent : m_aDamageEnts)
		{
			if (ent)
				SCR_EntityHelper.DeleteEntityAndChildren(ent);
		}
		m_aDamageEnts.Clear();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(ApplyDamage);
		GetGame().GetCallqueue().Remove(RestartParticle);
		DeleteDamageEnts();

		// Safety net — in case the node was deleted externally rather than via EOnFrame lifetime.
		NotifySpawnerDeath();
	}
}
