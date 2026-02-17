//------------------------------------------------------------------------------------------------
//! Vehicle Vegetation Destroyer Component
//! Destroys vegetation entities when the vehicle collides with them or gets close
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Vehicle", description: "Destroys vegetation on collision")]
class SCR_VehicleVegetationDestroyerComponentClass : ScriptComponentClass
{
}

class PendingVegetationDestruction
{
	IEntity m_Entity;
	float m_fDestructionTime;
}

class SCR_VehicleVegetationDestroyerComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable vegetation destruction")]
	private bool m_bEnabled;

	[Attribute("5.0", UIWidgets.Slider, "Minimum vehicle speed to destroy vegetation (km/h)", "0 50 1")]
	private float m_fMinSpeedKmh;

	[Attribute("15.0", UIWidgets.Slider, "Minimum speed to destroy trees (km/h)", "0 100 1")]
	private float m_fMinSpeedTrees;

	[Attribute("20.0", UIWidgets.Slider, "Minimum speed to destroy rocks (km/h)", "0 100 1")]
	private float m_fMinSpeedRocks;

	[Attribute("0.5", UIWidgets.Slider, "Delay before vegetation disappears (seconds)", "0 5.0 0.1")]
	private float m_fDestructionDelay;

	[Attribute("1", UIWidgets.CheckBox, "Destroy trees")]
	private bool m_bDestroyTrees;

	[Attribute("1", UIWidgets.CheckBox, "Destroy bushes")]
	private bool m_bDestroyBushes;

	[Attribute("1", UIWidgets.CheckBox, "Destroy grass")]
	private bool m_bDestroyGrass;

	[Attribute("1", UIWidgets.CheckBox, "Destroy plants")]
	private bool m_bDestroyPlants;

	[Attribute("0", UIWidgets.CheckBox, "Destroy rocks")]
	private bool m_bDestroyRocks;

	[Attribute("", UIWidgets.EditBox, "Include specific prefabs by name (comma-separated)")]
	private string m_sIncludedPrefabs;

	[Attribute("", UIWidgets.EditBox, "Exclude specific prefabs by name (comma-separated)")]
	private string m_sExcludedPrefabs;

	[Attribute("0", UIWidgets.CheckBox, "Enable debug messages")]
	private bool m_bDebug;

	[Attribute("1.0", UIWidgets.Slider, "Proximity check radius (meters)", "0.1 3.0 0.1")]
	private float m_fProximityRadius;

	[Attribute("0.1", UIWidgets.Slider, "Time between proximity checks (seconds)", "0.05 1.0 0.05")]
	private float m_fCheckInterval;

	protected Vehicle m_Vehicle;
	protected VehicleWheeledSimulation m_Simulation;
	protected Physics m_VehiclePhysics;
	protected BaseWorld m_World;

	protected ref array<string> m_aIncludedPrefabs;
	protected ref array<string> m_aExcludedPrefabs;
	protected ref array<ref PendingVegetationDestruction> m_aPendingDestructions;
	protected ref set<IEntity> m_sMarkedEntities;
	protected ref set<IEntity> m_sRecentlyChecked;
	protected ref array<IEntity> m_aProximityQueryResults;

	protected float m_fLastCheckTime;
	protected float m_fLastCleanupTime;
	protected int m_iDestroyedCount;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		m_Vehicle = Vehicle.Cast(owner);
		if (!m_Vehicle)
		{
			Print("VehicleVegetationDestroyer: Must be attached to Vehicle!", LogLevel.ERROR);
			return;
		}

		m_Simulation = VehicleWheeledSimulation.Cast(m_Vehicle.FindComponent(VehicleWheeledSimulation));
		m_VehiclePhysics = m_Vehicle.GetPhysics();
		m_World = GetGame().GetWorld();

		ParseIncludedPrefabs();
		ParseExcludedPrefabs();

		m_aPendingDestructions = {};
		m_sMarkedEntities = new set<IEntity>();
		m_sRecentlyChecked = new set<IEntity>();
		m_aProximityQueryResults = {};
		m_fLastCheckTime = 0;
		m_fLastCleanupTime = 0;
		m_iDestroyedCount = 0;

		if (m_bEnabled)
		{
			SetEventMask(owner, EntityEvent.CONTACT);
			SetEventMask(owner, EntityEvent.FRAME);

			if (m_bDebug)
				Print("VehicleVegetationDestroyer initialized", LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		if (!m_bEnabled || !other)
			return;

		// Check speed
		float speed = 0;
		if (m_Simulation)
			speed = Math.AbsFloat(m_Simulation.GetSpeedKmh());

		if (speed < m_fMinSpeedKmh)
			return;

		if (m_sMarkedEntities.Contains(other) || m_sRecentlyChecked.Contains(other))
			return;

		m_sRecentlyChecked.Insert(other);

		if (ShouldDestroyEntity(other))
		{
			float currentTime = m_World.GetWorldTime() * 0.001;
			MarkForDestruction(other, currentTime);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bEnabled)
			return;

		float currentTime = m_World.GetWorldTime() * 0.001;

		ProcessPendingDestructions(currentTime);

		if (currentTime - m_fLastCheckTime >= m_fCheckInterval)
		{
			m_fLastCheckTime = currentTime;
			CheckProximityVegetation(currentTime);
		}

		if (currentTime - m_fLastCleanupTime > 1.0)
		{
			m_sRecentlyChecked.Clear();
			m_fLastCleanupTime = currentTime;
		}
	}

	//------------------------------------------------------------------------------------------------
	void CheckProximityVegetation(float currentTime)
	{
		float speed = 0;
		if (m_Simulation)
			speed = Math.AbsFloat(m_Simulation.GetSpeedKmh());

		if (speed < m_fMinSpeedKmh)
			return;

		vector pos = m_Vehicle.GetOrigin();
		m_aProximityQueryResults.Clear();
		m_World.QueryEntitiesBySphere(pos, m_fProximityRadius, QueryProximityCallback);

		foreach (IEntity entity : m_aProximityQueryResults)
		{
			if (!entity || entity == m_Vehicle)
				continue;

			if (m_sMarkedEntities.Contains(entity) || m_sRecentlyChecked.Contains(entity))
				continue;

			if (!IsNonCollisionVegetation(entity))
				continue;

			m_sRecentlyChecked.Insert(entity);

			if (ShouldDestroyEntity(entity))
				MarkForDestruction(entity, currentTime);
		}
	}

	bool QueryProximityCallback(IEntity entity)
	{
		if (!entity || entity == m_Vehicle)
			return true;

		m_aProximityQueryResults.Insert(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsNonCollisionVegetation(IEntity entity)
	{
		if (!entity)
			return false;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return false;

		string prefabPath = prefabData.GetPrefabName();
		return IsBush(prefabPath) || IsGrass(prefabPath) || IsPlant(prefabPath);
	}

	void MarkForDestruction(IEntity entity, float currentTime)
	{
		if (!entity)
			return;

		m_sMarkedEntities.Insert(entity);

		PendingVegetationDestruction pending = new PendingVegetationDestruction();
		pending.m_Entity = entity;
		pending.m_fDestructionTime = currentTime + m_fDestructionDelay;
		m_aPendingDestructions.Insert(pending);
	}

	void ProcessPendingDestructions(float currentTime)
	{
		for (int i = m_aPendingDestructions.Count() - 1; i >= 0; i--)
		{
			PendingVegetationDestruction pending = m_aPendingDestructions[i];
			if (!pending)
			{
				m_aPendingDestructions.RemoveOrdered(i);
				continue;
			}

			if (currentTime >= pending.m_fDestructionTime)
			{
				if (pending.m_Entity)
				{
					DestroyEntity(pending.m_Entity);
					m_sMarkedEntities.RemoveItem(pending.m_Entity);
				}
				m_aPendingDestructions.RemoveOrdered(i);
			}
		}
	}

	bool ShouldDestroyEntity(IEntity entity)
	{
		if (!entity)
			return false;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return false;

		string prefabPath = prefabData.GetPrefabName();
		if (prefabPath == "")
			return false;

		// Speed check
		float currentSpeed = 0;
		if (m_Simulation)
			currentSpeed = Math.AbsFloat(m_Simulation.GetSpeedKmh());

		// Trees
		if (m_bDestroyTrees && IsTree(prefabPath))
			return currentSpeed >= m_fMinSpeedTrees;

		// Rocks
		if (m_bDestroyRocks && IsRock(prefabPath))
			return currentSpeed >= m_fMinSpeedRocks;

		// Bushes, grass, plants
		if (m_bDestroyBushes && IsBush(prefabPath)) return true;
		if (m_bDestroyGrass && IsGrass(prefabPath)) return true;
		if (m_bDestroyPlants && IsPlant(prefabPath)) return true;

		return false;
	}

	void DestroyEntity(IEntity entity)
	{
		if (!entity)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		m_iDestroyedCount++;
	}

	//------------------------------------------------------------------------------------------------
	// Static helper functions
	//------------------------------------------------------------------------------------------------
	static bool IsTree(string prefabPath)
	{
		return prefabPath.Contains("Tree") || prefabPath.Contains("Pine") || prefabPath.Contains("Oak") || prefabPath.Contains("Birch");
	}

	static bool IsBush(string prefabPath)
	{
		return prefabPath.Contains("Bush") || prefabPath.Contains("Shrub") || prefabPath.Contains("Hedge");
	}

	static bool IsGrass(string prefabPath)
	{
		return prefabPath.Contains("Grass") || prefabPath.Contains("Weed") || prefabPath.Contains("Flower");
	}

	static bool IsPlant(string prefabPath)
	{
		return prefabPath.Contains("Plant") || prefabPath.Contains("Fern") || prefabPath.Contains("Crop");
	}

	static bool IsRock(string prefabPath)
	{
		return prefabPath.Contains("Rock") || prefabPath.Contains("Stone") || prefabPath.Contains("Boulder");
	}

	//------------------------------------------------------------------------------------------------
	void ParseIncludedPrefabs()
	{
		m_aIncludedPrefabs = {};
		if (m_sIncludedPrefabs == "")
			return;

		array<string> prefabs = {};
		m_sIncludedPrefabs.Split(",", prefabs, true);

		foreach (string prefab : prefabs)
		{
			string trimmedPrefab = prefab;
			trimmedPrefab.Trim();
			if (trimmedPrefab != "")
				m_aIncludedPrefabs.Insert(trimmedPrefab);
		}
	}

	void ParseExcludedPrefabs()
	{
		m_aExcludedPrefabs = {};
		if (m_sExcludedPrefabs == "")
			return;

		array<string> prefabs = {};
		m_sExcludedPrefabs.Split(",", prefabs, true);

		foreach (string prefab : prefabs)
		{
			string trimmedPrefab = prefab;
			trimmedPrefab.Trim();
			if (trimmedPrefab != "")
				m_aExcludedPrefabs.Insert(trimmedPrefab);
		}
	}
}
