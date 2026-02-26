[BaseContainerProps()]
class FireMaterialRule
{
	[Attribute("", UIWidgets.EditBox, "Part of the surface material name to match (case-insensitive). Example: 'grass', 'wood', 'dirt'. Leave empty to match nothing.")]
	string m_sMatSubstring;

	[Attribute("5", UIWidgets.EditBox, "Minimum fire nodes spawned on this surface per impact. Set both min and max to the same value for a fixed count. Set to 0 to prevent fire spreading on this surface.")]
	int m_iMinNodes;

	[Attribute("8", UIWidgets.EditBox, "Maximum fire nodes spawned on this surface per impact. A random value between min and max is picked each time.")]
	int m_iMaxNodes;

}

[ComponentEditorProps(category: "WW2Vehicles/Projectile", description: "Spawns fire node prefabs spreading outward from the impact point.")]
class FireSpreadSpawnerClass : ScriptComponentClass {}

class FireSpreadSpawner : ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The FireSpreadNode entity prefab (.et) to place at each fire position.", params: "et")]
	protected ResourceName m_rPrefab;

	[Attribute("5", UIWidgets.EditBox, "Minimum number of fire nodes to spawn when the surface material matches no rule.")]
	protected int m_iCountMin;

	[Attribute("8", UIWidgets.EditBox, "Maximum number of fire nodes to spawn when the surface material matches no rule.")]
	protected int m_iCountMax;

	[Attribute("10", UIWidgets.EditBox, "Soft cap: spawning pauses when this many nodes from this impact are alive at once. Resumes as nodes burn out.")]
	protected int m_iMaxAlive;

	[Attribute("15", UIWidgets.EditBox, "Hard cap: if alive nodes exceed this value the oldest node is force-deleted immediately to make room. Should be higher than the soft cap. 0 = disabled.")]
	protected int m_iHardCap;

	[Attribute("0.4", UIWidgets.EditBox, "Minimum step distance (metres) between fire nodes. Each node steps at least this far forward.")]
	protected float m_fStepDistMin;

	[Attribute("0.9", UIWidgets.EditBox, "Maximum step distance (metres) between fire nodes. A random value between min and max is picked each spawn.")]
	protected float m_fStepDistMax;

	[Attribute("0.2", UIWidgets.EditBox, "Minimum sideways drift (metres) each node can deviate from the spread direction.")]
	protected float m_fDeviationMin;

	[Attribute("0.6", UIWidgets.EditBox, "Maximum sideways drift (metres) each node can deviate from the spread direction.")]
	protected float m_fDeviationMax;

	[Attribute("0.0", UIWidgets.EditBox, "Minimum random direction rotation (degrees) applied each step. Causes the fire to curve over time.")]
	protected float m_fDirDriftMin;

	[Attribute("8.0", UIWidgets.EditBox, "Maximum random direction rotation (degrees) applied each step. Higher values create more erratic, winding spread.")]
	protected float m_fDirDriftMax;

	[Attribute("0.5", UIWidgets.EditBox, "How often the spawner tries to place the next node (seconds). Lower = faster spread.")]
	protected float m_fInterval;

	[Attribute("5.0", UIWidgets.EditBox, "How far (metres) to raycast along the surface normal to find the surface when placing each node. Keeps nodes stuck to walls, floors and slopes. Set to 0 to disable.")]
	protected float m_fSurfaceSnapDist;

	[Attribute("2.0", UIWidgets.EditBox, "Before placing each node (after the first two), the spawner checks this radius (metres) for an existing fire node nearby. If none found, it waits. Set to 0 to always place immediately.")]
	protected float m_fProximityRadius;

	[Attribute("1.5", UIWidgets.EditBox, "Radius (metres) around each spawned node to search for and fell trees. Bushes cannot be removed via script on dedicated server. Set to 0 to disable.")]
	protected float m_fVegetationRadius;

	[Attribute("999999", UIWidgets.EditBox, "Damage applied to trees within the vegetation radius. Set high enough to fell the tree in one hit, or lower to just damage it.")]
	protected float m_fTreeDamage;

	[Attribute("1.0", UIWidgets.Slider, "How much the wind direction influences the spread direction. 0 = impact direction only, 1 = full wind direction.", params: "0.0 1.0 0.05")]
	protected float m_fWindInfluence;

	[Attribute()]
	protected ref array<ref FireMaterialRule> m_aMaterialRules;

	[Attribute("1", UIWidgets.CheckBox, "Draw debug spheres at each spawn position and log surface material and node counts to the console.")]
	protected bool m_bDebug;

	protected int    m_iSpawned;		// total nodes placed so far
	protected int    m_iAlive;			// nodes from this impact that are still burning
	protected int    m_iLocalCap;		// total node cap for this impact, rolled from material rule
	protected ref Resource m_NodeRes;	// cached prefab resource — avoids Resource.Load on every spawn
	protected ref array<IEntity> m_aLiveNodes = new array<IEntity>();		// oldest-first list of alive node entities
	protected ref set<IEntity> m_sDamagedTrees = new set<IEntity>();		// trees already hit — skip on repeat queries
	protected vector m_vLastPos;
	protected vector m_vForward;
	protected vector m_vRight;
	protected vector m_vSurfaceNormal;
	protected vector m_vNextPos;
	protected bool   m_bNextPosReady;
	protected bool   m_bFoundNearbyFire;

	protected ref array<ref Shape> m_aShapes = new array<ref Shape>();

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		GetGame().GetCallqueue().CallLater(Init, 500, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void Init()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Spawning, damage and vegetation destruction are server-only.
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (rpl && rpl.IsProxy())
			return;

		if (m_rPrefab.IsEmpty())
		{
			Print("[FireSpreadSpawner] No prefab set!", LogLevel.ERROR);
			return;
		}

		// Cache resource once — reused in every DoSpawn call.
		m_NodeRes = Resource.Load(m_rPrefab);
		if (!m_NodeRes || !m_NodeRes.IsValid())
		{
			Print("[FireSpreadSpawner] Resource load failed!", LogLevel.ERROR);
			return;
		}

		vector mat[4];
		owner.GetWorldTransform(mat);

		// +Y of the spawner entity is the surface normal — set by the warhead on impact.
		m_vSurfaceNormal = mat[1].Normalized();

		// Project forward/right onto the surface plane so spreading follows the surface.
		vector fwd3D = mat[2].Normalized();
		vector rt3D  = mat[0].Normalized();
		fwd3D = (fwd3D - m_vSurfaceNormal * (fwd3D * m_vSurfaceNormal)).Normalized();
		rt3D  = (rt3D  - m_vSurfaceNormal * (rt3D  * m_vSurfaceNormal)).Normalized();

		m_vForward = fwd3D;
		m_vRight   = rt3D;

		if (m_vForward.LengthSq() < 0.01)
			m_vForward = Vector(1, 0, 0);
		if (m_vRight.LengthSq() < 0.01)
			m_vRight = Vector(0, 0, 1);

		// Blend impact direction with wind direction based on m_fWindInfluence (0=impact, 1=wind).
		// Uses LocalWeatherSituation.GetLocalWindSway() — returns a world-space vector directly.
		if (m_fWindInfluence > 0)
		{
			ChimeraWorld cworld = ChimeraWorld.CastFrom(owner.GetWorld());
			if (cworld)
			{
				TimeAndWeatherManagerEntity weatherMgr = cworld.GetTimeAndWeatherManager();
				if (weatherMgr)
				{
					LocalWeatherSituation lws = new LocalWeatherSituation();
					if (weatherMgr.TryGetCompleteLocalWeather(lws, 1.0, owner.GetOrigin()))
					{
						vector windSwayRaw = lws.GetLocalWindSway();
						// Flatten Y by extracting components as floats — vector[1]=0 corrupts the vector.
						float wx = windSwayRaw[0];
						float wz = windSwayRaw[2];
						float wLenSq = wx * wx + wz * wz;
						float wLen = Math.Pow(wLenSq, 0.5);

						if (m_bDebug)
							Print("[FireSpreadSpawner] Wind sway raw: " + windSwayRaw + " XZlen=" + wLen + " speed=" + lws.GetGlobalWindSpeed(), LogLevel.WARNING);

						if (wLen > 0.0001)
						{
							// Negate — sway points INTO the wind; we want the downwind direction.
							vector windVec = Vector(-(wx / wLen), 0, -(wz / wLen));

							// Project onto surface plane.
							windVec = windVec - m_vSurfaceNormal * (windVec * m_vSurfaceNormal);
							if (windVec.LengthSq() > 0.0001)
								windVec = windVec.Normalized();

							// Blend: m_fWindInfluence=0 keeps impact dir, 1 uses pure wind dir.
							vector blended = m_vForward * (1.0 - m_fWindInfluence) + windVec * m_fWindInfluence;
							if (blended.LengthSq() > 0.0001)
								m_vForward = blended.Normalized();

							vector f = m_vForward;
							vector n = m_vSurfaceNormal;
							vector cross = Vector(f[2]*n[1] - f[1]*n[2], f[0]*n[2] - f[2]*n[0], f[1]*n[0] - f[0]*n[1]);
							if (cross.LengthSq() > 0.0001)
								m_vRight = cross.Normalized();

							if (m_bDebug)
								Print("[FireSpreadSpawner] Wind influence " + m_fWindInfluence + " — blended forward: " + m_vForward, LogLevel.WARNING);
						}
						else if (m_bDebug)
							Print("[FireSpreadSpawner] Wind XZ too small to use (len=" + wLen + "), keeping impact direction.", LogLevel.WARNING);
					}
				}
			}
		}

		vector origin   = owner.GetOrigin();
		m_vLastPos      = SnapToSurface(origin, owner.GetWorld());
		m_iSpawned      = 0;
		m_iAlive        = 0;
		m_bNextPosReady = false;

		// Proximity radius must exceed max step distance, otherwise the candidate
		// position can fall outside the radius of the previous node and stall.
		if (m_fProximityRadius > 0 && m_fProximityRadius <= m_fStepDistMax)
		{
			m_fProximityRadius = m_fStepDistMax + 0.5;
			if (m_bDebug)
				Print("[FireSpreadSpawner] Proximity radius too small — auto-adjusted to " + m_fProximityRadius, LogLevel.WARNING);
		}

		ResolveCaps(origin, owner.GetWorld());

		if (m_bDebug)
			Print("[FireSpreadSpawner] Surface cap: " + m_iLocalCap + " total, soft cap: " + m_iMaxAlive + ", hard cap: " + m_iHardCap, LogLevel.WARNING);

		if (m_iLocalCap == 0)
			return;

		// First node seeds the chain immediately.
		DoSpawn(owner, m_vLastPos);

		GetGame().GetCallqueue().CallLater(TrySpawnNext, m_fInterval * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	// Called by a node just before it deletes itself.
	void OnNodeDied(IEntity nodeEnt)
	{
		m_iAlive--;
		if (m_iAlive < 0)
			m_iAlive = 0;
		if (nodeEnt)
			m_aLiveNodes.RemoveItem(nodeEnt);
		if (m_bDebug)
			Print("[FireSpreadSpawner] Node died. Alive: " + m_iAlive + " / " + m_iMaxAlive, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveCaps(vector pos, BaseWorld world)
	{
		if (!m_aMaterialRules || m_aMaterialRules.Count() == 0 || m_fSurfaceSnapDist <= 0)
		{
			m_iLocalCap = RandRange(m_iCountMin, m_iCountMax);
			return;
		}

		vector from = pos + m_vSurfaceNormal * 0.5;
		vector to   = pos - m_vSurfaceNormal * m_fSurfaceSnapDist;

		TraceParam tp = new TraceParam();
		tp.Start   = from;
		tp.End     = to;
		tp.Flags   = TraceFlags.WORLD | TraceFlags.ENTS;
		tp.Exclude = GetOwner();

		float frac = world.TraceMove(tp, null);
		if (frac >= 1.0 || !tp.SurfaceProps)
		{
			m_iLocalCap = RandRange(m_iCountMin, m_iCountMax);
			return;
		}

		string matName = tp.SurfaceProps.GetName();
		matName.ToLower();

		if (m_bDebug)
			Print("[FireSpreadSpawner] Surface material: " + matName, LogLevel.WARNING);

		foreach (FireMaterialRule rule : m_aMaterialRules)
		{
			string sub = rule.m_sMatSubstring;
			sub.ToLower();
			if (!sub.IsEmpty() && matName.IndexOf(sub) != -1)
			{
				m_iLocalCap = RandRange(rule.m_iMinNodes, rule.m_iMaxNodes);
				return;
			}
		}

		m_iLocalCap = RandRange(m_iCountMin, m_iCountMax);
	}

	//------------------------------------------------------------------------------------------------
	protected int RandRange(int minVal, int maxVal)
	{
		if (minVal >= maxVal)
			return minVal;
		return minVal + Math.RandomInt(0, maxVal - minVal + 1);
	}

	//------------------------------------------------------------------------------------------------
	protected void TrySpawnNext()
	{
		if (m_iSpawned >= m_iLocalCap)
		{
			GetGame().GetCallqueue().Remove(TrySpawnNext);
			return;
		}

		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Soft cap — pause spawning until nodes burn out naturally.
		if (m_iMaxAlive > 0 && m_iAlive >= m_iMaxAlive)
		{
			if (m_bDebug)
				Print("[FireSpreadSpawner] Soft cap reached: " + m_iAlive + " / " + m_iMaxAlive + " — waiting.", LogLevel.WARNING);
			return;
		}

		// Compute candidate position once, hold it until the proximity check passes.
		if (!m_bNextPosReady)
		{
			// Randomly rotate the spread direction slightly each step to create organic curves.
			if (m_fDirDriftMax > 0)
			{
				float driftSign = 1;
				if (Math.RandomFloat(0, 1) > 0.5)
					driftSign = -1;
				float driftDeg = Math.RandomFloat(m_fDirDriftMin, m_fDirDriftMax) * driftSign;

				// Approximate cos/sin from drift angle without trig (Math.Sin/Cos return ~0).
				// For small angles (<=45 deg drift) this is accurate enough.
				float absDrift = driftDeg;
				if (absDrift < 0) absDrift = -absDrift;
				float sinD = absDrift / 45.0;
				if (sinD > 1) sinD = 1;
				float cosD = 1.0 - sinD * sinD * 0.5;	// cos(x) ≈ 1 - x²/2 for small x
				if (driftDeg < 0) sinD = -sinD;
				vector newFwd = m_vForward * cosD + m_vRight * sinD;
				vector newRt  = m_vRight   * cosD - m_vForward * sinD;
				m_vForward = newFwd.Normalized();
				m_vRight   = newRt.Normalized();
			}

			float step    = Math.RandomFloat(m_fStepDistMin, m_fStepDistMax);
			float lateralSign = 1;
			if (Math.RandomFloat(0, 1) > 0.5)
				lateralSign = -1;
			float lateral = Math.RandomFloat(m_fDeviationMin, m_fDeviationMax) * lateralSign;

			m_vNextPos = m_vLastPos
				+ m_vForward * step
				+ m_vRight   * lateral;

			m_vNextPos      = SnapToSurface(m_vNextPos, owner.GetWorld());
			m_bNextPosReady = true;

			if (m_bDebug && m_fProximityRadius > 0)
				m_aShapes.Insert(Shape.CreateSphere(ARGB(120, 0, 200, 255), ShapeFlags.VISIBLE | ShapeFlags.WIREFRAME, m_vNextPos, m_fProximityRadius));
		}

		// Proximity check — skip for first two nodes (entity not yet queryable right after spawn).
		if (m_fProximityRadius > 0 && m_iSpawned >= 2)
		{
			m_bFoundNearbyFire = false;
			owner.GetWorld().QueryEntitiesBySphere(m_vNextPos, m_fProximityRadius, ProximityCallback, null, EQueryEntitiesFlags.ALL);

			if (!m_bFoundNearbyFire)
				return;
		}

		DoSpawn(owner, m_vNextPos);
		m_bNextPosReady = false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool ProximityCallback(IEntity ent)
	{
		if (m_bFoundNearbyFire)
			return false;

		if (ent && ent.FindComponent(FireSpreadNode))
		{
			m_bFoundNearbyFire = true;
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool VegetationCallback(IEntity ent)
	{
		if (!ent)
			return true;

		EntityPrefabData pd = ent.GetPrefabData();
		if (!pd)
			return true;

		string fullPath = pd.GetPrefabName();
		string pathLower = fullPath;
		pathLower.ToLower();

		if (!pathLower.Contains("prefabs/vegetation"))
			return true;

		// Trees only — use damage system so they fall over properly.
		// Bushes cannot be removed via script on dedicated server (engine limitation).
		BaseTree tree = BaseTree.Cast(ent);
		if (tree)
		{
			if (m_sDamagedTrees.Contains(ent))
				return true;
			m_sDamagedTrees.Insert(ent);

			if (m_bDebug)
				Print("[FireSpreadSpawner] Felling tree: " + fullPath, LogLevel.WARNING);

			vector hitPosDirNorm[3];
			hitPosDirNorm[0] = ent.GetOrigin();
			hitPosDirNorm[1] = Vector(0, -1, 0);
			hitPosDirNorm[2] = Vector(0, -1, 0);
			tree.HandleDamage(EDamageType.FIRE, m_fTreeDamage, hitPosDirNorm);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void DoSpawn(IEntity owner, vector pos)
	{
		if (!m_NodeRes || !m_NodeRes.IsValid())
			return;

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = pos;

		EntitySpawnParams p = new EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		p.Transform     = mat;

		IEntity e = GetGame().SpawnEntityPrefab(m_NodeRes, owner.GetWorld(), p);
		if (!e)
		{
			Print("[FireSpreadSpawner] Spawn " + m_iSpawned + " failed!", LogLevel.ERROR);
			return;
		}

		// Register this spawner on the node so it can notify us when it dies.
		FireSpreadNode node = FireSpreadNode.Cast(e.FindComponent(FireSpreadNode));
		if (node)
			node.SetOwnerSpawner(this);

		m_aLiveNodes.Insert(e);
		m_vLastPos = pos;
		m_iSpawned++;
		m_iAlive++;

		// Hard cap — evict oldest nodes until we are back at or below the limit.
		// Remove from m_aLiveNodes BEFORE deleting to prevent the OnNodeDied callback
		// from modifying the array mid-iteration, which could loop forever.
		if (m_iHardCap > 0)
		{
			while (m_iAlive > m_iHardCap && m_aLiveNodes.Count() > 0)
			{
				IEntity oldest = m_aLiveNodes[0];
				m_aLiveNodes.RemoveOrdered(0);
				m_iAlive--;

				if (m_bDebug)
					Print("[FireSpreadSpawner] Hard cap exceeded — evicting oldest node. Alive now: " + m_iAlive, LogLevel.WARNING);

				// Zero the back-reference on the node before deleting so its OnDelete
				// doesn't call OnNodeDied and double-decrement m_iAlive.
				if (oldest)
				{
					FireSpreadNode evicted = FireSpreadNode.Cast(oldest.FindComponent(FireSpreadNode));
					if (evicted)
						evicted.m_OwnerSpawner = null;

					SCR_EntityHelper.DeleteEntityAndChildren(oldest);
				}
			}
		}

		// Destroy vegetation around the newly placed node.
		if (m_fVegetationRadius > 0)
			owner.GetWorld().QueryEntitiesBySphere(pos, m_fVegetationRadius, VegetationCallback, null, EQueryEntitiesFlags.ALL);

		if (m_bDebug)
			m_aShapes.Insert(Shape.CreateSphere(ARGB(255, 255, 80, 0), ShapeFlags.VISIBLE | ShapeFlags.WIREFRAME, pos, 0.3));
	}

	//------------------------------------------------------------------------------------------------
	protected vector SnapToSurface(vector pos, BaseWorld world)
	{
		if (m_fSurfaceSnapDist <= 0)
			return pos;

		vector from = pos + m_vSurfaceNormal * 0.5;
		vector to   = pos - m_vSurfaceNormal * m_fSurfaceSnapDist;

		TraceParam tp = new TraceParam();
		tp.Start   = from;
		tp.End     = to;
		tp.Flags   = TraceFlags.WORLD | TraceFlags.ENTS;
		tp.Exclude = GetOwner();

		float frac = world.TraceMove(tp, null);
		if (frac < 1.0)
			return from + (to - from) * frac;

		return pos;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(Init);
		GetGame().GetCallqueue().Remove(TrySpawnNext);

		// Zero the spawner back-reference on all live nodes so they don't call
		// OnNodeDied on a deleted spawner (dangling pointer).
		foreach (IEntity nodeEnt : m_aLiveNodes)
		{
			if (!nodeEnt)
				continue;
			FireSpreadNode node = FireSpreadNode.Cast(nodeEnt.FindComponent(FireSpreadNode));
			if (node)
				node.m_OwnerSpawner = null;
		}

		m_aShapes.Clear();
		m_aLiveNodes.Clear();
		m_sDamagedTrees.Clear();
	}
}
