//------------------------------------------------------------------------------------------------
//! Enhanced Vehicle Spline Track Spawner Component
//! Version: 4.0 - Tiered construction with fuel consumption
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Road surface configuration with tier requirements and fuel consumption
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_RoadTrackConfig : Managed
{
	[Attribute("Road", UIWidgets.EditBox, "Name for this road config")]
	string m_sConfigName;
	
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Road prefab to spawn", "et")]
	ResourceName m_sRoadPrefab;
	
	[Attribute("2.0", UIWidgets.Slider, "Road width (meters)", "0.5 10.0 0.1")]
	float m_fRoadWidth;
	
	[Attribute("2.0", UIWidgets.Slider, "Road spawn delay (seconds)", "0 10.0 0.5")]
	float m_fRoadSpawnDelay;
	
	[Attribute("0.5", UIWidgets.Slider, "Min distance between spline points (meters)", "0.05 2.0 0.1")]
	float m_fMinPointDistance;
	
	[Attribute("10.0", UIWidgets.Slider, "Max track segment length (meters)", "1.0 50.0 1.0")]
	float m_fMaxSegmentLength;
	
	[Attribute("5.0", UIWidgets.Slider, "Min distance between road segments (meters)", "1.0 50.0 1.0")]
	float m_fMinRoadSegmentDistance;
	
	[Attribute("5.0", UIWidgets.Slider, "Min time between segments (seconds)", "0 30.0 1.0")]
	float m_fMinSegmentInterval;
	
	[Attribute("0 -0.05 0", UIWidgets.Coords, "Track offset from ground", "inf inf inf purpose=coords space=entity")]
	vector m_vTrackOffset;
	
	[Attribute("0 0 0", UIWidgets.Coords, "Road position offset", "inf inf inf purpose=coords space=entity")]
	vector m_vPositionOffset;
	
	[Attribute("0 0 0", UIWidgets.Coords, "Road rotation offset", "inf inf inf purpose=angles space=entity")]
	vector m_vRotationOffset;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable this road config")]
	bool m_bEnabled;
	
	[Attribute("0", UIWidgets.CheckBox, "Only spawn when reversing")]
	bool m_bOnlyWhenReversing;
	
	[Attribute("0", UIWidgets.CheckBox, "Use terrain filtering")]
	bool m_bUseTerrainFiltering;
	
	[Attribute("", UIWidgets.EditBox, "Allowed terrain types (comma-separated)")]
	string m_sAllowedTerrainTypes;
	
	[Attribute("", UIWidgets.EditBox, "Blocked terrain types (comma-separated)")]
	string m_sBlockedTerrainTypes;
	
	[Attribute("1", UIWidgets.CheckBox, "Allow natural terrain")]
	bool m_bAllowNaturalTerrain;
	
	[Attribute("0", UIWidgets.CheckBox, "Allow roads")]
	bool m_bAllowRoads;
	
	// Tier system
	[Attribute("0", UIWidgets.Slider, "Tier level (0=base, higher=upgrades)", "0 10 1")]
	int m_iTierLevel;
	
	[Attribute("5.0", UIWidgets.Slider, "Search radius for base road (meters)", "1.0 20.0 1.0")]
	float m_fBaseRoadSearchRadius;
	
	// Fuel consumption
	[Attribute("0", UIWidgets.Slider, "Fuel consumption per meter (liters/m)", "0 2.0 0.01")]
	float m_fFuelConsumptionPerMeter;
	
	[Attribute("1.0", UIWidgets.Slider, "Fuel consumption multiplier", "0.1 5.0 0.1")]
	float m_fFuelConsumptionMultiplier;
	
	[Attribute("0", UIWidgets.CheckBox, "Requires fuel to construct")]
	bool m_bRequiresFuel;
	
	[Attribute("0", UIWidgets.CheckBox, "Ignore fuel requirement (spawn even if empty)")]
	bool m_bIgnoreFuelRequirement;
	
	[Attribute("", UIWidgets.EditBox, "Description for UI")]
	string m_sDescription;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_PrefabTrackConfig : Managed
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Deformation prefab", "et")]
	ResourceName m_sDeformationPrefab;
	
	[Attribute("1.0", UIWidgets.Slider, "Prefab spawn interval (meters)", "0.5 5.0 0.1")]
	float m_fPrefabSpawnInterval;
	
	[Attribute("0.8", UIWidgets.Slider, "Min distance between prefabs (meters)", "0.1 5.0 0.1")]
	float m_fMinPrefabDistance;
	
	[Attribute("0 0 0", UIWidgets.Coords, "Prefab position offset")]
	vector m_vPositionOffset;
	
	[Attribute("0 0 0", UIWidgets.Coords, "Prefab rotation offset")]
	vector m_vRotationOffset;
	
	[Attribute("1", UIWidgets.CheckBox, "Align prefabs to terrain")]
	bool m_bAlignToTerrain;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable prefab generation")]
	bool m_bEnabled;
	
	[Attribute("0", UIWidgets.CheckBox, "Ignore fuel requirement for prefabs")]
	bool m_bIgnoreFuelRequirement;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_SplineTrackConfig : Managed
{
	[Attribute("1", UIWidgets.CheckBox, "Stop tracking when idle")]
	bool m_bStopWhenIdle;
	
	[Attribute("3.0", UIWidgets.Slider, "Idle detection time (seconds)", "1.0 10.0 0.5")]
	float m_fIdleTime;
	
	[Attribute("2.0", UIWidgets.Slider, "Min speed to track (km/h)", "0 20 0.5")]
	float m_fMinSpeed;
}

//------------------------------------------------------------------------------------------------
//! Spawned road tracking
//------------------------------------------------------------------------------------------------
class SCR_SpawnedRoadData
{
	vector m_vPosition;
	int m_iTierLevel;
	float m_fTimestamp;
	
	void SCR_SpawnedRoadData(vector pos, int tier, float timestamp)
	{
		m_vPosition = pos;
		m_iTierLevel = tier;
		m_fTimestamp = timestamp;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Vehicle", description: "Creates tiered roads with fuel consumption")]
class SCR_VehicleSplineTrackSpawnerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class SCR_VehicleSplineTrackSpawnerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Auto, "Global tracking configuration")]
	protected ref SCR_SplineTrackConfig m_GlobalTrackConfig;
	
	[Attribute("", UIWidgets.Auto, "Road configurations")]
	protected ref array<ref SCR_RoadTrackConfig> m_aRoadConfigs;
	
	[Attribute("", UIWidgets.Auto, "Construction road configurations")]
	protected ref array<ref SCR_RoadTrackConfig> m_aConstructionRoadConfigs;
	
	[Attribute("", UIWidgets.Auto, "Prefab configuration")]
	protected ref SCR_PrefabTrackConfig m_PrefabConfig;
	
	[Attribute("0", UIWidgets.CheckBox, "Enable debug messages")]
	protected bool m_bDebug;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable fuel consumption system")]
	protected bool m_bUseFuelConsumption;
	
	[Attribute("1", UIWidgets.CheckBox, "Disable construction mode when reversing")]
	protected bool m_bDisableConstructionWhenReversing;
	
	protected Vehicle m_Vehicle;
	protected VehicleWheeledSimulation m_Simulation;
	protected SCR_VehicleTerrainDetectorComponent m_TerrainDetector;
	protected SCR_FuelConsumptionComponent m_FuelComponent;
	protected BaseWorld m_World;
	protected ref array<vector> m_aTrackPoints;
	protected bool m_bTrackingEnabled;
	protected ref array<ref DelayedPrefabSpawn> m_aPendingPrefabSpawns;
	protected ref array<ref DelayedRoadSpawn> m_aPendingRoadSpawns;
	protected ref array<vector> m_aSpawnedPrefabPositions;
	protected ref map<string, vector> m_mLastRoadSpawnPositions;
	protected ref map<string, float> m_mLastSegmentTimes;
	protected ref array<ref SCR_SpawnedRoadData> m_aSpawnedRoads;
	
	protected float m_fLastSegmentTime;
	protected float m_fLastMovementTime;
	protected vector m_vLastPosition;
	protected bool m_bWasAirborne;
	protected bool m_bRoadSpawnedSinceLanding;
	protected bool m_bConstructionMode;
	protected int m_iSelectedConstructionConfigIndex;
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
		
		m_Vehicle = Vehicle.Cast(owner);
		if (!m_Vehicle)
		{
			Print("VehicleSplineTrackSpawner: Must be attached to Vehicle!", LogLevel.ERROR);
			return;
		}
		
		m_Simulation = VehicleWheeledSimulation.Cast(m_Vehicle.FindComponent(VehicleWheeledSimulation));
		if (!m_Simulation)
		{
			Print("VehicleSplineTrackSpawner: Missing VehicleWheeledSimulation!", LogLevel.ERROR);
			return;
		}
		
		m_TerrainDetector = SCR_VehicleTerrainDetectorComponent.Cast(m_Vehicle.FindComponent(SCR_VehicleTerrainDetectorComponent));
		m_FuelComponent = SCR_FuelConsumptionComponent.Cast(m_Vehicle.FindComponent(SCR_FuelConsumptionComponent));
		
		m_World = GetGame().GetWorld();
		m_aTrackPoints = {};
		m_bTrackingEnabled = true;
		m_aPendingPrefabSpawns = {};
		m_aPendingRoadSpawns = {};
		m_aSpawnedPrefabPositions = {};
		m_mLastRoadSpawnPositions = new map<string, vector>();
		m_mLastSegmentTimes = new map<string, float>();
		m_aSpawnedRoads = {};
		m_fLastSegmentTime = 0;
		m_fLastMovementTime = 0;
		m_vLastPosition = "0 0 0";
		m_bWasAirborne = false;
		m_bRoadSpawnedSinceLanding = true;
		m_bConstructionMode = false;
		m_iSelectedConstructionConfigIndex = 0;
		
		SetEventMask(owner, EntityEvent.FRAME);
		
		if (m_bDebug)
			Print("VehicleSplineTrackSpawner initialized", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_Vehicle || !m_Simulation || !m_World || !m_bTrackingEnabled)
			return;
		
		// Check for reverse gear and disable construction mode if needed
		if (m_bDisableConstructionWhenReversing && m_bConstructionMode)
		{
			VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
			if (wheeledSim)
			{
				int currentGear = wheeledSim.GetGear();
				if (currentGear == 0) // Gear 0 = Reverse in v1.4
				{
					if (m_bDebug)
						Print("Reversing detected - disabling construction mode", LogLevel.NORMAL);
					
					SetConstructionMode(false);
				}
			}
		}
		
		if (m_GlobalTrackConfig && m_GlobalTrackConfig.m_bStopWhenIdle)
			CheckIdleState();
		
		RecordPosition();
		ProcessDelayedPrefabSpawns();
		ProcessDelayedRoadSpawns();
	}
	
	//------------------------------------------------------------------------------------------------
	SCR_RoadTrackConfig GetSelectedConstructionConfig()
	{
		if (!m_aConstructionRoadConfigs || m_aConstructionRoadConfigs.Count() == 0)
			return null;
		
		if (m_iSelectedConstructionConfigIndex < 0 || m_iSelectedConstructionConfigIndex >= m_aConstructionRoadConfigs.Count())
			m_iSelectedConstructionConfigIndex = 0;
		
		return m_aConstructionRoadConfigs[m_iSelectedConstructionConfigIndex];
	}
	
	//------------------------------------------------------------------------------------------------
	void CycleConstructionConfig(bool forward = true)
	{
		if (!m_aConstructionRoadConfigs || m_aConstructionRoadConfigs.Count() == 0)
			return;
		
		if (forward)
		{
			m_iSelectedConstructionConfigIndex++;
			if (m_iSelectedConstructionConfigIndex >= m_aConstructionRoadConfigs.Count())
				m_iSelectedConstructionConfigIndex = 0;
		}
		else
		{
			m_iSelectedConstructionConfigIndex--;
			if (m_iSelectedConstructionConfigIndex < 0)
				m_iSelectedConstructionConfigIndex = m_aConstructionRoadConfigs.Count() - 1;
		}
		
		SCR_RoadTrackConfig selectedConfig = GetSelectedConstructionConfig();
		if (m_bDebug && selectedConfig)
			PrintFormat("Selected: '%1' (Tier %2)", selectedConfig.m_sConfigName, selectedConfig.m_iTierLevel);
	}
	
	//------------------------------------------------------------------------------------------------
	int GetSelectedConstructionConfigIndex()
	{
		return m_iSelectedConstructionConfigIndex;
	}
	
	//------------------------------------------------------------------------------------------------
	int GetConstructionConfigCount()
	{
		if (!m_aConstructionRoadConfigs)
			return 0;
		return m_aConstructionRoadConfigs.Count();
	}
	
	//------------------------------------------------------------------------------------------------
	bool HasRequiredBaseRoad(vector position, SCR_RoadTrackConfig config)
	{
		// Tier 0 roads don't need a base
		if (!config || config.m_iTierLevel == 0)
			return true;
		
		if (!m_aSpawnedRoads || m_aSpawnedRoads.Count() == 0)
			return false;
		
		int requiredTier = config.m_iTierLevel - 1;
		float searchRadius = config.m_fBaseRoadSearchRadius;
		
		foreach (SCR_SpawnedRoadData roadData : m_aSpawnedRoads)
		{
			if (!roadData)
				continue;
			
			if (roadData.m_iTierLevel != requiredTier)
				continue;
			
			float distance = vector.Distance(position, roadData.m_vPosition);
			if (distance <= searchRadius)
				return true;
		}
		
		if (m_bDebug)
			PrintFormat("'%1' (Tier %2) needs Tier %3 road within %.1fm", config.m_sConfigName, config.m_iTierLevel, requiredTier, searchRadius);
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	bool CanAffordRoadConstruction(SCR_RoadTrackConfig config, float segmentLength)
	{
		if (!config || !config.m_bRequiresFuel || !m_bUseFuelConsumption)
			return true;
		
		// Bypass fuel check if ignore flag is set
		if (config.m_bIgnoreFuelRequirement)
			return true;
		
		if (!m_FuelComponent)
			return true;
		
		// Check total available fuel across all tanks
		float requiredFuel = segmentLength * config.m_fFuelConsumptionPerMeter * config.m_fFuelConsumptionMultiplier;
		float totalAvailableFuel = GetTotalAvailableFuel();
		
		return totalAvailableFuel >= requiredFuel;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetTotalAvailableFuel()
	{
		if (!m_FuelComponent)
			return 0;
		
		IEntity owner = GetOwner();
		if (!owner)
			return 0;
		
		FuelManagerComponent fuelManager = FuelManagerComponent.Cast(owner.FindComponent(FuelManagerComponent));
		if (!fuelManager)
			return 0;
		
		array<BaseFuelNode> fuelNodes = {};
		fuelManager.GetFuelNodesList(fuelNodes);
		
		float totalFuel = 0;
		foreach (BaseFuelNode node : fuelNodes)
		{
			if (node)
				totalFuel += node.GetFuel();
		}
		
		return totalFuel;
	}
	
	//------------------------------------------------------------------------------------------------
	void ConsumeFuelForRoad(SCR_RoadTrackConfig config, float segmentLength)
	{
		if (!config || !config.m_bRequiresFuel || !m_bUseFuelConsumption)
			return;
		
		// Don't consume fuel if ignore flag is set
		if (config.m_bIgnoreFuelRequirement)
			return;
		
		if (!m_FuelComponent)
			return;
		
		IEntity owner = GetOwner();
		if (!owner)
			return;
		
		FuelManagerComponent fuelManager = FuelManagerComponent.Cast(owner.FindComponent(FuelManagerComponent));
		if (!fuelManager)
			return;
		
		float fuelCost = segmentLength * config.m_fFuelConsumptionPerMeter * config.m_fFuelConsumptionMultiplier;
		float remainingCost = fuelCost;
		float totalConsumed = 0;
		
		// Get all fuel tanks
		array<BaseFuelNode> fuelNodes = {};
		fuelManager.GetFuelNodesList(fuelNodes);
		
		// Consume from current tank first, then others
		BaseFuelNode currentTank = m_FuelComponent.GetCurrentFuelTank();
		if (currentTank && remainingCost > 0)
		{
			float availableInCurrent = currentTank.GetFuel();
			float toConsume = Math.Min(availableInCurrent, remainingCost);
			currentTank.SetFuel(Math.Max(0, availableInCurrent - toConsume));
			remainingCost -= toConsume;
			totalConsumed += toConsume;
		}
		
		// If still need more fuel, drain from other tanks
		if (remainingCost > 0)
		{
			foreach (BaseFuelNode node : fuelNodes)
			{
				if (!node || node == currentTank)
					continue;
				
				float availableInNode = node.GetFuel();
				if (availableInNode <= 0)
					continue;
				
				float toConsume = Math.Min(availableInNode, remainingCost);
				node.SetFuel(Math.Max(0, availableInNode - toConsume));
				remainingCost -= toConsume;
				totalConsumed += toConsume;
				
				if (remainingCost <= 0)
					break;
			}
		}
		
		if (m_bDebug)
		{
			if (totalConsumed < fuelCost)
				PrintFormat("Consumed %.2fL/%.2fL for %.1fm '%1' (partial)", totalConsumed, fuelCost, segmentLength, config.m_sConfigName);
			else
				PrintFormat("Consumed %.2fL for %.1fm '%1'", totalConsumed, segmentLength, config.m_sConfigName);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void CheckIdleState()
	{
		if (!m_GlobalTrackConfig)
			return;
		
		float speed = m_Simulation.GetSpeedKmh();
		vector currentPos = m_Vehicle.GetOrigin();
		float currentTime = m_World.GetWorldTime() * 0.001;
		
		if (speed < m_GlobalTrackConfig.m_fMinSpeed)
		{
			if (m_fLastMovementTime == 0)
			{
				m_fLastMovementTime = currentTime;
				m_vLastPosition = currentPos;
			}
			else
			{
				float idleDuration = currentTime - m_fLastMovementTime;
				float distanceMoved = vector.Distance(currentPos, m_vLastPosition);
				
				if (idleDuration >= m_GlobalTrackConfig.m_fIdleTime && distanceMoved < 1.0)
				{
					if (m_aTrackPoints && m_aTrackPoints.Count() >= 2)
						CreateSplineFromPoints();
					
					m_fLastMovementTime = 0;
				}
			}
		}
		else
		{
			m_fLastMovementTime = 0;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void RecordPosition()
	{
		int leftWheelIndex = 2;
		int rightWheelIndex = 3;
		
		SCR_RoadTrackConfig primaryConfig = GetPrimaryRoadConfig();
		if (!primaryConfig)
			return;
		
		bool hasContact = false;
		int wheelCount = m_Simulation.WheelCount();
		for (int i = 0; i < wheelCount; i++)
		{
			if (m_Simulation.WheelHasContact(i))
			{
				hasContact = true;
				break;
			}
		}
		
		if (!hasContact)
		{
			if (m_aTrackPoints && m_aTrackPoints.Count() > 0)
				m_aTrackPoints.Clear();
			
			if (!m_bWasAirborne)
			{
				m_bWasAirborne = true;
				m_bRoadSpawnedSinceLanding = false;
			}
			return;
		}
		
		if (m_bWasAirborne)
			m_bWasAirborne = false;
		
		if (!m_Simulation.WheelHasContact(leftWheelIndex) || !m_Simulation.WheelHasContact(rightWheelIndex))
			return;
		
		vector leftWheelPos = m_Simulation.WheelGetContactPosition(leftWheelIndex);
		vector rightWheelPos = m_Simulation.WheelGetContactPosition(rightWheelIndex);
		vector centerPos = (leftWheelPos + rightWheelPos) * 0.5;
		
		vector vehicleTransform[4];
		m_Vehicle.GetWorldTransform(vehicleTransform);
		
		vector worldOffset = vehicleTransform[0] * primaryConfig.m_vTrackOffset[0] +
							 vehicleTransform[1] * primaryConfig.m_vTrackOffset[1] +
							 vehicleTransform[2] * primaryConfig.m_vTrackOffset[2];
		centerPos = centerPos + worldOffset;
		
		if (m_aTrackPoints.Count() == 0)
		{
			m_aTrackPoints.Insert(centerPos);
			return;
		}
		
		vector lastPoint = m_aTrackPoints[m_aTrackPoints.Count() - 1];
		float distance = vector.Distance(centerPos, lastPoint);
		
		if (distance < primaryConfig.m_fMinPointDistance)
			return;
		
		m_aTrackPoints.Insert(centerPos);
		
		float totalLength = CalculatePathLength(m_aTrackPoints);
		if (totalLength >= primaryConfig.m_fMaxSegmentLength)
			CreateSplineFromPoints();
	}
	
	//------------------------------------------------------------------------------------------------
	float CalculatePathLength(array<vector> points)
	{
		if (!points || points.Count() < 2)
			return 0;
		
		float totalLength = 0;
		for (int i = 1; i < points.Count(); i++)
			totalLength += vector.Distance(points[i], points[i - 1]);
		
		return totalLength;
	}
	
	//------------------------------------------------------------------------------------------------
	void CreateSplineFromPoints()
	{
		if (!m_aTrackPoints || m_aTrackPoints.Count() < 2)
			return;
		
		array<vector> splinePoints = {};
		foreach (vector pt : m_aTrackPoints)
			splinePoints.Insert(pt);
		
		vector startPos = splinePoints[0];
		vector endPos = splinePoints[splinePoints.Count() - 1];
		vector direction = (endPos - startPos).Normalized();
		vector segmentCenter = (startPos + endPos) * 0.5;
		
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		vector up = "0 1 0";
		vector right = (up * direction).Normalized();
		vector realUp = (direction * right).Normalized();
		
		spawnParams.Transform[0] = right;
		spawnParams.Transform[1] = realUp;
		spawnParams.Transform[2] = direction;
		spawnParams.Transform[3] = startPos;
		
		IEntity splineEntity = GetGame().SpawnEntity(SplineShapeEntity, m_World, spawnParams);
		if (!splineEntity)
			return;
		
		SplineShapeEntity spline = SplineShapeEntity.Cast(splineEntity);
		if (!spline)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(splineEntity);
			return;
		}
		
		array<vector> localPoints = {};
		foreach (vector worldPt : splinePoints)
		{
			vector localPt = spline.CoordToLocal(worldPt);
			localPoints.Insert(localPt);
		}
		
		spline.SetPointsSpline(localPoints, null, 0.3, 0.3, 0);
		splineEntity.SetName("vehicle_track_segment_spline");
		
		float segmentLength = CalculatePathLength(splinePoints);
		bool anyRoadWillSpawn = false;
		bool tierRequirementFailed = false;
		array<ref SCR_RoadTrackConfig> activeRoadConfigs = GetActiveRoadConfigs();
		float currentTime = m_World.GetWorldTime() * 0.001;
		
		// Check if reversing - prevent construction mode spawning
		bool isReversing = false;
		VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
		if (wheeledSim)
			isReversing = (wheeledSim.GetGear() == 0);
		
		// Block construction mode spawning when reversing if option enabled
		if (m_bDisableConstructionWhenReversing && m_bConstructionMode && isReversing)
		{
			if (m_bDebug)
				Print("Construction mode blocked while reversing", LogLevel.NORMAL);
			
			// Clear points and return without spawning
			vector lastPoint = m_aTrackPoints[m_aTrackPoints.Count() - 1];
			m_aTrackPoints.Clear();
			m_aTrackPoints.Insert(lastPoint);
			return;
		}
		
		if (activeRoadConfigs && activeRoadConfigs.Count() > 0)
		{
			foreach (SCR_RoadTrackConfig roadConfig : activeRoadConfigs)
			{
				if (!roadConfig || !roadConfig.m_bEnabled || roadConfig.m_sRoadPrefab == "")
					continue;
				
				if (!HasRequiredBaseRoad(segmentCenter, roadConfig))
				{
					if (m_bDebug)
						PrintFormat("'%1' needs base road", roadConfig.m_sConfigName);
					
					if (m_bConstructionMode && roadConfig.m_iTierLevel > 0)
						tierRequirementFailed = true;
					
					continue;
				}
				
				// No fuel blocking - roads spawn regardless, fuel consumed when available
				
				if (roadConfig.m_fMinSegmentInterval > 0)
				{
					float lastSegmentTime;
					if (m_mLastSegmentTimes.Find(roadConfig.m_sConfigName, lastSegmentTime))
					{
						if (currentTime - lastSegmentTime < roadConfig.m_fMinSegmentInterval)
							continue;
					}
				}
				
				bool tooClose = false;
				vector lastSpawnPos;
				if (m_mLastRoadSpawnPositions.Find(roadConfig.m_sConfigName, lastSpawnPos))
				{
					if (vector.Distance(segmentCenter, lastSpawnPos) < roadConfig.m_fMinRoadSegmentDistance)
						tooClose = true;
				}
				
				if (tooClose)
					continue;
				
				if (ShouldSpawnRoadOnTerrain(roadConfig))
				{
					anyRoadWillSpawn = true;
					
					DelayedRoadSpawn delayedRoad = new DelayedRoadSpawn();
					delayedRoad.m_Spline = spline;
					delayedRoad.m_RoadConfig = roadConfig;
					delayedRoad.m_SegmentCenter = segmentCenter;
					delayedRoad.m_SegmentLength = segmentLength;
					delayedRoad.m_fSpawnTime = m_World.GetWorldTime() + (roadConfig.m_fRoadSpawnDelay * 1000.0);
					m_aPendingRoadSpawns.Insert(delayedRoad);
					
					m_mLastRoadSpawnPositions.Set(roadConfig.m_sConfigName, segmentCenter);
					m_mLastSegmentTimes.Set(roadConfig.m_sConfigName, currentTime);
				}
			}
		}
		
		// Auto-disable construction mode if tier requirement failed
		if (tierRequirementFailed && !anyRoadWillSpawn && m_bConstructionMode)
		{
			if (m_bDebug)
				Print("No base tier found - disabling construction mode", LogLevel.NORMAL);
			
			SetConstructionMode(false);
			
			// Retry with normal road configs
			CreateSplineFromPoints();
			return;
		}
		
		if (m_PrefabConfig && m_PrefabConfig.m_bEnabled && m_PrefabConfig.m_sDeformationPrefab != "")
		{
			if (anyRoadWillSpawn)
			{
				if (!m_bRoadSpawnedSinceLanding)
					m_bRoadSpawnedSinceLanding = true;
				
				SpawnPrefabsAlongSpline(spline, splinePoints);
			}
		}
		
		vector lastPoint = m_aTrackPoints[m_aTrackPoints.Count() - 1];
		m_aTrackPoints.Clear();
		m_aTrackPoints.Insert(lastPoint);
	}
	
	//------------------------------------------------------------------------------------------------
	void ProcessDelayedRoadSpawns()
	{
		if (!m_aPendingRoadSpawns || m_aPendingRoadSpawns.Count() == 0)
			return;
		
		float currentTime = m_World.GetWorldTime();
		
		for (int i = m_aPendingRoadSpawns.Count() - 1; i >= 0; i--)
		{
			DelayedRoadSpawn spawn = m_aPendingRoadSpawns[i];
			if (!spawn)
			{
				m_aPendingRoadSpawns.RemoveOrdered(i);
				continue;
			}
			
			if (currentTime >= spawn.m_fSpawnTime)
			{
				if (spawn.m_Spline && spawn.m_RoadConfig)
				{
					IEntity roadEntity = CreateRoadOnSpline(spawn.m_Spline, spawn.m_RoadConfig);
					
					if (roadEntity)
					{
						ConsumeFuelForRoad(spawn.m_RoadConfig, spawn.m_SegmentLength);
						
						// Track spawned road with tier level
						SCR_SpawnedRoadData roadData = new SCR_SpawnedRoadData(
							spawn.m_SegmentCenter,
							spawn.m_RoadConfig.m_iTierLevel,
							currentTime
						);
						m_aSpawnedRoads.Insert(roadData);
						
						if (m_bDebug)
							PrintFormat("Spawned Tier %1 road at %2", spawn.m_RoadConfig.m_iTierLevel, spawn.m_SegmentCenter);
					}
				}
				m_aPendingRoadSpawns.RemoveOrdered(i);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void ProcessDelayedPrefabSpawns()
	{
		if (!m_aPendingPrefabSpawns || m_aPendingPrefabSpawns.Count() == 0)
			return;
		
		float currentTime = m_World.GetWorldTime();
		
		for (int i = m_aPendingPrefabSpawns.Count() - 1; i >= 0; i--)
		{
			DelayedPrefabSpawn spawn = m_aPendingPrefabSpawns[i];
			if (!spawn)
			{
				m_aPendingPrefabSpawns.RemoveOrdered(i);
				continue;
			}
			
			if (currentTime >= spawn.m_fSpawnTime)
			{
				if (spawn.m_Spline)
					SpawnPrefabsAlongSpline(spawn.m_Spline, spawn.m_aSplinePoints);
				
				m_aPendingPrefabSpawns.RemoveOrdered(i);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	bool CheckOverlap(vector position, float minDistance)
	{
		if (!m_aSpawnedPrefabPositions)
			return false;
		
		foreach (vector spawnedPos : m_aSpawnedPrefabPositions)
		{
			if (vector.Distance(position, spawnedPos) < minDistance)
				return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	bool ShouldSpawnRoadOnTerrain(SCR_RoadTrackConfig roadConfig)
	{
		if (!roadConfig)
			return false;
		
		if (roadConfig.m_bOnlyWhenReversing)
		{
			if (m_Simulation.GetSpeedKmh() >= 0)
				return false;
		}
		
		if (!roadConfig.m_bUseTerrainFiltering)
			return true;
		
		if (!m_TerrainDetector)
			return true;
		
		string currentTerrain = m_TerrainDetector.GetCurrentTerrain();
		if (currentTerrain == "" || currentTerrain == "UNKNOWN")
			return true;
		
		if (roadConfig.m_sBlockedTerrainTypes != "")
		{
			if (roadConfig.m_sBlockedTerrainTypes.IndexOf(currentTerrain) != -1)
				return false;
		}
		
		if (roadConfig.m_sAllowedTerrainTypes != "")
		{
			if (roadConfig.m_sAllowedTerrainTypes.IndexOf(currentTerrain) != -1)
				return true;
		}
		
		if (roadConfig.m_bAllowNaturalTerrain && IsNaturalTerrain(currentTerrain))
			return true;
		
		if (roadConfig.m_bAllowRoads && IsRoadTerrain(currentTerrain))
			return true;
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsNaturalTerrain(string terrainType)
	{
		return terrainType == "DIRT" || 
			   terrainType == "GRASS" || 
			   terrainType == "SAND" || 
			   terrainType == "GRAVEL" || 
			   terrainType == "ROCK" ||
			   terrainType == "SNOW" ||
			   terrainType == "MUD";
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsRoadTerrain(string terrainType)
	{
		return terrainType.IndexOf("ROAD") != -1 || 
			   terrainType == "ASPHALT" || 
			   terrainType == "CONCRETE";
	}
	
	//------------------------------------------------------------------------------------------------
	void SpawnPrefabsAlongSpline(IEntity spline, array<vector> splinePoints)
	{
		if (!spline || !splinePoints || splinePoints.Count() < 2)
			return;
		
		if (!m_PrefabConfig || !m_PrefabConfig.m_bEnabled || m_PrefabConfig.m_sDeformationPrefab == "")
			return;
		
		Resource prefabResource = Resource.Load(m_PrefabConfig.m_sDeformationPrefab);
		if (!prefabResource || !prefabResource.IsValid())
			return;
		
		float totalLength = CalculatePathLength(splinePoints);
		float spawnInterval = m_PrefabConfig.m_fPrefabSpawnInterval;
		int prefabCount = Math.Floor(totalLength / spawnInterval);
		
		if (prefabCount < 1)
			return;
		
		float currentDistance = 0;
		int nextSpawnIndex = 0;
		float trackWidth = 1.5;
		
		for (int i = 1; i < splinePoints.Count(); i++)
		{
			vector segmentStart = splinePoints[i - 1];
			vector segmentEnd = splinePoints[i];
			float segmentLength = vector.Distance(segmentStart, segmentEnd);
			
			while (currentDistance + segmentLength >= nextSpawnIndex * spawnInterval && nextSpawnIndex < prefabCount)
			{
				float targetDistance = nextSpawnIndex * spawnInterval;
				float segmentProgress = (targetDistance - currentDistance) / segmentLength;
				
				vector centerPos = vector.Lerp(segmentStart, segmentEnd, segmentProgress);
				vector spawnDir = (segmentEnd - segmentStart).Normalized();
				
				vector up = "0 1 0";
				vector forward = spawnDir;
				forward[1] = 0;
				forward = forward.Normalized();
				vector right = (up * forward).Normalized();
				
				vector leftPos = centerPos - right * (trackWidth * 0.5);
				vector rightPos = centerPos + right * (trackWidth * 0.5);
				
				if (!CheckOverlap(leftPos, m_PrefabConfig.m_fMinPrefabDistance))
				{
					if (SpawnDeformationPrefab(spline, leftPos, spawnDir, prefabResource))
						m_aSpawnedPrefabPositions.Insert(leftPos);
				}
				
				if (!CheckOverlap(rightPos, m_PrefabConfig.m_fMinPrefabDistance))
				{
					if (SpawnDeformationPrefab(spline, rightPos, spawnDir, prefabResource))
						m_aSpawnedPrefabPositions.Insert(rightPos);
				}
				
				nextSpawnIndex++;
			}
			
			currentDistance += segmentLength;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	bool SpawnDeformationPrefab(IEntity spline, vector position, vector direction, Resource prefabResource)
	{
		vector terrainPos = position;
		terrainPos[1] = SCR_TerrainHelper.GetTerrainY(terrainPos);
		
		vector surfaceNormal = SCR_TerrainHelper.GetTerrainNormal(terrainPos);
		
		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		vector forward = direction;
		forward[1] = 0;
		forward = forward.Normalized();
		
		vector right = (surfaceNormal * forward).Normalized();
		vector realForward = (right * surfaceNormal).Normalized();
		
		spawnParams.Transform[0] = right;
		spawnParams.Transform[1] = surfaceNormal;
		spawnParams.Transform[2] = realForward;
		spawnParams.Transform[3] = terrainPos;
		
		if (m_PrefabConfig.m_vPositionOffset != "0 0 0")
		{
			vector offset = spawnParams.Transform[0] * m_PrefabConfig.m_vPositionOffset[0] +
							spawnParams.Transform[1] * m_PrefabConfig.m_vPositionOffset[1] +
							spawnParams.Transform[2] * m_PrefabConfig.m_vPositionOffset[2];
			spawnParams.Transform[3] = spawnParams.Transform[3] + offset;
		}
		
		if (m_PrefabConfig && m_PrefabConfig.m_vRotationOffset != "0 0 0")
		{
			vector rotatedTransform[4];
			ApplyRotationOffset(spawnParams.Transform, m_PrefabConfig.m_vRotationOffset, rotatedTransform);
			spawnParams.Transform = rotatedTransform;
		}
		
		IEntity prefabEntity = GetGame().SpawnEntityPrefab(prefabResource, m_World, spawnParams);
		return prefabEntity != null;
	}
	
	//------------------------------------------------------------------------------------------------
	IEntity CreateRoadOnSpline(SplineShapeEntity spline, SCR_RoadTrackConfig roadConfig)
	{
		if (!roadConfig || !roadConfig.m_bEnabled)
			return null;
		
		Resource roadResource = Resource.Load(roadConfig.m_sRoadPrefab);
		if (!roadResource || !roadResource.IsValid())
			return null;
		
		vector splineTransform[4];
		spline.GetWorldTransform(splineTransform);
		
		EntitySpawnParams roadParams = new EntitySpawnParams();
		roadParams.TransformMode = ETransformMode.WORLD;
		roadParams.Transform = splineTransform;
		
		if (roadConfig.m_vPositionOffset != "0 0 0")
		{
			vector offset = roadParams.Transform[0] * roadConfig.m_vPositionOffset[0] +
							roadParams.Transform[1] * roadConfig.m_vPositionOffset[1] +
							roadParams.Transform[2] * roadConfig.m_vPositionOffset[2];
			roadParams.Transform[3] = roadParams.Transform[3] + offset;
		}
		
		if (roadConfig.m_vRotationOffset != "0 0 0")
		{
			vector rotatedTransform[4];
			ApplyRotationOffset(roadParams.Transform, roadConfig.m_vRotationOffset, rotatedTransform);
			roadParams.Transform = rotatedTransform;
		}
		
		IEntity roadGenEntity = GetGame().SpawnEntityPrefab(roadResource, m_World, roadParams);
		if (!roadGenEntity)
			return null;
		
		spline.AddChild(roadGenEntity, -1, EAddChildFlags.AUTO_TRANSFORM);
		roadGenEntity.SetName("vehicle_track_road_generator");
		
		return roadGenEntity;
	}
	
	//------------------------------------------------------------------------------------------------
	void ApplyRotationOffset(vector baseTransform[4], vector rotationOffset, out vector outTransform[4])
	{
		float pitch = rotationOffset[0] * Math.DEG2RAD;
		float yaw = rotationOffset[1] * Math.DEG2RAD;
		float roll = rotationOffset[2] * Math.DEG2RAD;
		
		vector rotMatrix[4];
		Math3D.MatrixIdentity4(rotMatrix);
		
		float cosPitch = Math.Cos(pitch);
		float sinPitch = Math.Sin(pitch);
		float cosYaw = Math.Cos(yaw);
		float sinYaw = Math.Sin(yaw);
		float cosRoll = Math.Cos(roll);
		float sinRoll = Math.Sin(roll);
		
		rotMatrix[0][0] = cosYaw * cosRoll;
		rotMatrix[0][1] = cosYaw * sinRoll;
		rotMatrix[0][2] = -sinYaw;
		
		rotMatrix[1][0] = sinPitch * sinYaw * cosRoll - cosPitch * sinRoll;
		rotMatrix[1][1] = sinPitch * sinYaw * sinRoll + cosPitch * cosRoll;
		rotMatrix[1][2] = sinPitch * cosYaw;
		
		rotMatrix[2][0] = cosPitch * sinYaw * cosRoll + sinPitch * sinRoll;
		rotMatrix[2][1] = cosPitch * sinYaw * sinRoll - sinPitch * cosRoll;
		rotMatrix[2][2] = cosPitch * cosYaw;
		
		Math3D.MatrixMultiply4(baseTransform, rotMatrix, outTransform);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_aTrackPoints && m_aTrackPoints.Count() >= 2)
			CreateSplineFromPoints();
		
		super.OnDelete(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsTrackingEnabled() { return m_bTrackingEnabled; }
	void SetTrackingEnabled(bool enabled) { m_bTrackingEnabled = enabled; }
	void ToggleTracking() { SetTrackingEnabled(!m_bTrackingEnabled); }
	bool IsConstructionMode() { return m_bConstructionMode; }
	void SetConstructionMode(bool enabled) { m_bConstructionMode = enabled; }
	void ToggleConstructionMode() { SetConstructionMode(!m_bConstructionMode); }
	
	//------------------------------------------------------------------------------------------------
	array<ref SCR_RoadTrackConfig> GetActiveRoadConfigs()
	{
		if (m_bConstructionMode && m_aConstructionRoadConfigs && m_aConstructionRoadConfigs.Count() > 0)
		{
			array<ref SCR_RoadTrackConfig> selectedConfig = {};
			SCR_RoadTrackConfig config = GetSelectedConstructionConfig();
			if (config)
				selectedConfig.Insert(config);
			return selectedConfig;
		}
		
		return m_aRoadConfigs;
	}
	
	//------------------------------------------------------------------------------------------------
	SCR_RoadTrackConfig GetPrimaryRoadConfig()
	{
		array<ref SCR_RoadTrackConfig> activeConfigs = GetActiveRoadConfigs();
		
		if (!activeConfigs || activeConfigs.Count() == 0)
			return null;
		
		foreach (SCR_RoadTrackConfig config : activeConfigs)
		{
			if (config && config.m_bEnabled)
				return config;
		}
		
		return null;
	}
}

//------------------------------------------------------------------------------------------------
class DelayedRoadSpawn
{
	SplineShapeEntity m_Spline;
	SCR_RoadTrackConfig m_RoadConfig;
	vector m_SegmentCenter;
	float m_SegmentLength;
	float m_fSpawnTime;
}

//------------------------------------------------------------------------------------------------
class DelayedPrefabSpawn
{
	IEntity m_Spline;
	ref array<vector> m_aSplinePoints;
	float m_fSpawnTime;
}

