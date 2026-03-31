// BDS_UnconsciousComponentClass.c
// Keeps characters in ragdoll pose while unconscious instead of blending to animation.
// RefreshRagdoll must be called only on the entity owner (server for AI, owning client for players).
// Ragdoll state is replicated to other clients by the engine automatically.

modded class SCR_CharacterDamageManagerComponent
{
	protected bool m_bBDS_KeepingRagdoll = false;

	protected static const float REFRESH_INTERVAL_MS = 1000.0;

	//------------------------------------------------------------------------------------------------
	override void OnLifeStateChanged(ECharacterLifeState previousLifeState, ECharacterLifeState newLifeState, bool isJIP)
	{
		super.OnLifeStateChanged(previousLifeState, newLifeState, isJIP);

		// Only run on the entity owner -- RefreshRagdoll must be called on owner only
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl && !rpl.IsOwner())
			return;

		if (newLifeState == ECharacterLifeState.INCAPACITATED)
		{
			StartKeepingRagdoll();
		}
		else if (m_bBDS_KeepingRagdoll)
		{
			StopKeepingRagdoll();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void StartKeepingRagdoll()
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(GetOwner());
		if (!character)
			return;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return;

		m_bBDS_KeepingRagdoll = true;

		// Start with long timeout, loop will maintain it
		controller.RefreshRagdoll(10000.0);

		GetGame().GetCallqueue().CallLater(RefreshRagdollLoop, REFRESH_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopKeepingRagdoll()
	{
		if (!m_bBDS_KeepingRagdoll)
			return;

		m_bBDS_KeepingRagdoll = false;
		GetGame().GetCallqueue().Remove(RefreshRagdollLoop);

		// Immediately expire ragdoll so vanilla wake-up animation can play
		ChimeraCharacter character = ChimeraCharacter.Cast(GetOwner());
		if (character)
		{
			CharacterControllerComponent controller = character.GetCharacterController();
			if (controller)
				controller.RefreshRagdoll(0.0);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshRagdollLoop()
	{
		if (!m_bBDS_KeepingRagdoll)
		{
			StopKeepingRagdoll();
			return;
		}

		ChimeraCharacter character = ChimeraCharacter.Cast(GetOwner());
		if (!character)
		{
			StopKeepingRagdoll();
			return;
		}

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
		{
			StopKeepingRagdoll();
			return;
		}

		if (controller.GetLifeState() != ECharacterLifeState.INCAPACITATED)
		{
			StopKeepingRagdoll();
			return;
		}

		controller.RefreshRagdoll(3000.0);
	}
}
