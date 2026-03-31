[BaseContainerProps()]
class SCR_SprayPaintDecalEffect : BaseProjectileEffect
{
	//! Paint decal material (.emat) to project onto the hit surface
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Paint decal material (.emat)", "emat")]
	protected ResourceName m_sPaintMaterial;

	//! Size of the decal in meters
	[Attribute("0.3", UIWidgets.Slider, "Decal size in meters", "0.05 2 0.01")]
	protected float m_fDecalSize;

	//! How long the decal stays visible (seconds)
	[Attribute("30", UIWidgets.EditBox, "Decal lifetime in seconds")]
	protected float m_fDecalLifetime;

	//------------------------------------------------------------------------------------------------
	override void OnEffect(IEntity pHitEntity, inout vector outMat[3], IEntity damageSource, notnull Instigator instigator, string colliderName, float speed)
	{
		if (!m_sPaintMaterial || m_sPaintMaterial.IsEmpty())
			return;

		// pHitEntity is null for terrain/world geometry — skip in that case
		if (!pHitEntity)
			return;

		if (!damageSource)
			return;

		World world = damageSource.GetWorld();
		if (!world)
			return;

		// Use the projectile's position at impact for hit location
		// outMat[2] is the surface normal (Z-axis of the hit matrix)
		vector hitPos    = damageSource.GetOrigin();
		vector hitNormal = outMat[2];

		world.CreateTrackDecal(pHitEntity, hitPos, hitNormal, m_fDecalSize, m_fDecalLifetime, m_sPaintMaterial, null, 1.0);
	}
}
