//------------------------------------------------------------------------------------------------
//! Dual track configuration
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_DualTrackConfig
{
	[Attribute("", UIWidgets.Auto, "Track prefabs")]
	ref array<ref SCR_TrackPrefabConfig> m_aPrefabs;
	
	[Attribute("0 0 0", UIWidgets.Coords, "Spawn offset", "inf inf inf purpose=coords space=entity")]
	vector m_vSpawnOffset;
	
	[Attribute("0", UIWidgets.EditBox, "Left wheel index")]
	int m_iLeftWheelIndex;
	
	[Attribute("1", UIWidgets.EditBox, "Right wheel index")]
	int m_iRightWheelIndex;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable dual track spawning")]
	bool m_bEnabled;
}//------------------------------------------------------------------------------------------------
//! Track prefab configuration
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_TrackPrefabConfig
{
	[Attribute("Unnamed Track", UIWidgets.EditBox, "Config name")]
	string m_sConfigName;
	
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Track prefab", "et")]
	ResourceName m_sPrefabResource;
	
	[Attribute("0", UIWidgets.Slider, "Min speed (km/h)", "0 150 1")]
	float m_fMinSpeed;
	
	[Attribute("150", UIWidgets.Slider, "Max speed (km/h)", "0 150 1")]
	float m_fMaxSpeed;
	
	[Attribute("20", UIWidgets.Slider, "Overlap percentage", "0 50 1")]
	float m_fOverlapPercent;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable safety radius check")]
	bool m_bUseSafetyCheck;
	
	[Attribute("3", UIWidgets.Slider, "Terrain check points", "1 10 1")]
	int m_iTerrainCheckPoints;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable this prefab")]
	bool m_bEnabled;
	
	float m_fCachedTrackLength = 0;
	
	//------------------------------------------------------------------------------------------------
	void SetCachedTrackLength(float length)
	{
		m_fCachedTrackLength = length;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetTrackLength()
	{
		if (m_fCachedTrackLength > 0)
			return m_fCachedTrackLength;
		
		return 1.5;
	}
	
	//------------------------------------------------------------------------------------------------
	float GetSpawnDistance()
	{
		return GetTrackLength() * (1.0 - (m_fOverlapPercent * 0.01));
	}
	
	//------------------------------------------------------------------------------------------------
	float GetSafetyRadius()
	{
		return GetTrackLength() * 0.5;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsActiveAtSpeed(float speed)
	{
		if (!m_bEnabled || m_sPrefabResource == "")
			return false;
		
		float absSpeed = Math.AbsFloat(speed);
		return absSpeed >= m_fMinSpeed && absSpeed <= m_fMaxSpeed;
	}
}

//------------------------------------------------------------------------------------------------
//! Track side configuration
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_TrackSideConfig
{
	[Attribute("", UIWidgets.Auto, "Track prefabs")]
	ref array<ref SCR_TrackPrefabConfig> m_aPrefabs;
	
	[Attribute("0 0 0", UIWidgets.Coords, "Spawn offset", "inf inf inf purpose=coords space=entity")]
	vector m_vSpawnOffset;
	
	[Attribute("0", UIWidgets.EditBox, "Wheel index")]
	int m_iWheelIndex;
}

//------------------------------------------------------------------------------------------------
//! Front drive configuration
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//! Front drive configuration
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("Front Drive")]
class SCR_FrontDriveConfig
{
	[Attribute("1", UIWidgets.CheckBox, "Enable front drive")]
	bool m_bEnabled;
	
	[Attribute("", UIWidgets.Auto, "Dual track (both wheels)")]
	ref SCR_DualTrackConfig m_DualTrack;
	
	[Attribute("", UIWidgets.Auto, "Left track")]
	ref SCR_TrackSideConfig m_LeftTrack;
	
	[Attribute("", UIWidgets.Auto, "Right track")]
	ref SCR_TrackSideConfig m_RightTrack;
}
//------------------------------------------------------------------------------------------------
//! Reverse drive configuration
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("Reverse Drive")]
class SCR_ReverseDriveConfig
{
	[Attribute("1", UIWidgets.CheckBox, "Enable reverse drive")]
	bool m_bEnabled;
	
	[Attribute("", UIWidgets.Auto, "Dual track (both wheels)")]
	ref SCR_DualTrackConfig m_DualTrack;
	
	[Attribute("", UIWidgets.Auto, "Left track")]
	ref SCR_TrackSideConfig m_LeftTrack;
	
	[Attribute("", UIWidgets.Auto, "Right track")]
	ref SCR_TrackSideConfig m_RightTrack;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted/Vehicle", description: "Sequential track spawner")]
class SCR_SingleTrackSpawnerComponentClass : ScriptComponentClass
{
}

class SCR_SingleTrackSpawnerComponent : ScriptComponent
{
	[Attribute("", UIWidgets.Auto, "Front drive configuration", category: "Front Drive")]
	ref SCR_FrontDriveConfig m_FrontDriveConfig;
	
	[Attribute("", UIWidgets.Auto, "Reverse drive configuration", category: "Reverse Drive")]
	ref SCR_ReverseDriveConfig m_ReverseDriveConfig;
	
	[Attribute("", UIWidgets.EditBox, "Blocked terrains (comma-separated)", category: "Terrain Filter")]
	protected string m_sBlockedTerrains;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable terrain filtering", category: "Terrain Filter")]
	protected bool m_bEnableTerrainFilter;
	
	[Attribute("0", UIWidgets.CheckBox, "Ignore all limits", category: "Limits")]
	protected bool m_bIgnoreLimits;
	
	[Attribute("200", UIWidgets.Slider, "Max tracks per vehicle", "50 2000 10", category: "Limits")]
	protected int m_iMaxTracksPerVehicle;
	
	[Attribute("2000", UIWidgets.Slider, "Global max tracks", "500 10000 100", category: "Limits")]
	protected static int s_iGlobalMaxTracks;
	
	[Attribute("1", UIWidgets.CheckBox, "Skip aligned tracks", category: "Track Detection")]
	protected bool m_bSkipAlignedTracks;
	
	[Attribute("0.7", UIWidgets.Slider, "Alignment threshold", "0.5 0.95 0.05", category: "Track Detection")]
	protected float m_fAlignmentThreshold;
	
	[Attribute("2.0", UIWidgets.Slider, "Min track age (seconds)", "0 10.0 0.5", category: "Track Detection")]
	protected float m_fMinTrackAge;
	
	[Attribute("1", UIWidgets.CheckBox, "Align tracks to terrain normal", category: "General")]
	protected bool m_bAlignToTerrain;
	
	[Attribute("1", UIWidgets.CheckBox, "Enable component", category: "General")]
	protected bool m_bEnabled;
	
	[Attribute("0", UIWidgets.CheckBox, "Enable debug", category: "General")]
	protected bool m_bDebug;
	
	protected Vehicle m_Vehicle;
	protected VehicleWheeledSimulation m_Simulation;
	protected vector m_vLastFrontLeftSpawn;
	protected vector m_vLastFrontRightSpawn;
	protected vector m_vLastRearLeftSpawn;
	protected vector m_vLastRearRightSpawn;
	protected ref array<IEntity> m_aSpawnedTracks;
	protected ref map<IEntity, float> m_mTrackSpawnTimes;
	protected ref map<IEntity, vector> m_mTrackCenters;
	protected ref map<IEntity, bool> m_mTrackIsLeft;
	protected ref array<string> m_aBlockedTerrainList;
	protected SCR_VehicleTerrainDetectorComponent m_TerrainDetector;
	
	protected int m_iFrontLeftSequence = 0;
	protected int m_iFrontRightSequence = 0;
	protected int m_iRearLeftSequence = 0;
	protected int m_iRearRightSequence = 0;
	
	protected SCR_TrackPrefabConfig m_LastFrontLeftPrefab;
	protected SCR_TrackPrefabConfig m_LastFrontRightPrefab;
	protected SCR_TrackPrefabConfig m_LastRearLeftPrefab;
	protected SCR_TrackPrefabConfig m_LastRearRightPrefab;
	
	protected SCR_TrackPrefabConfig m_LastSpawnedFrontLeftPrefab;
	protected SCR_TrackPrefabConfig m_LastSpawnedFrontRightPrefab;
	protected SCR_TrackPrefabConfig m_LastSpawnedRearLeftPrefab;
	protected SCR_TrackPrefabConfig m_LastSpawnedRearRightPrefab;
	
	private static int s_iTotalTracksSpawned = 0;
	protected bool m_bWasReversing = false;
	protected bool m_bLastFrontLeftContact = false;
	protected bool m_bLastFrontRightContact = false;
	protected bool m_bLastRearLeftContact = false;
	protected bool m_bLastRearRightContact = false;
	protected vector m_vLastFrontDualSpawn;
	protected vector m_vLastRearDualSpawn;
	protected int m_iFrontDualSequence;
	protected int m_iRearDualSequence;
	protected SCR_TrackPrefabConfig m_LastSpawnedFrontDualPrefab;
	protected SCR_TrackPrefabConfig m_LastSpawnedRearDualPrefab;
	protected bool m_bLastFrontDualContact;
	protected bool m_bLastRearDualContact;
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		if (!Replication.IsServer())
			return;
		
		m_Vehicle = Vehicle.Cast(owner);
		if (!m_Vehicle)
			return;
		
		m_Simulation = VehicleWheeledSimulation.Cast(m_Vehicle.FindComponent(VehicleWheeledSimulation));
		if (!m_Simulation)
			return;
		
		m_TerrainDetector = SCR_VehicleTerrainDetectorComponent.Cast(m_Vehicle.FindComponent(SCR_VehicleTerrainDetectorComponent));
		
		m_aSpawnedTracks = new array<IEntity>();
		m_mTrackSpawnTimes = new map<IEntity, float>();
		m_mTrackCenters = new map<IEntity, vector>();
		m_mTrackIsLeft = new map<IEntity, bool>();
		m_vLastFrontLeftSpawn = vector.Zero;
		m_vLastFrontRightSpawn = vector.Zero;
		m_vLastRearLeftSpawn = vector.Zero;
		m_vLastRearRightSpawn = vector.Zero;
		m_iFrontLeftSequence = 0;
		m_iFrontRightSequence = 0;
		m_iRearLeftSequence = 0;
		m_iRearRightSequence = 0;
		m_LastSpawnedFrontLeftPrefab = null;
		m_LastSpawnedFrontRightPrefab = null;
		m_LastSpawnedRearLeftPrefab = null;
		m_LastSpawnedRearRightPrefab = null;
		m_bWasReversing = false;
		m_bLastFrontLeftContact = false;
		m_bLastFrontRightContact = false;
		m_bLastRearLeftContact = false;
		m_bLastRearRightContact = false;
		m_vLastFrontDualSpawn = vector.Zero;
		m_vLastRearDualSpawn = vector.Zero;
		m_iFrontDualSequence = 0;
		m_iRearDualSequence = 0;
		m_LastSpawnedFrontDualPrefab = null;
		m_LastSpawnedRearDualPrefab = null;
		m_bLastFrontDualContact = false;
		m_bLastRearDualContact = false;
		
		ParseBlockedTerrains();
		SetEventMask(owner, EntityEvent.FRAME);
		
		if (m_bDebug)
			Print("=== Track Spawner Initialized ===", LogLevel.NORMAL);
	}
	
	//------------------------------------------------------------------------------------------------
	void ParseBlockedTerrains()
	{
		m_aBlockedTerrainList = new array<string>();
		
		if (m_sBlockedTerrains == "")
			return;
		
		TStringArray parts = new TStringArray();
		m_sBlockedTerrains.Split(",", parts, true);
		
		foreach (string part : parts)
		{
			string trimmed = part;
			trimmed.Trim();
			if (trimmed != "")
				m_aBlockedTerrainList.Insert(trimmed);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsTerrainBlocked(string terrainType)
	{
		if (!m_bEnableTerrainFilter || !m_aBlockedTerrainList)
			return false;
		
		foreach (string blockedType : m_aBlockedTerrainList)
		{
			if (terrainType.Contains(blockedType))
				return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsReversing()
	{
		VehicleWheeledSimulation wheeledSim = VehicleWheeledSimulation.Cast(m_Simulation);
		if (!wheeledSim)
			return false;
		
		return wheeledSim.GetGear() == 0;
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!m_bEnabled || !m_Vehicle || !m_Simulation)
			return;
		
		if (!m_bIgnoreLimits)
		{
			if (s_iTotalTracksSpawned >= s_iGlobalMaxTracks)
				CleanupOldestTrack();
			
			if (m_aSpawnedTracks.Count() >= m_iMaxTracksPerVehicle)
				CleanupOldestTrack();
		}
		
		float speed = m_Simulation.GetSpeedKmh();
		bool reversing = IsReversing();
		
		if (reversing != m_bWasReversing)
		{
			if (reversing)
			{
				m_vLastFrontLeftSpawn = vector.Zero;
				m_vLastFrontRightSpawn = vector.Zero;
				m_vLastFrontDualSpawn = vector.Zero;
				m_bLastFrontLeftContact = false;
				m_bLastFrontRightContact = false;
				m_bLastFrontDualContact = false;
			}
			else
			{
				m_vLastRearLeftSpawn = vector.Zero;
				m_vLastRearRightSpawn = vector.Zero;
				m_vLastRearDualSpawn = vector.Zero;
				m_bLastRearLeftContact = false;
				m_bLastRearRightContact = false;
				m_bLastRearDualContact = false;
			}
			m_bWasReversing = reversing;
		}
		
		if (reversing)
		{
			if (m_ReverseDriveConfig && m_ReverseDriveConfig.m_bEnabled)
			{
				bool dualProcessed = false;
				if (m_ReverseDriveConfig.m_DualTrack && m_ReverseDriveConfig.m_DualTrack.m_bEnabled)
				{
					dualProcessed = ProcessDualTrack(
						m_ReverseDriveConfig.m_DualTrack,
						m_vLastRearDualSpawn,
						m_bLastRearDualContact,
						speed,
						true
					);
					
					if (dualProcessed)
					{
						vector leftWheelPos = m_Simulation.WheelGetContactPosition(m_ReverseDriveConfig.m_DualTrack.m_iLeftWheelIndex);
						vector rightWheelPos = m_Simulation.WheelGetContactPosition(m_ReverseDriveConfig.m_DualTrack.m_iRightWheelIndex);
						
						m_vLastRearLeftSpawn = leftWheelPos;
						m_vLastRearRightSpawn = rightWheelPos;
					}
				}
				
				if (!dualProcessed)
				{
					ProcessWheel(m_ReverseDriveConfig.m_LeftTrack, m_vLastRearLeftSpawn, m_bLastRearLeftContact, speed, true, true);
					ProcessWheel(m_ReverseDriveConfig.m_RightTrack, m_vLastRearRightSpawn, m_bLastRearRightContact, speed, true, false);
				}
			}
		}
		else
		{
			if (m_FrontDriveConfig && m_FrontDriveConfig.m_bEnabled)
			{
				bool dualProcessed = false;
				if (m_FrontDriveConfig.m_DualTrack && m_FrontDriveConfig.m_DualTrack.m_bEnabled)
				{
					dualProcessed = ProcessDualTrack(
						m_FrontDriveConfig.m_DualTrack,
						m_vLastFrontDualSpawn,
						m_bLastFrontDualContact,
						speed,
						false
					);
					
					if (dualProcessed)
					{
						vector leftWheelPos = m_Simulation.WheelGetContactPosition(m_FrontDriveConfig.m_DualTrack.m_iLeftWheelIndex);
						vector rightWheelPos = m_Simulation.WheelGetContactPosition(m_FrontDriveConfig.m_DualTrack.m_iRightWheelIndex);
						
						m_vLastFrontLeftSpawn = leftWheelPos;
						m_vLastFrontRightSpawn = rightWheelPos;
					}
				}
				
				if (!dualProcessed)
				{
					ProcessWheel(m_FrontDriveConfig.m_LeftTrack, m_vLastFrontLeftSpawn, m_bLastFrontLeftContact, speed, false, true);
					ProcessWheel(m_FrontDriveConfig.m_RightTrack, m_vLastFrontRightSpawn, m_bLastFrontRightContact, speed, false, false);
				}
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	void ProcessWheel(SCR_TrackSideConfig config, inout vector lastSpawnPos, inout bool lastContactState, float currentSpeed, bool isRear, bool isLeft)
	{
		if (!config || !config.m_aPrefabs || config.m_aPrefabs.Count() == 0)
			return;
		
		bool hasContact = m_Simulation.WheelHasContact(config.m_iWheelIndex);
		
		if (!hasContact)
		{
			if (lastContactState)
			{
				lastSpawnPos = vector.Zero;
				lastContactState = false;
				
				if (m_bDebug)
				{
					string side = GetSideString(isRear, isLeft);
					PrintFormat("✗ %1: Lost wheel contact - reset spawn position", side);
				}
			}
			return;
		}
		
		if (!lastContactState)
		{
			lastContactState = true;
			lastSpawnPos = vector.Zero;
			
			if (m_bDebug)
			{
				string side = GetSideString(isRear, isLeft);
				PrintFormat("✓ %1: Gained wheel contact - initializing", side);
			}
		}
		
		vector wheelPos = m_Simulation.WheelGetContactPosition(config.m_iWheelIndex);
		
		SCR_TrackPrefabConfig activePrefab = GetActivePrefabForSpeed(config, currentSpeed, isRear, isLeft);
		if (!activePrefab)
		{
			activePrefab = GetLastUsedPrefab(isRear, isLeft);
			if (!activePrefab)
			{
				if (lastSpawnPos != vector.Zero)
				{
					lastSpawnPos = vector.Zero;
					
					if (m_bDebug)
					{
						string side = GetSideString(isRear, isLeft);
						PrintFormat("✗ %1: No valid prefab found - reset spawn position", side);
					}
				}
				return;
			}
		}
		else
		{
			SetLastUsedPrefab(isRear, isLeft, activePrefab);
		}
		
		if (lastSpawnPos == vector.Zero)
		{
			lastSpawnPos = wheelPos;
			return;
		}
		
		float distanceTraveled = vector.Distance(wheelPos, lastSpawnPos);
		
		SCR_TrackPrefabConfig lastSpawnedPrefab = GetLastSpawnedPrefab(isRear, isLeft);
		bool isTransition = false;
		if (lastSpawnedPrefab)
		{
			if (lastSpawnedPrefab != activePrefab)
				isTransition = true;
		}
		
		float spawnDistance;
		if (isTransition)
		{
			float prevHalfLength = lastSpawnedPrefab.GetTrackLength() * 0.5;
			float newHalfLength = activePrefab.GetTrackLength() * 0.5;
			float prevOverlap = lastSpawnedPrefab.GetTrackLength() * (lastSpawnedPrefab.m_fOverlapPercent * 0.01);
			float newOverlap = activePrefab.GetTrackLength() * (activePrefab.m_fOverlapPercent * 0.01);
			
			spawnDistance = prevHalfLength - prevOverlap + newHalfLength - newOverlap;
			
			if (spawnDistance < 0.2)
				spawnDistance = 0.2;
		}
		else
		{
			spawnDistance = activePrefab.GetSpawnDistance();
		}
		
		if (distanceTraveled < spawnDistance)
			return;
		
		vector travelDir = wheelPos - lastSpawnPos;
		travelDir.Normalize();
		
		vector spawnPos = lastSpawnPos + (travelDir * spawnDistance);
		
		vector spawnDir = travelDir;
		spawnDir[1] = 0;
		spawnDir.Normalize();
		
		if (m_bEnableTerrainFilter && m_TerrainDetector)
		{
			string terrain = GetTerrainAtPosition(spawnPos);
			
			if (IsTerrainBlocked(terrain))
			{
				lastSpawnPos = spawnPos;
				return;
			}
		}
		
		if (m_TerrainDetector && m_TerrainDetector.HasBlacklistedDecalAt(spawnPos, 0.5))
		{
			lastSpawnPos = spawnPos;
			return;
		}
		
		if (!CheckTrackTerrainClearance(spawnPos, spawnDir, activePrefab, isRear, isLeft))
		{
			lastSpawnPos = spawnPos;
			return;
		}
		
		if (activePrefab.m_bUseSafetyCheck)
		{
			float safetyRadius = activePrefab.GetSafetyRadius();
			if (IsTrackNearby(spawnPos, safetyRadius, isLeft))
			{
				lastSpawnPos = spawnPos;
				return;
			}
		}
		
		if (m_bSkipAlignedTracks)
		{
			if (ShouldSkipDueToAlignment(spawnPos, spawnDir))
			{
				lastSpawnPos = spawnPos;
				return;
			}
		}
		
		IEntity spawnedTrack = SpawnTrack(activePrefab, spawnPos, spawnDir, config.m_vSpawnOffset, isLeft);
		
		if (spawnedTrack)
		{
			m_mTrackCenters.Set(spawnedTrack, spawnPos);
			lastSpawnPos = spawnPos;
			SetLastSpawnedPrefab(isRear, isLeft, activePrefab);
			
			if (m_bDebug)
			{
				string side = GetSideString(isRear, isLeft);
				int seqNum = GetCurrentSequenceNumber(isRear, isLeft);
				PrintFormat("✓ %1: '%2' [#%3] | Speed: %.1f km/h | Length: %.2fm", 
					side, activePrefab.m_sConfigName, seqNum, Math.AbsFloat(currentSpeed), activePrefab.GetTrackLength());
			}
		}
	}
	
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	protected bool ProcessDualTrack(SCR_DualTrackConfig config, inout vector lastSpawnPos, inout bool lastContactState, float currentSpeed, bool isRear)
	{
		if (!config || !config.m_aPrefabs || config.m_aPrefabs.Count() == 0)
			return false;
		
		bool leftContact = m_Simulation.WheelHasContact(config.m_iLeftWheelIndex);
		bool rightContact = m_Simulation.WheelHasContact(config.m_iRightWheelIndex);
		
		if (!leftContact || !rightContact)
		{
			if (lastContactState)
			{
				lastSpawnPos = vector.Zero;
				lastContactState = false;
			}
			return false;
		}
		
		if (!lastContactState)
		{
			lastContactState = true;
			lastSpawnPos = vector.Zero;
		}
		
		vector leftWheelPos = m_Simulation.WheelGetContactPosition(config.m_iLeftWheelIndex);
		vector rightWheelPos = m_Simulation.WheelGetContactPosition(config.m_iRightWheelIndex);
		vector centerPos = (leftWheelPos + rightWheelPos) * 0.5;
		
		vector vehicleTransform[4];
		m_Vehicle.GetWorldTransform(vehicleTransform);
		vector vehicleDirection = vehicleTransform[2];
		
		IEntity foundLeftMarker;
		IEntity foundRightMarker;
		bool isAlignedLeft;
		bool isAlignedRight;
		
		string leftMarker = SCR_SimpleCollisionMarkerComponent.CheckPositionAgainstAllMarkersWithEntity(
			leftWheelPos, 0.5, foundLeftMarker, isAlignedLeft, vehicleDirection, m_fAlignmentThreshold
		);
		
		string rightMarker = SCR_SimpleCollisionMarkerComponent.CheckPositionAgainstAllMarkersWithEntity(
			rightWheelPos, 0.5, foundRightMarker, isAlignedRight, vehicleDirection, m_fAlignmentThreshold
		);
		
		bool leftHasOldMarker = false;
		bool rightHasOldMarker = false;
		
		if (leftMarker != "" && foundLeftMarker)
		{
			SCR_SimpleCollisionMarkerComponent leftMarkerComp = SCR_SimpleCollisionMarkerComponent.Cast(
				foundLeftMarker.FindComponent(SCR_SimpleCollisionMarkerComponent)
			);
			if (leftMarkerComp)
			{
				float leftAge = leftMarkerComp.GetAge();
				if (leftAge > m_fMinTrackAge)
					leftHasOldMarker = true;
			}
		}
		
		if (rightMarker != "" && foundRightMarker)
		{
			SCR_SimpleCollisionMarkerComponent rightMarkerComp = SCR_SimpleCollisionMarkerComponent.Cast(
				foundRightMarker.FindComponent(SCR_SimpleCollisionMarkerComponent)
			);
			if (rightMarkerComp)
			{
				float rightAge = rightMarkerComp.GetAge();
				if (rightAge > m_fMinTrackAge)
					rightHasOldMarker = true;
			}
		}
		
		if (leftHasOldMarker || rightHasOldMarker)
		{
			lastSpawnPos = centerPos;
			return false;
		}
		
		string leftTerrain = GetTerrainAtPosition(leftWheelPos);
		string rightTerrain = GetTerrainAtPosition(rightWheelPos);
		
		bool leftBlocked = IsTerrainBlocked(leftTerrain);
		bool rightBlocked = IsTerrainBlocked(rightTerrain);
		
		if (leftBlocked != rightBlocked)
		{
			lastSpawnPos = centerPos;
			return false;
		}
		
		if (leftBlocked && rightBlocked)
		{
			lastSpawnPos = centerPos;
			return false;
		}
		
		if (m_TerrainDetector)
		{
			bool leftHasDecal = m_TerrainDetector.HasBlacklistedDecalAt(leftWheelPos, 0.5);
			bool rightHasDecal = m_TerrainDetector.HasBlacklistedDecalAt(rightWheelPos, 0.5);
			
			if (leftHasDecal != rightHasDecal)
			{
				lastSpawnPos = centerPos;
				return false;
			}
			
			if (leftHasDecal && rightHasDecal)
			{
				lastSpawnPos = centerPos;
				return false;
			}
		}
		
		if (lastSpawnPos == vector.Zero)
		{
			lastSpawnPos = centerPos;
			return true;
		}
		
		SCR_TrackPrefabConfig activePrefab = GetActivePrefabForSpeedArray(config.m_aPrefabs, currentSpeed);
		if (!activePrefab)
			return false;
		
		float distanceTraveled = vector.Distance(centerPos, lastSpawnPos);
		float spawnDistance = activePrefab.GetSpawnDistance();
		
		if (spawnDistance < 0.1)
			spawnDistance = 0.1;
		
		if (distanceTraveled < spawnDistance)
			return true;
		
		vector travelDir = centerPos - lastSpawnPos;
		travelDir.Normalize();
		vector spawnPos = lastSpawnPos + (travelDir * spawnDistance);
		vector spawnDir = travelDir;
		spawnDir[1] = 0;
		spawnDir.Normalize();
		
		IEntity spawnedTrack = SpawnTrack(activePrefab, spawnPos, spawnDir, config.m_vSpawnOffset, true);
		
		if (spawnedTrack)
		{
			m_mTrackCenters.Set(spawnedTrack, spawnPos);
			lastSpawnPos = spawnPos;
			
			if (isRear)
			{
				m_LastSpawnedRearDualPrefab = activePrefab;
				m_iRearDualSequence++;
			}
			else
			{
				m_LastSpawnedFrontDualPrefab = activePrefab;
				m_iFrontDualSequence++;
			}
		}
		
		return true;
	}
	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	bool CheckTrackTerrainClearance(vector center, vector direction, SCR_TrackPrefabConfig config, bool isRear, bool isLeft)
	{
		if (!m_bEnableTerrainFilter || !m_TerrainDetector)
			return true;
		
		int checkPoints = config.m_iTerrainCheckPoints;
		if (checkPoints < 1)
			checkPoints = 1;
		
		float trackLength = config.GetTrackLength();
		float halfLength = trackLength * 0.5;
		
		for (int i = 0; i < checkPoints; i++)
		{
			float t = 0;
			if (checkPoints > 1)
			{
				float iFloat = i;
				float pointsFloat = checkPoints - 1;
				t = (iFloat / pointsFloat) * 2.0 - 1.0;
			}
			
			vector checkPos = center + direction * (halfLength * t);
			
			string terrain = GetTerrainAtPosition(checkPos);
			
			if (IsTerrainBlocked(terrain))
				return false;
			
			if (m_TerrainDetector.HasBlacklistedDecalAt(checkPos, 0.3))
				return false;
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	SCR_TrackPrefabConfig GetLastUsedPrefab(bool isRear, bool isLeft)
	{
		if (isRear)
		{
			if (isLeft)
				return m_LastRearLeftPrefab;
			else
				return m_LastRearRightPrefab;
		}
		else
		{
			if (isLeft)
				return m_LastFrontLeftPrefab;
			else
				return m_LastFrontRightPrefab;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLastUsedPrefab(bool isRear, bool isLeft, SCR_TrackPrefabConfig prefab)
	{
		if (isRear)
		{
			if (isLeft)
				m_LastRearLeftPrefab = prefab;
			else
				m_LastRearRightPrefab = prefab;
		}
		else
		{
			if (isLeft)
				m_LastFrontLeftPrefab = prefab;
			else
				m_LastFrontRightPrefab = prefab;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	SCR_TrackPrefabConfig GetLastSpawnedPrefab(bool isRear, bool isLeft)
	{
		if (isRear)
		{
			if (isLeft)
				return m_LastSpawnedRearLeftPrefab;
			else
				return m_LastSpawnedRearRightPrefab;
		}
		else
		{
			if (isLeft)
				return m_LastSpawnedFrontLeftPrefab;
			else
				return m_LastSpawnedFrontRightPrefab;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void SetLastSpawnedPrefab(bool isRear, bool isLeft, SCR_TrackPrefabConfig prefab)
	{
		if (isRear)
		{
			if (isLeft)
				m_LastSpawnedRearLeftPrefab = prefab;
			else
				m_LastSpawnedRearRightPrefab = prefab;
		}
		else
		{
			if (isLeft)
				m_LastSpawnedFrontLeftPrefab = prefab;
			else
				m_LastSpawnedFrontRightPrefab = prefab;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	int GetCurrentSequenceNumber(bool isRear, bool isLeft)
	{
		if (isRear)
		{
			if (isLeft)
				return m_iRearLeftSequence;
			else
				return m_iRearRightSequence;
		}
		else
		{
			if (isLeft)
				return m_iFrontLeftSequence;
			else
				return m_iFrontRightSequence;
		}
	}
	
	//------------------------------------------------------------------------------------------------
	bool IsTrackNearby(vector position, float radius, bool isLeft)
	{
		foreach (IEntity track, vector center : m_mTrackCenters)
		{
			if (!track)
				continue;
			
			if (m_mTrackIsLeft.Contains(track) && m_mTrackIsLeft.Get(track) != isLeft)
				continue;
			
			float dist = vector.Distance(position, center);
			if (dist < radius)
				return true;
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	bool ShouldSkipDueToAlignment(vector position, vector direction)
	{
		vector flatDir = direction;
		flatDir[1] = 0;
		flatDir.Normalize();
		
		IEntity foundMarker;
		bool isAligned;
		SCR_SimpleCollisionMarkerComponent.CheckPositionAgainstAllMarkersWithEntity(
			position,
			0.3,
			foundMarker,
			isAligned,
			flatDir,
			m_fAlignmentThreshold
		);
		
		if (foundMarker && isAligned)
		{
			SCR_SimpleCollisionMarkerComponent markerComp = SCR_SimpleCollisionMarkerComponent.Cast(
				foundMarker.FindComponent(SCR_SimpleCollisionMarkerComponent)
			);
			
			if (markerComp)
			{
				float age = markerComp.GetAge();
				if (age > m_fMinTrackAge)
					return true;
			}
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	IEntity SpawnTrack(SCR_TrackPrefabConfig config, vector position, vector direction, vector offset, bool isLeft)
	{
		if (!config || config.m_sPrefabResource == "")
			return null;
		
		Resource res = Resource.Load(config.m_sPrefabResource);
		if (!res || !res.IsValid())
			return null;
		
		vector vehicleMat[4];
		m_Vehicle.GetWorldTransform(vehicleMat);
		
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		
		vector forward = direction;
		forward[1] = 0;
		forward.Normalize();
		
		vector up;
		vector right;
		
		if (m_bAlignToTerrain)
		{
			vector terrainNormal = GetTerrainNormal(position);
			up = terrainNormal;
			right = up * forward;
			right.Normalize();
			forward = right * up;
			forward.Normalize();
		}
		else
		{
			up = Vector(0, 1, 0);
			right = up * forward;
			right.Normalize();
			up = forward * right;
			up.Normalize();
		}
		
		params.Transform[0] = right;
		params.Transform[1] = up;
		params.Transform[2] = forward;
		
		vector spawnPos = position + 
			vehicleMat[0] * offset[0] + 
			vehicleMat[1] * offset[1] + 
			vehicleMat[2] * offset[2];
		params.Transform[3] = spawnPos;
		
		IEntity entity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		
		if (entity)
		{
			m_aSpawnedTracks.Insert(entity);
			s_iTotalTracksSpawned++;
			
			float currentTime = GetGame().GetWorld().GetWorldTime();
			m_mTrackSpawnTimes.Set(entity, currentTime);
			m_mTrackIsLeft.Set(entity, isLeft);
			
			if (config.m_fCachedTrackLength == 0)
			{
				SCR_SimpleCollisionMarkerComponent marker = SCR_SimpleCollisionMarkerComponent.Cast(
					entity.FindComponent(SCR_SimpleCollisionMarkerComponent)
				);
				
				if (marker)
				{
					float trackLength = marker.GetTrackLength();
					config.SetCachedTrackLength(trackLength);
				}
			}
		}
		
		return entity;
	}
	
	//------------------------------------------------------------------------------------------------
	vector GetTerrainNormal(vector position)
	{
		vector startPos = position + Vector(0, 2, 0);
		vector endPos = position - Vector(0, 2, 0);
		
		autoptr TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = endPos;
		trace.Flags = TraceFlags.WORLD;
		trace.Exclude = m_Vehicle;
		
		float traceDist = GetGame().GetWorld().TraceMove(trace, null);
		
		if (traceDist < 1.0)
			return trace.TraceNorm;
		
		return Vector(0, 1, 0);
	}
	
	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	string GetTerrainAtPosition(vector position)
	{
		vector startPos = position + Vector(0, 2, 0);
		vector endPos = position - Vector(0, 2, 0);
		
		autoptr TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = endPos;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = m_Vehicle;
		
		float traceDist = GetGame().GetWorld().TraceMove(trace, null);
		
		if (traceDist < 1.0 && trace.SurfaceProps)
		{
			GameMaterial material = trace.SurfaceProps;
			if (material && m_TerrainDetector)
				return m_TerrainDetector.ClassifyMaterial(material.GetName(), false);
		}
		
		return "UNKNOWN";
	}
	
	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	SCR_TrackPrefabConfig GetActivePrefabForSpeed(SCR_TrackSideConfig config, float speed, bool isRear, bool isLeft)
	{
		if (!config || !config.m_aPrefabs)
			return null;
		
		array<SCR_TrackPrefabConfig> matchingPrefabs = new array<SCR_TrackPrefabConfig>();
		
		foreach (SCR_TrackPrefabConfig prefab : config.m_aPrefabs)
		{
			if (prefab && prefab.IsActiveAtSpeed(speed))
				matchingPrefabs.Insert(prefab);
		}
		
		if (matchingPrefabs.Count() == 0)
			return null;
		
		if (matchingPrefabs.Count() == 1)
			return matchingPrefabs[0];
		
		float absSpeed = Math.AbsFloat(speed);
		SCR_TrackPrefabConfig bestPrefab = matchingPrefabs[0];
		
		foreach (SCR_TrackPrefabConfig prefab : matchingPrefabs)
		{
			if (prefab.m_fMinSpeed > bestPrefab.m_fMinSpeed)
				bestPrefab = prefab;
		}
		
		return bestPrefab;
	}
		

//------------------------------------------------------------------------------------------------
	SCR_TrackPrefabConfig GetActivePrefabForSpeedArray(array<ref SCR_TrackPrefabConfig> prefabs, float speed)
	{
		if (!prefabs)
			return null;
		
		foreach (SCR_TrackPrefabConfig prefab : prefabs)
		{
			if (prefab && prefab.IsActiveAtSpeed(speed))
				return prefab;
		}
		
		return null;
	}	
	
		//------------------------------------------------------------------------------------------------
	string GetSideString(bool isRear, bool isLeft)
	{
		string side = "";
		
		if (isRear)
			side = "REAR";
		else
			side = "FRONT";
		
		if (isLeft)
			side = side + "_L";
		else
			side = side + "_R";
		
		return side;
	}
	
	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	void CleanupOldestTrack()
	{
		if (!m_aSpawnedTracks || m_aSpawnedTracks.Count() == 0)
			return;
		
		float currentTime = GetGame().GetWorld().GetWorldTime();
		IEntity oldestTrack;
		float oldestAge = 0;
		int oldestIndex = -1;
		
		for (int i = m_aSpawnedTracks.Count() - 1; i >= 0; i--)
		{
			IEntity track = m_aSpawnedTracks[i];
			
			if (!track)
			{
				m_aSpawnedTracks.Remove(i);
				continue;
			}
			
			if (!m_mTrackSpawnTimes.Contains(track))
			{
				m_aSpawnedTracks.Remove(i);
				SCR_EntityHelper.DeleteEntityAndChildren(track);
				continue;
			}
			
			float spawnTime = m_mTrackSpawnTimes.Get(track);
			float age = currentTime - spawnTime;
			
			if (age > oldestAge)
			{
				oldestAge = age;
				oldestTrack = track;
				oldestIndex = i;
			}
		}
		
		if (oldestTrack && oldestIndex >= 0)
		{
			m_aSpawnedTracks.Remove(oldestIndex);
			m_mTrackSpawnTimes.Remove(oldestTrack);
			m_mTrackCenters.Remove(oldestTrack);
			m_mTrackIsLeft.Remove(oldestTrack);
			
			SCR_EntityHelper.DeleteEntityAndChildren(oldestTrack);
			s_iTotalTracksSpawned--;
		}
	}
	
	//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_aSpawnedTracks)
		{
			foreach (IEntity track : m_aSpawnedTracks)
			{
				if (track)
					SCR_EntityHelper.DeleteEntityAndChildren(track);
			}
			m_aSpawnedTracks.Clear();
			m_aSpawnedTracks = null;
		}
		
		if (m_mTrackSpawnTimes)
		{
			m_mTrackSpawnTimes.Clear();
			m_mTrackSpawnTimes = null;
		}
		
		if (m_mTrackCenters)
		{
			m_mTrackCenters.Clear();
			m_mTrackCenters = null;
		}
		
		if (m_mTrackIsLeft)
		{
			m_mTrackIsLeft.Clear();
			m_mTrackIsLeft = null;
		}
		
		if (m_aBlockedTerrainList)
		{
			m_aBlockedTerrainList.Clear();
			m_aBlockedTerrainList = null;
		}
		
		m_Vehicle = null;
		m_Simulation = null;
		m_TerrainDetector = null;
		m_FrontDriveConfig = null;
		m_ReverseDriveConfig = null;
		m_LastFrontLeftPrefab = null;
		m_LastFrontRightPrefab = null;
		m_LastRearLeftPrefab = null;
		m_LastRearRightPrefab = null;
		m_LastSpawnedFrontLeftPrefab = null;
		m_LastSpawnedFrontRightPrefab = null;
		m_LastSpawnedRearLeftPrefab = null;
		m_LastSpawnedRearRightPrefab = null;
		m_LastSpawnedFrontDualPrefab = null;
		m_LastSpawnedRearDualPrefab = null;
		
		super.OnDelete(owner);
	}
	
	//------------------------------------------------------------------------------------------------
	void SetEnabled(bool enabled) { m_bEnabled = enabled; }
	bool IsEnabled() { return m_bEnabled; }
	int GetSpawnedTrackCount()
	{
		if (m_aSpawnedTracks)
			return m_aSpawnedTracks.Count();
		return 0;
	}
	static int GetGlobalTrackCount() { return s_iTotalTracksSpawned; }
}
