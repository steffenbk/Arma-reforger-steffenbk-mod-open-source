//------------------------------------------------------------------------------------------------
//! Vehicle Terrain Detection Component
//! Detects terrain type - switches to rear wheels when reversing
//! Can remove specified decal types when driving over them (directional)
//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Vehicle", description: "Detects terrain underneath vehicle wheels")]
class SCR_VehicleTerrainDetectorComponentClass : ScriptComponentClass
{
}

class SCR_VehicleTerrainDetectorComponent : ScriptComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Enable marker detection")]
	protected bool m_bEnableMarkerDetection;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable debug output")]
	protected bool m_bEnableDebug;
	
	[Attribute("0", UIWidgets.CheckBox, "Show detailed wheel detection")]
	protected bool m_bDebugWheelDetection;

	[Attribute("1.0", UIWidgets.EditBox, "Detection interval (seconds)")]
	protected float m_fDetectionInterval;

	[Attribute("0.5", UIWidgets.EditBox, "Min speed to detect (km/h)")]
	protected float m_fMinSpeedForDetection;
	
	[Attribute("0", UIWidgets.EditBox, "Left front wheel index")]
	protected int m_iLeftFrontWheelIndex;
	
	[Attribute("1", UIWidgets.EditBox, "Right front wheel index")]
	protected int m_iRightFrontWheelIndex;
	
	[Attribute("4", UIWidgets.EditBox, "Left rear wheel index")]
	protected int m_iLeftRearWheelIndex;
	
	[Attribute("5", UIWidgets.EditBox, "Right rear wheel index")]
	protected int m_iRightRearWheelIndex;
	
	[Attribute("0 0 2.0", UIWidgets.Coords, "Front wheel detection offset", "inf inf inf purpose=coords space=entity")]
	protected vector m_vFrontOffset;
	
	[Attribute("0 0 -2.0", UIWidgets.Coords, "Rear wheel detection offset", "inf inf inf purpose=coords space=entity")]
	protected vector m_vRearOffset;
	
	[Attribute("1", UIWidgets.CheckBox, "Use rear wheels when reversing")]
	protected bool m_bUseRearWheelsWhenReversing;
	
	[Attribute("1", UIWidgets.CheckBox, "Show detection gizmo")]
	protected bool m_bShowDetectionGizmo;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable decal removal")]
	protected bool m_bEnableDecalRemoval;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable directional removal (only delete when crossing)")]
	protected bool m_bDirectionalRemoval;
	
	[Attribute("0.7", UIWidgets.Slider, "Alignment threshold (0.5=loose, 0.9=strict)", "0.5 0.95 0.05")]
	protected float m_fAlignmentThreshold;
	
	[Attribute("", UIWidgets.EditBox, "Decal types to remove (comma-separated, e.g. DECAL_1,DECAL_2)")]
	protected string m_sRemovableDecals;
	
	[Attribute("0", UIWidgets.Slider, "Delay before removing decals (seconds, 0 = instant)", "0 5.0 0.1")]
	protected float m_fRemovalDelay;
	
	[Attribute("0", UIWidgets.CheckBox, "Enable spawn blacklist (prevent spawning on these prefabs)")]
	protected bool m_bEnableSpawnBlacklist;
	
	[Attribute("", UIWidgets.EditBox, "Blacklisted prefabs for spawning (comma-separated, e.g. track_mud,track_snow)")]
	protected string m_sBlacklistedPrefabs;
	
	protected static ref array<SCR_SimpleCollisionMarkerComponent> s_AllMarkers;
	protected float m_fSpawnTime;
	protected ref array<string> m_aBlacklistedPrefabList;
	protected ref array<IEntity> m_aTempEntityList;
	protected Vehicle m_Vehicle;
	protected VehicleWheeledSimulation m_Simulation;
	protected float m_fLastDetectionTime;
	protected string m_sLastDetectedTerrain;
	protected vector m_vLastPosition;
	protected ref array<string> m_aRemovableDecalList;
	protected ref array<vector> m_aCurrentDetectionPositions;
	protected ref map<IEntity, float> m_mPendingRemovals;
	protected string m_sLastDebugTerrain;
	protected int m_iLastDebugGear;
	protected float m_fWheelDebugTimer;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
	
		if (!Replication.IsServer())
			return;
	
		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	
		m_Vehicle = Vehicle.Cast(owner);
		if (!m_Vehicle)
		{
			Print("SCR_VehicleTerrainDetector: Must be attached to Vehicle!", LogLevel.ERROR);
			return;
		}
	
		m_Simulation = VehicleWheeledSimulation.Cast(m_Vehicle.FindComponent(VehicleWheeledSimulation));
		if (!m_Simulation)
		{
			Print("SCR_VehicleTerrainDetector: Missing VehicleWheeledSimulation!", LogLevel.ERROR);
			return;
		}
	
		m_fLastDetectionTime = 0;
		m_sLastDetectedTerrain = "";
		m_vLastPosition = "0 0 0";
		m_aCurrentDetectionPositions = new array<vector>();
		m_mPendingRemovals = new map<IEntity, float>();
		m_sLastDebugTerrain = "";
		m_iLastDebugGear = -1;
		m_fWheelDebugTimer = 0;
		
		if (!s_AllMarkers)
			s_AllMarkers = new array<SCR_SimpleCollisionMarkerComponent>();
		
		ParseRemovableDecals();
	
		if (m_bEnableDebug)
			PrintFormat("SCR_VehicleTerrainDetector initialized on %1", owner.GetName());
	}
	
	//------------------------------------------------------------------------------------------------
	void ParseRemovableDecals()
	{
		m_aRemovableDecalList = new array<string>();
		m_aBlacklistedPrefabList = new array<string>();
		
		if (m_sRemovableDecals != "")
		{
			TStringArray parts = new TStringArray();
			m_sRemovableDecals.Split(",", parts, true);
			
			foreach (string part : parts)
			{
				string trimmed = part;
				trimmed.Trim();
				
				if (trimmed != "")
				{
					m_aRemovableDecalList.Insert(trimmed);
					
					if (m_bEnableDebug)
						PrintFormat("Added removable decal type: %1", trimmed);
				}
			}
		}
		
		if (m_sBlacklistedPrefabs != "")
		{
			TStringArray parts = new TStringArray();
			m_sBlacklistedPrefabs.Split(",", parts, true);
			
			foreach (string part : parts)
			{
				string trimmed = part;
				trimmed.Trim();
				
				if (trimmed != "")
				{
					m_aBlacklistedPrefabList.Insert(trimmed);
					
					if (m_bEnableDebug)
						PrintFormat("Added blacklisted prefab for spawning: %1", trimmed);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	bool ShouldRemoveDecal(string terrainType)
	{
		if (!m_bEnableDecalRemoval || !m_aRemovableDecalList || m_aRemovableDecalList.Count() == 0)
			return false;
		
		foreach (string removableType : m_aRemovableDecalList)
		{
			if (terrainType == removableType)
				return true;
		}
		
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_Vehicle || !m_Simulation)
			return;
	
		float currentTime = GetGame().GetWorld().GetWorldTime();
		
		if (m_bDebugWheelDetection)
		{
			m_fWheelDebugTimer += timeSlice;
			if (m_fWheelDebugTimer >= 0.5)
			{
				DebugWheelDetectionDetailed();
				m_fWheelDebugTimer = 0;
			}
		}
	
		if (m_fRemovalDelay > 0 && m_mPendingRemovals.Count() > 0)
		{
			array<IEntity> toRemove = new array<IEntity>();
			
			foreach (IEntity entity, float removalTime : m_mPendingRemovals)
			{
				if (!entity)
				{
					toRemove.Insert(entity);
					continue;
				}
				
				if (currentTime >= removalTime)
				{
					if (m_bEnableDebug)
						PrintFormat("DELAYED REMOVAL: %1", entity.GetName());
					
					SCR_EntityHelper.DeleteEntityAndChildren(entity);
					toRemove.Insert(entity);
				}
			}
			
			foreach (IEntity entity : toRemove)
			{
				m_mPendingRemovals.Remove(entity);
			}
		}
	
		if (currentTime - m_fLastDetectionTime < m_fDetectionInterval * 1000)
			return;
	
		float speed = m_Simulation.GetSpeedKmh();
		if (speed < m_fMinSpeedForDetection && speed > -m_fMinSpeedForDetection)
			return;
	
		m_fLastDetectionTime = currentTime;
		DetectTerrain();
	}

	//------------------------------------------------------------------------------------------------
	void DetectTerrain()
	{
		if (!m_Vehicle || !m_Simulation)
			return;
	
		vector vehiclePos = m_Vehicle.GetOrigin();
		
		m_aCurrentDetectionPositions.Clear();
		
		string terrainName;
		vector detectionPos;
		GetTerrainFromWheels(terrainName, detectionPos);
	
		if (terrainName != m_sLastDetectedTerrain)
		{
			m_sLastDetectedTerrain = terrainName;
			OnTerrainChanged(terrainName, vehiclePos);
		}
	
		if (m_bEnableDebug)
		{
			VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
			int currentGear = -1;
			if (wheeledSim)
				currentGear = wheeledSim.GetGear();
			
			if (terrainName != m_sLastDebugTerrain || currentGear != m_iLastDebugGear)
			{
				DebugTerrainInfo(vehiclePos, terrainName);
				m_sLastDebugTerrain = terrainName;
				m_iLastDebugGear = currentGear;
			}
		}
	
		m_vLastPosition = vehiclePos;
	}

	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	void DebugWheelDetectionDetailed()
	{
		Print("╔════════════════════════════════════════════════════════════╗", LogLevel.NORMAL);
		Print("║           DETAILED WHEEL DETECTION DEBUG                  ║", LogLevel.NORMAL);
		Print("╚════════════════════════════════════════════════════════════╝", LogLevel.NORMAL);
		
		VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
		int currentGear = -1;
		bool isReversing = false;
		
		if (wheeledSim)
		{
			currentGear = wheeledSim.GetGear();
			isReversing = (currentGear == 0);
		}
		
		PrintFormat("Vehicle: %1", m_Vehicle.GetName());
		
		string directionStr;
		if (isReversing)
			directionStr = "REVERSE";
		else
			directionStr = "FORWARD";
		
		PrintFormat("Gear: %1 | Direction: %2", currentGear, directionStr);
		PrintFormat("Speed: %.1f km/h", m_Simulation.GetSpeedKmh());
		Print("", LogLevel.NORMAL);
		
		int leftWheel = m_iLeftFrontWheelIndex;
		int rightWheel = m_iRightFrontWheelIndex;
		vector offset = m_vFrontOffset;
		string wheelSet = "FRONT";
		
		if (m_bUseRearWheelsWhenReversing && isReversing)
		{
			leftWheel = m_iLeftRearWheelIndex;
			rightWheel = m_iRightRearWheelIndex;
			offset = m_vRearOffset;
			wheelSet = "REAR";
		}
		
		PrintFormat("Active Wheel Set: %1 (L=%2, R=%3)", wheelSet, leftWheel, rightWheel);
		PrintFormat("Detection Offset: %1", offset);
		Print("", LogLevel.NORMAL);
		
		vector vehicleTransform[4];
		m_Vehicle.GetWorldTransform(vehicleTransform);
		vector vehicleDirection = vehicleTransform[2];
		
		vector worldOffset = vehicleTransform[0] * offset[0] +
							 vehicleTransform[1] * offset[1] +
							 vehicleTransform[2] * offset[2];
		
		Print("┌─────────────── LEFT WHEEL ───────────────┐", LogLevel.NORMAL);
		bool leftContact = m_Simulation.WheelHasContact(leftWheel);
		
		string leftContactStr;
		if (leftContact)
			leftContactStr = "YES";
		else
			leftContactStr = "NO";
		
		PrintFormat("│ Contact: %1", leftContactStr);
		
		if (leftContact)
		{
			vector wheelPos = m_Simulation.WheelGetContactPosition(leftWheel);
			vector detectionPos = wheelPos + worldOffset;
			
			PrintFormat("│ Wheel Position: %1", wheelPos);
			PrintFormat("│ Detection Position: %1", detectionPos);
			
			IEntity markerEntity;
			bool isAligned;
			string terrain = GetTerrainAtPosition(detectionPos, markerEntity, isAligned, vehicleDirection);
			
			PrintFormat("│ Detected Terrain: %1", terrain);
			
			string markerFoundStr;
			if (markerEntity)
				markerFoundStr = "YES";
			else
				markerFoundStr = "NO";
			
			PrintFormat("│ Marker Found: %1", markerFoundStr);
			
			if (markerEntity)
			{
				PrintFormat("│ Marker Entity: %1", markerEntity.GetName());
				
				string alignedStr;
				if (isAligned)
					alignedStr = "YES";
				else
					alignedStr = "NO";
				
				PrintFormat("│ Is Aligned: %1 (threshold: %.2f)", alignedStr, m_fAlignmentThreshold);
				
				SCR_SimpleCollisionMarkerComponent markerComp = SCR_SimpleCollisionMarkerComponent.Cast(
					markerEntity.FindComponent(SCR_SimpleCollisionMarkerComponent)
				);
				
				if (markerComp)
				{
					PrintFormat("│ Marker Age: %.1fs", markerComp.GetAge());
					PrintFormat("│ Track Length: %.2fm", markerComp.GetTrackLength());
				}
				
				bool shouldRemove = ShouldRemoveDecal(terrain);
				
				string shouldRemoveStr;
				if (shouldRemove)
					shouldRemoveStr = "YES";
				else
					shouldRemoveStr = "NO";
				
				PrintFormat("│ Should Remove: %1", shouldRemoveStr);
				
				if (shouldRemove && m_bDirectionalRemoval)
				{
					string directionalCheckStr;
					if (isAligned)
						directionalCheckStr = "ALIGNED (skip)";
					else
						directionalCheckStr = "CROSSING (remove)";
					
					PrintFormat("│ Directional Check: %1", directionalCheckStr);
				}
			}
			
			bool hasBlacklisted = HasBlacklistedDecalAt(detectionPos, 0.5);
			
			string blacklistedStr;
			if (hasBlacklisted)
				blacklistedStr = "YES";
			else
				blacklistedStr = "NO";
			
			PrintFormat("│ Has Blacklisted Decal: %1", blacklistedStr);
		}
		Print("└──────────────────────────────────────────┘", LogLevel.NORMAL);
		Print("", LogLevel.NORMAL);
		
		Print("┌─────────────── RIGHT WHEEL ──────────────┐", LogLevel.NORMAL);
		bool rightContact = m_Simulation.WheelHasContact(rightWheel);
		
		string rightContactStr;
		if (rightContact)
			rightContactStr = "YES";
		else
			rightContactStr = "NO";
		
		PrintFormat("│ Contact: %1", rightContactStr);
		
		if (rightContact)
		{
			vector wheelPos = m_Simulation.WheelGetContactPosition(rightWheel);
			vector detectionPos = wheelPos + worldOffset;
			
			PrintFormat("│ Wheel Position: %1", wheelPos);
			PrintFormat("│ Detection Position: %1", detectionPos);
			
			IEntity markerEntity;
			bool isAligned;
			string terrain = GetTerrainAtPosition(detectionPos, markerEntity, isAligned, vehicleDirection);
			
			PrintFormat("│ Detected Terrain: %1", terrain);
			
			string markerFoundStr;
			if (markerEntity)
				markerFoundStr = "YES";
			else
				markerFoundStr = "NO";
			
			PrintFormat("│ Marker Found: %1", markerFoundStr);
			
			if (markerEntity)
			{
				PrintFormat("│ Marker Entity: %1", markerEntity.GetName());
				
				string alignedStr;
				if (isAligned)
					alignedStr = "YES";
				else
					alignedStr = "NO";
				
				PrintFormat("│ Is Aligned: %1 (threshold: %.2f)", alignedStr, m_fAlignmentThreshold);
				
				SCR_SimpleCollisionMarkerComponent markerComp = SCR_SimpleCollisionMarkerComponent.Cast(
					markerEntity.FindComponent(SCR_SimpleCollisionMarkerComponent)
				);
				
				if (markerComp)
				{
					PrintFormat("│ Marker Age: %.1fs", markerComp.GetAge());
					PrintFormat("│ Track Length: %.2fm", markerComp.GetTrackLength());
				}
				
				bool shouldRemove = ShouldRemoveDecal(terrain);
				
				string shouldRemoveStr;
				if (shouldRemove)
					shouldRemoveStr = "YES";
				else
					shouldRemoveStr = "NO";
				
				PrintFormat("│ Should Remove: %1", shouldRemoveStr);
				
				if (shouldRemove && m_bDirectionalRemoval)
				{
					string directionalCheckStr;
					if (isAligned)
						directionalCheckStr = "ALIGNED (skip)";
					else
						directionalCheckStr = "CROSSING (remove)";
					
					PrintFormat("│ Directional Check: %1", directionalCheckStr);
				}
			}
			
			bool hasBlacklisted = HasBlacklistedDecalAt(detectionPos, 0.5);
			
			string blacklistedStr;
			if (hasBlacklisted)
				blacklistedStr = "YES";
			else
				blacklistedStr = "NO";
			
			PrintFormat("│ Has Blacklisted Decal: %1", blacklistedStr);
		}
		Print("└──────────────────────────────────────────┘", LogLevel.NORMAL);
		Print("", LogLevel.NORMAL);
		
		Print("┌────────── CONFIGURATION ─────────────────┐", LogLevel.NORMAL);
		
		string markerDetectionStr;
		if (m_bEnableMarkerDetection)
			markerDetectionStr = "ENABLED";
		else
			markerDetectionStr = "DISABLED";
		
		PrintFormat("│ Marker Detection: %1", markerDetectionStr);
		
		string decalRemovalStr;
		if (m_bEnableDecalRemoval)
			decalRemovalStr = "ENABLED";
		else
			decalRemovalStr = "DISABLED";
		
		PrintFormat("│ Decal Removal: %1", decalRemovalStr);
		
		string directionalRemovalStr;
		if (m_bDirectionalRemoval)
			directionalRemovalStr = "ENABLED";
		else
			directionalRemovalStr = "DISABLED";
		
		PrintFormat("│ Directional Removal: %1", directionalRemovalStr);
		PrintFormat("│ Removal Delay: %.1fs", m_fRemovalDelay);
		
		string spawnBlacklistStr;
		if (m_bEnableSpawnBlacklist)
			spawnBlacklistStr = "ENABLED";
		else
			spawnBlacklistStr = "DISABLED";
		
		PrintFormat("│ Spawn Blacklist: %1", spawnBlacklistStr);
		PrintFormat("│ Total Markers: %1", SCR_SimpleCollisionMarkerComponent.GetMarkerCount());
		Print("└──────────────────────────────────────────┘", LogLevel.NORMAL);
		Print("", LogLevel.NORMAL);
	}
	//------------------------------------------------------------------------------------------------
	void GetTerrainFromWheels(out string terrainName, out vector detectionPos)
	{
		terrainName = "UNKNOWN";
		detectionPos = vector.Zero;
		
		if (!m_Simulation)
			return;
	
		bool isReversing = false;
		int currentGear = -1;
		
		VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
		if (wheeledSim)
		{
			currentGear = wheeledSim.GetGear();
			isReversing = (currentGear == 0);
		}
		
		int leftWheel = m_iLeftFrontWheelIndex;
		int rightWheel = m_iRightFrontWheelIndex;
		vector offset = m_vFrontOffset;
		
		if (m_bUseRearWheelsWhenReversing && isReversing)
		{
			leftWheel = m_iLeftRearWheelIndex;
			rightWheel = m_iRightRearWheelIndex;
			offset = m_vRearOffset;
		}
	
		vector vehicleTransform[4];
		m_Vehicle.GetWorldTransform(vehicleTransform);
		vector vehicleDirection = vehicleTransform[2];
		
		vector worldOffset = vehicleTransform[0] * offset[0] +
							 vehicleTransform[1] * offset[1] +
							 vehicleTransform[2] * offset[2];
	
		if (m_Simulation.WheelHasContact(leftWheel))
		{
			vector wheelPos = m_Simulation.WheelGetContactPosition(leftWheel);
			vector leftDetectionPos = wheelPos + worldOffset;
			m_aCurrentDetectionPositions.Insert(leftDetectionPos);
			
			IEntity markerEntity;
			bool isAligned;
			string terrain = GetTerrainAtPosition(leftDetectionPos, markerEntity, isAligned, vehicleDirection);
			
			if (terrain != "UNKNOWN")
			{
				terrainName = terrain;
				detectionPos = leftDetectionPos;
				
				if (markerEntity && ShouldRemoveDecal(terrain))
				{
					if (m_bDirectionalRemoval)
					{
						if (!isAligned)
						{
							if (m_bEnableDebug)
								PrintFormat("CROSSING track (LEFT) - REMOVING: %1", markerEntity.GetName());
							
							RemoveMarkerEntity(markerEntity, terrain, "LEFT");
						}
						else
						{
							terrainName = "EXISTING_" + terrain;
						}
					}
					else
					{
						RemoveMarkerEntity(markerEntity, terrain, "LEFT");
					}
				}
			}
		}
	
		if (m_Simulation.WheelHasContact(rightWheel))
		{
			vector wheelPos = m_Simulation.WheelGetContactPosition(rightWheel);
			vector rightDetectionPos = wheelPos + worldOffset;
			m_aCurrentDetectionPositions.Insert(rightDetectionPos);
			
			IEntity markerEntity;
			bool isAligned;
			string terrain = GetTerrainAtPosition(rightDetectionPos, markerEntity, isAligned, vehicleDirection);
			
			if (terrain != "UNKNOWN")
			{
				if (terrainName == "UNKNOWN")
				{
					terrainName = terrain;
					detectionPos = rightDetectionPos;
				}
				
				if (markerEntity && ShouldRemoveDecal(terrain))
				{
					if (m_bDirectionalRemoval)
					{
						if (!isAligned)
						{
							if (m_bEnableDebug)
								PrintFormat("CROSSING track (RIGHT) - REMOVING: %1", markerEntity.GetName());
							
							RemoveMarkerEntity(markerEntity, terrain, "RIGHT");
						}
						else
						{
							if (terrainName == "UNKNOWN" || !terrainName.Contains("EXISTING_"))
								terrainName = "EXISTING_" + terrain;
						}
					}
					else
					{
						RemoveMarkerEntity(markerEntity, terrain, "RIGHT");
					}
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	string CheckDetectionBoxes(vector position, out IEntity foundMarker, out bool isAligned, vector vehicleDirection)
	{
		foundMarker = null;
		isAligned = false;
		
		if (!m_bEnableMarkerDetection)
			return "UNKNOWN";
		
		string markerTerrain = SCR_SimpleCollisionMarkerComponent.CheckPositionAgainstAllMarkersWithEntity(
			position, 
			0.5, 
			foundMarker, 
			isAligned, 
			vehicleDirection,
			m_fAlignmentThreshold
		);
		
		if (markerTerrain != "")
			return markerTerrain;
		
		return "UNKNOWN";
	}
	
	//------------------------------------------------------------------------------------------------
	void RemoveMarkerEntity(IEntity markerEntity, string terrainType, string wheelSide)
	{
		if (!markerEntity)
			return;
		
		if (m_mPendingRemovals.Contains(markerEntity))
			return;
		
		if (m_fRemovalDelay > 0)
		{
			float currentTime = GetGame().GetWorld().GetWorldTime();
			float removalTime = currentTime + (m_fRemovalDelay * 1000);
			m_mPendingRemovals.Set(markerEntity, removalTime);
			
			if (m_bEnableDebug)
				PrintFormat("SCHEDULED removal at %1 wheel: %2 | Type: %3 | Delay: %.1fs", wheelSide, markerEntity.GetName(), terrainType, m_fRemovalDelay);
		}
		else
		{
			if (m_bEnableDebug)
				PrintFormat("REMOVING at %1 wheel: %2 | Type: %3", wheelSide, markerEntity.GetName(), terrainType);
			
			SCR_EntityHelper.DeleteEntityAndChildren(markerEntity);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	string GetTerrainAtPosition(vector position, out IEntity markerEntity, out bool isAligned, vector vehicleDirection)
	{
		markerEntity = null;
		isAligned = false;
		
		string boxTerrain = CheckDetectionBoxes(position, markerEntity, isAligned, vehicleDirection);
		if (boxTerrain != "UNKNOWN")
			return boxTerrain;
		
		vector startPos = position;
		startPos[1] = startPos[1] + 2.0;
		
		vector endPos = position;
		endPos[1] = endPos[1] - 2.0;
		
		autoptr TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = endPos;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = m_Vehicle;
		
		float traceDist = GetGame().GetWorld().TraceMove(trace, null);
		
		if (traceDist < 1.0)
		{
			if (trace.SurfaceProps)
			{
				GameMaterial surfaceMaterial = trace.SurfaceProps;
				if (surfaceMaterial)
				{
					string materialName = surfaceMaterial.GetName();
					
					if (IsVehicleMaterial(materialName))
						return "UNKNOWN";
					
					return ClassifyMaterial(materialName, false);
				}
			}
		}
		
		return "UNKNOWN";
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsVehicleMaterial(string materialName)
	{
		if (materialName == "")
			return false;
		
		string lowerName = materialName;
		lowerName.ToLower();
		
		if (lowerName.IndexOf("vehicle") != -1)
			return true;
		if (lowerName.IndexOf("wheel") != -1)
			return true;
		if (lowerName.IndexOf("tire") != -1)
			return true;
		if (lowerName.IndexOf("chassis") != -1)
			return true;
		if (lowerName.IndexOf("body") != -1)
			return true;
		if (lowerName.IndexOf("armor") != -1)
			return true;
		if (lowerName.IndexOf("hull") != -1)
			return true;
		
		return false;
	}

	//------------------------------------------------------------------------------------------------
	string ClassifyMaterial(string materialName, bool isLiquid)
	{
		if (materialName == "")
			return "UNKNOWN";

		string lowerName = materialName;
		lowerName.ToLower();

		if (isLiquid)
		{
			if (lowerName.IndexOf("mud") != -1)
				return "MUD";
			else if (lowerName.IndexOf("water") != -1)
				return "WATER";
			else
				return "LIQUID";
		}

		bool isRoadLike = (lowerName.IndexOf("road") != -1 || lowerName.IndexOf("street") != -1 ||
						lowerName.IndexOf("path") != -1 || lowerName.IndexOf("track") != -1 ||
						lowerName.IndexOf("lane") != -1 || lowerName.IndexOf("highway") != -1);

		if (isRoadLike)
		{
			if (lowerName.IndexOf("asphalt") != -1 || lowerName.IndexOf("paved") != -1 ||
				lowerName.IndexOf("tarmac") != -1)
				return "ASPHALT_ROAD";

			if (lowerName.IndexOf("concrete") != -1 || lowerName.IndexOf("cement") != -1)
				return "CONCRETE_ROAD";

			if (lowerName.IndexOf("dirt") != -1 || lowerName.IndexOf("unpaved") != -1 ||
				lowerName.IndexOf("earth") != -1)
				return "DIRT_ROAD";

			if (lowerName.IndexOf("gravel") != -1 || lowerName.IndexOf("stone") != -1)
				return "GRAVEL_ROAD";

			if (lowerName.IndexOf("sand") != -1)
				return "SAND_ROAD";

			return "ROAD";
		}

		if (lowerName.IndexOf("dirt") != -1 || lowerName.IndexOf("soil") != -1 ||
			lowerName.IndexOf("earth") != -1)
			return "DIRT";

		if (lowerName.IndexOf("grass") != -1 || lowerName.IndexOf("vegetation") != -1 ||
			lowerName.IndexOf("field") != -1 || lowerName.IndexOf("meadow") != -1)
			return "GRASS";

		if (lowerName.IndexOf("gravel") != -1 || lowerName.IndexOf("pebble") != -1 ||
			lowerName.IndexOf("stones") != -1)
			return "GRAVEL";

		if (lowerName.IndexOf("sand") != -1 || lowerName.IndexOf("beach") != -1 ||
			lowerName.IndexOf("dune") != -1)
			return "SAND";

		if (lowerName.IndexOf("concrete") != -1 || lowerName.IndexOf("cement") != -1)
			return "CONCRETE";

		if (lowerName.IndexOf("snow") != -1 || lowerName.IndexOf("ice") != -1)
			return "SNOW";

		if (lowerName.IndexOf("wood") != -1 || lowerName.IndexOf("timber") != -1 ||
			lowerName.IndexOf("plank") != -1)
			return "WOOD";

		if (lowerName.IndexOf("metal") != -1 || lowerName.IndexOf("steel") != -1 ||
			lowerName.IndexOf("iron") != -1)
			return "METAL";

		if (lowerName.IndexOf("rock") != -1 || lowerName.IndexOf("stone") != -1 ||
			lowerName.IndexOf("granite") != -1 || lowerName.IndexOf("boulder") != -1)
			return "ROCK";

		return "MAT_" + materialName;
	}

	//------------------------------------------------------------------------------------------------
	void OnTerrainChanged(string terrainName, vector position)
	{
		if (m_bEnableDebug)
		{
			Print("=== TERRAIN CHANGED ===", LogLevel.NORMAL);
			PrintFormat("New Terrain: %1", terrainName);
			PrintFormat("Position: %1", position);
		}
	}

	//------------------------------------------------------------------------------------------------
	void DebugTerrainInfo(vector vehiclePos, string terrainName)
	{
		VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
		int currentGear = -1;
		bool isReversing = false;
		
		if (wheeledSim)
		{
			currentGear = wheeledSim.GetGear();
			isReversing = (currentGear == 0);
		}
		
		int leftWheel = m_iLeftFrontWheelIndex;
		int rightWheel = m_iRightFrontWheelIndex;
		vector offset = m_vFrontOffset;
		
		if (m_bUseRearWheelsWhenReversing && isReversing)
		{
			leftWheel = m_iLeftRearWheelIndex;
			rightWheel = m_iRightRearWheelIndex;
			offset = m_vRearOffset;
		}

		Print("=== TERRAIN DEBUG ===", LogLevel.NORMAL);
		PrintFormat("Terrain: %1", terrainName);
		PrintFormat("Gear: %1 | Reversing: %2", currentGear, isReversing);
		PrintFormat("Using wheels: L=%1, R=%2 | Offset: %3", leftWheel, rightWheel, offset);
		PrintFormat("Removal: enabled=%1 | directional=%2 | delay=%.1fs", m_bEnableDecalRemoval, m_bDirectionalRemoval, m_fRemovalDelay);
		Print("==================", LogLevel.NORMAL);
		
		if (m_bShowDetectionGizmo)
		{
			foreach (vector pos : m_aCurrentDetectionPositions)
			{
				Shape.CreateSphere(COLOR_GREEN, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, pos, 0.3);
			}
			
			vector vehicleTransform[4];
			m_Vehicle.GetWorldTransform(vehicleTransform);
			
			vector worldOffset = vehicleTransform[0] * offset[0] +
								 vehicleTransform[1] * offset[1] +
								 vehicleTransform[2] * offset[2];
			
			if (m_Simulation.WheelHasContact(leftWheel))
			{
				vector wheelPos = m_Simulation.WheelGetContactPosition(leftWheel);
				vector detectionPos = wheelPos + worldOffset;
				Shape.CreateArrow(wheelPos, detectionPos, 0.1, COLOR_YELLOW, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			}
			
			if (m_Simulation.WheelHasContact(rightWheel))
			{
				vector wheelPos = m_Simulation.WheelGetContactPosition(rightWheel);
				vector detectionPos = wheelPos + worldOffset;
				Shape.CreateArrow(wheelPos, detectionPos, 0.1, COLOR_YELLOW, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	string GetCurrentTerrain()
	{
		return m_sLastDetectedTerrain;
	}
	
	//------------------------------------------------------------------------------------------------
	vector GetLastDetectedPosition()
	{
		return m_vLastPosition;
	}
	
	//------------------------------------------------------------------------------------------------
	Vehicle GetVehicle()
	{
		return m_Vehicle;
	}

	//------------------------------------------------------------------------------------------------
	void SetDebugMode(bool enable)
	{
		m_bEnableDebug = enable;
		string status;
		if (enable)
			status = "enabled";
		else
			status = "disabled";
		PrintFormat("Debug mode %1", status);
	}

	//------------------------------------------------------------------------------------------------
	void TriggerDetection()
	{
		DetectTerrain();
	}

	//------------------------------------------------------------------------------------------------
	bool HasBlacklistedDecalAt(vector position, float radius)
	{
		if (!m_bEnableSpawnBlacklist)
			return false;
		
		if (!m_aBlacklistedPrefabList || m_aBlacklistedPrefabList.Count() == 0)
			return false;
		
		m_aTempEntityList = new array<IEntity>();
		
		GetGame().GetWorld().QueryEntitiesBySphere(position, radius, CollectEntitiesCallback);
		
		foreach (IEntity entity : m_aTempEntityList)
		{
			if (!entity)
				continue;
			
			EntityPrefabData prefabData = entity.GetPrefabData();
			if (!prefabData)
				continue;
			
			string prefabName = prefabData.GetPrefabName();
			if (prefabName == "")
				continue;
			
			foreach (string blacklistedPrefab : m_aBlacklistedPrefabList)
			{
				if (prefabName.Contains(blacklistedPrefab))
				{
					if (m_bEnableDebug)
						PrintFormat("Found blacklisted prefab at position: %1 (prefab: %2)", blacklistedPrefab, prefabName);
					return true;
				}
			}
		}
		
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectEntitiesCallback(IEntity entity)
	{
		if (entity && m_aTempEntityList)
			m_aTempEntityList.Insert(entity);
		return true;
	}
}