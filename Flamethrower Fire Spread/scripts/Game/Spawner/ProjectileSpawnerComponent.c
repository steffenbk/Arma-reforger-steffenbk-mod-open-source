[ComponentEditorProps(category: "WW2Vehicles/Spawner", description: "Spawns a projectile prefab from this entity's origin and forward vector.")]
class ProjectileSpawnerComponentClass : ScriptComponentClass {}

class ProjectileSpawnerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Projectile .et prefab to spawn", params: "et")]
	protected ResourceName m_rProjectilePrefab;

	[Attribute("2.0", UIWidgets.EditBox, "Auto-fire interval in seconds. Set to 0 to disable auto-fire.")]
	protected float m_fFireInterval;

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// Server-only: skip setup on network proxies (clients)
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (rpl && rpl.IsProxy())
			return;

		if (m_fFireInterval > 0)
			GetGame().GetCallqueue().CallLater(Fire, m_fFireInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	// Public: call this from any other script to fire one projectile immediately.
	void Fire()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (!m_rProjectilePrefab || m_rProjectilePrefab.IsEmpty())
		{
			Print("[ProjectileSpawnerComponent] No prefab set — nothing to spawn.", LogLevel.WARNING);
			return;
		}

		Resource res = Resource.Load(m_rProjectilePrefab);
		if (!res || !res.IsValid())
		{
			Print("[ProjectileSpawnerComponent] Failed to load prefab: " + m_rProjectilePrefab, LogLevel.ERROR);
			return;
		}

		// Build spawn transform from owner world transform
		vector transform[4];
		owner.GetWorldTransform(transform);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform = transform;

		IEntity spawned = GetGame().SpawnEntityPrefab(res, owner.GetWorld(), params);
		if (!spawned)
		{
			Print("[ProjectileSpawnerComponent] SpawnEntityPrefab returned null.", LogLevel.ERROR);
			return;
		}

	}

	//------------------------------------------------------------------------------------------------
	void ~ProjectileSpawnerComponent()
	{
		// Cancel the repeating timer so it doesn't fire after deletion
		GetGame().GetCallqueue().Remove(Fire);
	}
}
