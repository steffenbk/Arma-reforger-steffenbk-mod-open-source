class SCR_SprayPositionEntry
{
	vector m_vPos;
	float m_fExpiryMs;
	float m_fSize;
	Decal m_decal;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted", description: "Spray projectile - raycasts forward on spawn, spawns decal, deletes self")]
class SCR_SprayProjectileClass : ScriptComponentClass {}

class SCR_SprayProjectile : ScriptComponent
{
	[Attribute("20", UIWidgets.Slider, "Max spray range in meters", "0.5 50 0.5")]
	protected float m_fMaxRange;

	[Attribute("60", UIWidgets.EditBox, "Decal lifetime in seconds")]
	protected float m_fDecalLifetime;

	[Attribute("0", UIWidgets.CheckBox, "Enable debug prints")]
	protected bool m_bDebug;

	protected IEntity m_QueryResult;

	// Set by SCR_SpraySizeManagerComponent
	protected static float s_fDecalSize = 0.3;
	protected static float s_fMinSpacing = 0.15;
	protected static ResourceName s_sMaterial;
	protected static float s_fOpacity = 1.0;
	protected static float s_fBrightness = 1.0;
	protected static bool s_bRandomRotation = true;
	protected static vector s_vPlayerForward = "0 0 1";
	protected static float s_fPlayerYawDeg = 0;
	protected static SCR_SpraySizeManagerComponent s_Manager;

	protected static ref array<ref SCR_SprayPositionEntry> s_aDecalPositions = new array<ref SCR_SprayPositionEntry>();

	//------------------------------------------------------------------------------------------------
	static void SetSize(float decalSize, float minSpacing)
	{
		s_fDecalSize = decalSize;
		s_fMinSpacing = minSpacing;
	}

	//------------------------------------------------------------------------------------------------
	static void SetMaterial(ResourceName mat)
	{
		s_sMaterial = mat;
	}

	//------------------------------------------------------------------------------------------------
	static void SetOpacity(float alpha)
	{
		s_fOpacity = alpha;
	}

	//------------------------------------------------------------------------------------------------
	static void SetBrightness(float brightness)
	{
		s_fBrightness = brightness;
	}

	//------------------------------------------------------------------------------------------------
	static void SetRandomRotation(bool enabled)
	{
		s_bRandomRotation = enabled;
	}

	//------------------------------------------------------------------------------------------------
	static void SetPlayerForward(vector fwd)
	{
		s_vPlayerForward = fwd;
	}

	//------------------------------------------------------------------------------------------------
	static void SetPlayerYaw(float yawDeg)
	{
		s_fPlayerYawDeg = yawDeg;
	}

	//------------------------------------------------------------------------------------------------
	static float GetPlayerYaw()
	{
		return s_fPlayerYawDeg;
	}

	//------------------------------------------------------------------------------------------------
	static void SetManager(SCR_SpraySizeManagerComponent manager)
	{
		s_Manager = manager;
	}

	//------------------------------------------------------------------------------------------------
	protected bool QueryHitEntityCallback(IEntity entity)
	{
		m_QueryResult = entity;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void PurgeExpiredPositions(float currentTimeMs)
	{
		for (int i = s_aDecalPositions.Count() - 1; i >= 0; i--)
		{
			if (s_aDecalPositions[i].m_fExpiryMs <= currentTimeMs)
				s_aDecalPositions.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsTooClose(vector hitPos, float minSpacing, float newSize)
	{
		if (minSpacing <= 0)
			return false;

		PurgeExpiredPositions(GetGame().GetWorld().GetWorldTime());

		float spacingSq = minSpacing * minSpacing;
		foreach (SCR_SprayPositionEntry entry : s_aDecalPositions)
		{
			if (vector.DistanceSq(hitPos, entry.m_vPos) < spacingSq)
			{
				// Allow bigger decals to override smaller ones
				if (newSize > entry.m_fSize)
					continue;

				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveSmallerDecals(World world, vector hitPos, float newSize)
	{
		float coverRadius = newSize * 0.5;
		float coverRadiusSq = coverRadius * coverRadius;

		for (int i = s_aDecalPositions.Count() - 1; i >= 0; i--)
		{
			SCR_SprayPositionEntry entry = s_aDecalPositions[i];
			if (entry.m_fSize >= newSize)
				continue;

			if (vector.DistanceSq(hitPos, entry.m_vPos) < coverRadiusSq)
			{
				if (entry.m_decal)
					world.RemoveDecal(entry.m_decal);

				s_aDecalPositions.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackPosition(vector hitPos, float lifetimeSeconds, float size, Decal decal)
	{
		SCR_SprayPositionEntry entry = new SCR_SprayPositionEntry();
		entry.m_vPos = hitPos;
		entry.m_fExpiryMs = GetGame().GetWorld().GetWorldTime() + lifetimeSeconds * 1000;
		entry.m_fSize = size;
		entry.m_decal = decal;
		s_aDecalPositions.Insert(entry);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		GetGame().GetCallqueue().CallLater(DoSpray, 0, false, owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoSpray(IEntity owner)
	{
		if (!owner)
			return;

		float decalSize = s_fDecalSize;
		float minSpacing = s_fMinSpacing;

		int alpha = Math.ClampInt(s_fOpacity * 255, 0, 255);
		int bright = Math.ClampInt(s_fBrightness * 255, 0, 255);
		int decalColor = (alpha << 24) | (bright << 16) | (bright << 8) | bright;

		vector mat[4];
		owner.GetWorldTransform(mat);

		vector startPos = mat[3];
		vector forward = mat[2];
		vector rayEnd = startPos + forward * m_fMaxRange;

		World world = owner.GetWorld();
		if (!world)
		{
			delete owner;
			return;
		}

		TraceParam trace = new TraceParam();
		trace.Start = startPos;
		trace.End = rayEnd;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = owner;

		float hitFraction = world.TraceMove(trace, null);

		if (hitFraction >= 1.0)
		{
			if (m_bDebug)
				Print("[SprayProjectile] Miss - nothing hit", LogLevel.NORMAL);

			delete owner;
			return;
		}

		vector hitPos = startPos + (rayEnd - startPos) * hitFraction;
		vector surfaceNormal = trace.TraceNorm;
		IEntity hitEntity = trace.TraceEnt;

		// Ensure normal faces the sprayer (fixes reversed normals on thin surfaces like glass)
		vector toSprayer = startPos - hitPos;
		toSprayer.Normalize();
		if (vector.Dot(surfaceNormal, toSprayer) < 0)
			surfaceNormal = -surfaceNormal;

		// Block painting characters directly
		if (hitEntity && ChimeraCharacter.Cast(hitEntity))
		{
			if (m_bDebug)
				Print("[SprayProjectile] Skipped - hit a character", LogLevel.NORMAL);

			delete owner;
			return;
		}

		if (IsTooClose(hitPos, minSpacing, decalSize))
		{
			if (m_bDebug)
				Print("[SprayProjectile] Skipped - too close to existing decal", LogLevel.NORMAL);

			delete owner;
			return;
		}

		if (!hitEntity)
		{
			world.QueryEntitiesBySphere(hitPos, 1.0, QueryHitEntityCallback);
			hitEntity = m_QueryResult;
			m_QueryResult = null;
		}

		if (!hitEntity)
		{
			if (m_bDebug)
				Print("[SprayProjectile] Hit but no entity found to attach decal", LogLevel.WARNING);

			delete owner;
			return;
		}

		// Walk up to root parent so decal covers the full object (building + glass, not just a frame piece)
		IEntity parent = hitEntity.GetParent();
		while (parent)
		{
			hitEntity = parent;
			parent = hitEntity.GetParent();
		}

		ResourceName material = s_sMaterial;
		if (!material || material.IsEmpty())
		{
			if (m_bDebug)
				Print("[SprayProjectile] No decal material set", LogLevel.WARNING);

			delete owner;
			return;
		}

		vector decalOrigin = hitPos + surfaceNormal * 0.2;

		Decal decal;
		if (s_bRandomRotation)
		{
			float randomAngle = Math.RandomFloat(0, 360);
			decal = world.CreateDecal(hitEntity, decalOrigin, -surfaceNormal, 0.0, 1.0, randomAngle, decalSize, 1.0, material, m_fDecalLifetime, decalColor);
		}
		else
		{
			// Build an explicit orientation matrix so the stencil appears
			// upright on walls and facing the player on floors/ceilings/objects.

			// Desired "up" for the stencil texture:
			//   - On walls/angled surfaces: project world-up onto the surface plane
			//   - On near-horizontal surfaces: project player's forward onto the surface plane
			vector worldUp = Vector(0, 1, 0);
			float dotNU = vector.Dot(surfaceNormal, worldUp);
			vector desiredUp = worldUp - surfaceNormal * dotNU;
			float desLen = desiredUp.Length();

			if (desLen < 0.15)
			{
				// Near-horizontal surface — use player forward as stencil "up"
				float dotNF = vector.Dot(surfaceNormal, s_vPlayerForward);
				desiredUp = s_vPlayerForward - surfaceNormal * dotNF;
				desLen = desiredUp.Length();
			}

			if (desLen > 0.001)
				desiredUp = desiredUp * (1.0 / desLen);
			else
				desiredUp = Vector(0, 0, 1);

			// right = cross(surfaceNormal, desiredUp)  (stencil X-axis)
			vector right = Vector(
				surfaceNormal[1] * desiredUp[2] - surfaceNormal[2] * desiredUp[1],
				surfaceNormal[2] * desiredUp[0] - surfaceNormal[0] * desiredUp[2],
				surfaceNormal[0] * desiredUp[1] - surfaceNormal[1] * desiredUp[0]
			);
			float rLen = right.Length();
			if (rLen > 0.001)
				right = right * (1.0 / rLen);

			// Recompute up to ensure orthogonality: up = cross(right, -surfaceNormal)
			vector up = Vector(
				right[1] * (-surfaceNormal[2]) - right[2] * (-surfaceNormal[1]),
				right[2] * (-surfaceNormal[0]) - right[0] * (-surfaceNormal[2]),
				right[0] * (-surfaceNormal[1]) - right[1] * (-surfaceNormal[0])
			);

			// Build decal matrix: [right, -up, projectionDir, origin]
			vector decalMat[4];
			decalMat[0] = right;
			decalMat[1] = -up;
			decalMat[2] = -surfaceNormal;
			decalMat[3] = decalOrigin;

			decal = world.CreateDecal2(hitEntity, decalMat, 0.0, 1.0, decalSize, 1.0, material, m_fDecalLifetime, decalColor);
		}

		RemoveSmallerDecals(world, hitPos, decalSize);
		TrackPosition(hitPos, m_fDecalLifetime, decalSize, decal);

		// Auto-cycle to next color/stencil if the mode has it enabled
		if (s_Manager)
			s_Manager.AutoCycleColor();

		if (m_bDebug)
		{
			Print(string.Format("[SprayProjectile] Decal '%1' at %2 on %3", material, hitPos, hitEntity), LogLevel.NORMAL);
			Print(string.Format("[SprayProjectile] forward=%1  surfaceNormal=%2  normalY=%3  randomRot=%4",
				forward, surfaceNormal, surfaceNormal[1], s_bRandomRotation), LogLevel.NORMAL);
		}

		delete owner;
	}
}
