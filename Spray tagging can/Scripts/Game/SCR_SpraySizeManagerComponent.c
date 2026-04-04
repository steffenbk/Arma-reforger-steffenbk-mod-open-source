[BaseContainerProps()]
class SCR_SizePreset
{
	[Attribute("Medium")]
	string m_sName;

	[Attribute("0.4", UIWidgets.Slider, "Decal size in meters", "0.1 2 0.05")]
	float m_fDecalSize;

	[Attribute("0.15", UIWidgets.Slider, "Min spacing in meters", "0.01 1 0.01")]
	float m_fMinSpacing;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_SprayColorEntry
{
	[Attribute("Color")]
	string m_sName;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Color material (.emat)", "emat")]
	ResourceName m_sMaterial;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_SprayOpacityPreset
{
	[Attribute("Full")]
	string m_sName;

	[Attribute("1.0", UIWidgets.Slider, "Opacity (0=invisible, 1=full)", "0.05 1 0.05")]
	float m_fAlpha;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_SprayBrightnessPreset
{
	[Attribute("Full")]
	string m_sName;

	[Attribute("1.0", UIWidgets.Slider, "Brightness (0=dark, 1=full)", "0.1 1 0.05")]
	float m_fBrightness;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class SCR_SprayModePreset
{
	[Attribute("Free Paint")]
	string m_sName;

	[Attribute("0", UIWidgets.CheckBox, "Auto-cycle through colors on each spray (stencil mode)")]
	bool m_bAutoCycle;

	[Attribute("1", UIWidgets.CheckBox, "Randomize decal rotation (disable for symbols)")]
	bool m_bRandomRotation;

	[Attribute()]
	ref array<ref SCR_SprayColorEntry> m_aColors;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "GameScripted", description: "Manages spray can size, color and opacity presets — pairs with SCR_SprayProjectile")]
class SCR_SpraySizeManagerComponentClass : ScriptComponentClass {}

class SCR_SpraySizeManagerComponent : ScriptComponent
{
	[Attribute()]
	protected ref array<ref SCR_SizePreset> m_aPresets;

	[Attribute()]
	protected ref array<ref SCR_SprayModePreset> m_aModes;

	[Attribute()]
	protected ref array<ref SCR_SprayOpacityPreset> m_aOpacities;

	[Attribute()]
	protected ref array<ref SCR_SprayBrightnessPreset> m_aBrightnesses;

	[Attribute("9000", UIWidgets.EditBox, "Ammo count for the spray can")]
	protected int m_iAmmoCount;

	[Attribute("1", UIWidgets.CheckBox, "Show laser dot indicator where paint will land")]
	protected bool m_bShowLaserDot;

	[Attribute("20", UIWidgets.Slider, "Laser dot max range in meters", "1 50 1")]
	protected float m_fLaserRange;

	[Attribute("0", UIWidgets.CheckBox, "Debug: show stencil orientation arrows on horizontal surfaces")]
	protected bool m_bDebugOrientation;

	protected ref Shape m_LaserDot;

	protected int m_iSizeIndex = 0;
	protected int m_iModeIndex = 0;
	protected int m_iColorIndex = 0;
	protected int m_iOpacityIndex = 0;
	protected int m_iBrightnessIndex = 0;
	protected bool m_bWasActive = false;

	//------------------------------------------------------------------------------------------------
	protected array<ref SCR_SprayColorEntry> GetCurrentColors()
	{
		if (!m_aModes || m_aModes.IsEmpty())
			return null;

		return m_aModes[m_iModeIndex].m_aColors;
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_iAmmoCount > 0)
			GetGame().GetCallqueue().CallLater(ApplyAmmoCount, 0, false, owner);

		SetEventMask(owner, EntityEvent.FRAME);
		owner.SetFlags(EntityFlags.ACTIVE, true);
	}

	//------------------------------------------------------------------------------------------------
	override protected void EOnFrame(IEntity owner, float timeSlice)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;

		IEntity player = pc.GetControlledEntity();
		if (!player)
			return;

		BaseWeaponManagerComponent weaponMgr = BaseWeaponManagerComponent.Cast(
			player.FindComponent(BaseWeaponManagerComponent));
		if (!weaponMgr)
			return;

		BaseWeaponComponent currentWeapon = weaponMgr.GetCurrentWeapon();
		if (!currentWeapon || currentWeapon.GetOwner() != owner)
		{
			m_bWasActive = false;
			return;
		}

		// This can is currently held — register as the active manager
		SCR_SprayProjectile.SetManager(this);

		// On first frame of being held, push all instance settings to the projectile statics
		if (!m_bWasActive)
		{
			m_bWasActive = true;
			bool randomRot = true;
			if (m_aModes && !m_aModes.IsEmpty())
				randomRot = m_aModes[m_iModeIndex].m_bRandomRotation;
			SCR_SprayProjectile.SetRandomRotation(randomRot);
			if (m_aPresets && !m_aPresets.IsEmpty())
				ApplyCurrentSize();
			if (GetCurrentColors() && !GetCurrentColors().IsEmpty())
				ApplyCurrentColor();
			if (m_aOpacities && !m_aOpacities.IsEmpty())
				ApplyCurrentOpacity();
			if (m_aBrightnesses && !m_aBrightnesses.IsEmpty())
				ApplyCurrentBrightness();
		}

		// Always track player facing for stencil orientation
		vector charMat[4];
		player.GetWorldTransform(charMat);
		float hx = charMat[2][0];
		float hz = charMat[2][2];
		float hLen = Math.Sqrt(hx * hx + hz * hz);
		if (hLen > 0.01)
		{
			SCR_SprayProjectile.SetPlayerForward(Vector(hx / hLen, 0, hz / hLen));
			SCR_SprayProjectile.SetPlayerYaw(Math.Atan2(hx / hLen, -(hz / hLen)) * 57.2958 + 180);
		}

		if (!m_bShowLaserDot)
			return;

		m_LaserDot = null;

		vector mat[4];
		owner.GetWorldTransform(mat);

		vector start = mat[3];
		vector dir = mat[2];
		vector end = start + dir * m_fLaserRange;

		World world = owner.GetWorld();
		if (!world)
			return;

		TraceParam trace = new TraceParam();
		trace.Start = start;
		trace.End = end;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = owner;

		float hitFraction = world.TraceMove(trace, null);
		if (hitFraction >= 1.0)
			return;

		vector hitPos = start + (end - start) * hitFraction;

		m_LaserDot = Shape.CreateSphere(ARGB(255, 255, 0, 0), ShapeFlags.ONCE | ShapeFlags.NOOUTLINE | ShapeFlags.NOZBUFFER, hitPos, 0.008);

		if (m_bDebugOrientation && Math.AbsFloat(trace.TraceNorm[1]) > 0.7)
		{
			float yaw = SCR_SprayProjectile.GetPlayerYaw();
			float rad = yaw * Math.DEG2RAD;

			// Yellow arrow: stencil "up" direction based on current angle formula
			// angle=0 is south (0,0,-1), rotates CW around Y
			vector stencilUp = Vector(Math.Sin(rad), 0, -Math.Cos(rad));
			Shape.CreateArrow(hitPos, hitPos + stencilUp * 0.4, 0.03, ARGB(255, 255, 255, 0), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);

			// Green arrow: actual player horizontal facing
			vector charMat2[4];
			player.GetWorldTransform(charMat2);
			vector playerFacing = Vector(charMat2[2][0], 0, charMat2[2][2]);
			Shape.CreateArrow(hitPos, hitPos + playerFacing * 0.4, 0.03, ARGB(255, 0, 255, 0), ShapeFlags.ONCE | ShapeFlags.NOZBUFFER);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyAmmoCount(IEntity owner)
	{
		WeaponComponent weapon = WeaponComponent.Cast(owner.FindComponent(WeaponComponent));
		if (!weapon)
			return;

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
			return;

		BaseMagazineComponent mag = muzzle.GetMagazine();
		if (mag)
			mag.SetAmmoCount(m_iAmmoCount);
	}

	//------------------------------------------------------------------------------------------------
	void CycleSize()
	{
		if (!m_aPresets || m_aPresets.IsEmpty())
			return;

		m_iSizeIndex = (m_iSizeIndex + 1) % m_aPresets.Count();
		ApplyCurrentSize();
	}

	//------------------------------------------------------------------------------------------------
	void CycleMode()
	{
		if (!m_aModes || m_aModes.IsEmpty())
			return;

		m_iModeIndex = (m_iModeIndex + 1) % m_aModes.Count();

		SCR_SprayModePreset mode = m_aModes[m_iModeIndex];
		SCR_SprayProjectile.SetRandomRotation(mode.m_bRandomRotation);

		array<ref SCR_SprayColorEntry> colors = mode.m_aColors;
		if (colors && !colors.IsEmpty())
		{
			if (m_iColorIndex >= colors.Count())
				m_iColorIndex = 0;

			ApplyCurrentColor();
		}
	}

	//------------------------------------------------------------------------------------------------
	void CycleColor()
	{
		array<ref SCR_SprayColorEntry> colors = GetCurrentColors();
		if (!colors || colors.IsEmpty())
			return;

		m_iColorIndex = (m_iColorIndex + 1) % colors.Count();
		ApplyCurrentColor();
	}

	//------------------------------------------------------------------------------------------------
	void CycleOpacity()
	{
		if (!m_aOpacities || m_aOpacities.IsEmpty())
			return;

		m_iOpacityIndex = (m_iOpacityIndex + 1) % m_aOpacities.Count();
		ApplyCurrentOpacity();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCurrentSize()
	{
		SCR_SizePreset preset = m_aPresets[m_iSizeIndex];
		SCR_SprayProjectile.SetSize(preset.m_fDecalSize, preset.m_fMinSpacing);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCurrentColor()
	{
		array<ref SCR_SprayColorEntry> colors = GetCurrentColors();
		if (!colors || colors.IsEmpty())
			return;

		SCR_SprayColorEntry entry = colors[m_iColorIndex];
		SCR_SprayProjectile.SetMaterial(entry.m_sMaterial);
	}

	//------------------------------------------------------------------------------------------------
	void CycleBrightness()
	{
		if (!m_aBrightnesses || m_aBrightnesses.IsEmpty())
			return;

		m_iBrightnessIndex = (m_iBrightnessIndex + 1) % m_aBrightnesses.Count();
		ApplyCurrentBrightness();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCurrentOpacity()
	{
		SCR_SprayProjectile.SetOpacity(m_aOpacities[m_iOpacityIndex].m_fAlpha);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyCurrentBrightness()
	{
		SCR_SprayProjectile.SetBrightness(m_aBrightnesses[m_iBrightnessIndex].m_fBrightness);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by SCR_SprayProjectile after each spray to advance to the next color
	//! when the current mode has auto-cycle enabled (e.g. stencil modes).
	void AutoCycleColor()
	{
		if (!m_aModes || m_aModes.IsEmpty())
			return;

		SCR_SprayModePreset mode = m_aModes[m_iModeIndex];
		if (!mode.m_bAutoCycle)
			return;

		array<ref SCR_SprayColorEntry> colors = mode.m_aColors;
		if (!colors || colors.Count() <= 1)
			return;

		m_iColorIndex = (m_iColorIndex + 1) % colors.Count();
		ApplyCurrentColor();
	}

	//------------------------------------------------------------------------------------------------
	bool HasModes()
	{
		return m_aModes && m_aModes.Count() > 1;
	}

	//------------------------------------------------------------------------------------------------
	bool HasColors()
	{
		array<ref SCR_SprayColorEntry> colors = GetCurrentColors();
		return colors && !colors.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	bool HasOpacities()
	{
		return m_aOpacities && !m_aOpacities.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	bool HasBrightnesses()
	{
		return m_aBrightnesses && !m_aBrightnesses.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	string GetCurrentSizeName()
	{
		if (!m_aPresets || m_aPresets.IsEmpty())
			return "Default";

		return m_aPresets[m_iSizeIndex].m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string GetCurrentModeName()
	{
		if (!m_aModes || m_aModes.IsEmpty())
			return "Default";

		return m_aModes[m_iModeIndex].m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string GetCurrentColorName()
	{
		array<ref SCR_SprayColorEntry> colors = GetCurrentColors();
		if (!colors || colors.IsEmpty())
			return "Default";

		return colors[m_iColorIndex].m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string GetCurrentOpacityName()
	{
		if (!m_aOpacities || m_aOpacities.IsEmpty())
			return "Default";

		return m_aOpacities[m_iOpacityIndex].m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string GetCurrentBrightnessName()
	{
		if (!m_aBrightnesses || m_aBrightnesses.IsEmpty())
			return "Default";

		return m_aBrightnesses[m_iBrightnessIndex].m_sName;
	}
}

//------------------------------------------------------------------------------------------------
class SCR_SprayAdjustSizeAction : ScriptedUserAction
{
	protected SCR_SpraySizeManagerComponent m_Manager;

	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Manager = SCR_SpraySizeManagerComponent.Cast(
			pOwnerEntity.FindComponent(SCR_SpraySizeManagerComponent));
	}

	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Manager)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
			character.FindComponent(CharacterControllerComponent));

		return ctrl && ctrl.GetInspectEntity() == GetOwner();
	}

	override bool CanBePerformedScript(IEntity user) { return m_Manager != null; }

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Manager)
			m_Manager.CycleSize();
	}

	override bool GetActionNameScript(out string outName)
	{
		if (!m_Manager)
			return false;

		outName = string.Format("Spray Size: %1", m_Manager.GetCurrentSizeName());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class SCR_SprayCycleColorAction : ScriptedUserAction
{
	protected SCR_SpraySizeManagerComponent m_Manager;

	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Manager = SCR_SpraySizeManagerComponent.Cast(
			pOwnerEntity.FindComponent(SCR_SpraySizeManagerComponent));
	}

	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Manager || !m_Manager.HasColors())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
			character.FindComponent(CharacterControllerComponent));

		return ctrl && ctrl.GetInspectEntity() == GetOwner();
	}

	override bool CanBePerformedScript(IEntity user) { return m_Manager != null; }

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Manager)
			m_Manager.CycleColor();
	}

	override bool GetActionNameScript(out string outName)
	{
		if (!m_Manager)
			return false;

		outName = string.Format("%1: %2", m_Manager.GetCurrentModeName(), m_Manager.GetCurrentColorName());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class SCR_SprayCycleOpacityAction : ScriptedUserAction
{
	protected SCR_SpraySizeManagerComponent m_Manager;

	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Manager = SCR_SpraySizeManagerComponent.Cast(
			pOwnerEntity.FindComponent(SCR_SpraySizeManagerComponent));
	}

	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Manager || !m_Manager.HasOpacities())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
			character.FindComponent(CharacterControllerComponent));

		return ctrl && ctrl.GetInspectEntity() == GetOwner();
	}

	override bool CanBePerformedScript(IEntity user) { return m_Manager != null; }

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Manager)
			m_Manager.CycleOpacity();
	}

	override bool GetActionNameScript(out string outName)
	{
		if (!m_Manager)
			return false;

		outName = string.Format("Spray Opacity: %1", m_Manager.GetCurrentOpacityName());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class SCR_SprayCycleBrightnessAction : ScriptedUserAction
{
	protected SCR_SpraySizeManagerComponent m_Manager;

	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Manager = SCR_SpraySizeManagerComponent.Cast(
			pOwnerEntity.FindComponent(SCR_SpraySizeManagerComponent));
	}

	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Manager || !m_Manager.HasBrightnesses())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
			character.FindComponent(CharacterControllerComponent));

		return ctrl && ctrl.GetInspectEntity() == GetOwner();
	}

	override bool CanBePerformedScript(IEntity user) { return m_Manager != null; }

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Manager)
			m_Manager.CycleBrightness();
	}

	override bool GetActionNameScript(out string outName)
	{
		if (!m_Manager)
			return false;

		outName = string.Format("Spray Brightness: %1", m_Manager.GetCurrentBrightnessName());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class SCR_SprayCycleModeAction : ScriptedUserAction
{
	protected SCR_SpraySizeManagerComponent m_Manager;

	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Manager = SCR_SpraySizeManagerComponent.Cast(
			pOwnerEntity.FindComponent(SCR_SpraySizeManagerComponent));
	}

	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Manager || !m_Manager.HasModes())
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (!character)
			return false;

		CharacterControllerComponent ctrl = CharacterControllerComponent.Cast(
			character.FindComponent(CharacterControllerComponent));

		return ctrl && ctrl.GetInspectEntity() == GetOwner();
	}

	override bool CanBePerformedScript(IEntity user) { return m_Manager != null; }

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (m_Manager)
			m_Manager.CycleMode();
	}

	override bool GetActionNameScript(out string outName)
	{
		if (!m_Manager)
			return false;

		outName = string.Format("Spray Mode: %1", m_Manager.GetCurrentModeName());
		return true;
	}
}
