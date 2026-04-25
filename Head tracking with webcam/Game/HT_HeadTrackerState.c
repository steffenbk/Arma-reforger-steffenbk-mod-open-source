class HT_PoseData : JsonApiStruct
{
	float yaw;
	float pitch;
	float roll;
	bool active;

	void HT_PoseData()
	{
		RegV("yaw");
		RegV("pitch");
		RegV("roll");
		RegV("active");
	}
}

class HT_HeadTrackerState
{
	static const int   PORT              = 7837;
	static const float POLL_INTERVAL     = 0.05; // 20 Hz
	static const float YAW_SENSITIVITY   = 1.0;
	static const float PITCH_SENSITIVITY = 1.0;
	static const float ROLL_SENSITIVITY  = 1.0;
	static const float DEADZONE          = 2.0;
	static const float SMOOTH_RATE       = 8.0;
	static const float APPLIED_SMOOTH_RATE = 6.0;

	bool m_bEnableInVehicles = true;  // toggle from UI
	static const bool  DEBUG_LOG         = true;
	static const string DEBUG_LOG_PATH   = "$profile:head_tracker_debug.log";

	float m_fYaw     = 0.0;  // smoothed (used by camera)
	float m_fPitch   = 0.0;
	float m_fRoll    = 0.0;
	bool  m_bActive  = false;
	bool  m_bEnabled = true;

	protected float            m_fTargetYaw   = 0.0;  // latest sample from server
	protected float            m_fTargetPitch = 0.0;
	protected float            m_fTargetRoll  = 0.0;
	protected RestContext      m_RestContext;
	protected ref RestCallback m_Callback;
	protected ref RestCallback m_CmdCallback;
	protected float            m_fPollTimer  = 0.0;
	protected bool             m_bPending    = false;  // prevent request flooding
	protected FileHandle       m_DebugFile;

	protected static ref HT_HeadTrackerState s_Instance;

	static HT_HeadTrackerState GetInstance()
	{
		return s_Instance;
	}

	static HT_HeadTrackerState GetOrCreate()
	{
		if (!s_Instance)
		{
			s_Instance = new HT_HeadTrackerState();
			s_Instance.Init();
		}
		return s_Instance;
	}

	static void Reset()
	{
		s_Instance = null;
	}

	protected void Init()
	{
		RestApi restApi = GetGame().GetRestApi();
		if (!restApi)
			return;
		m_RestContext = restApi.GetContext("http://localhost:" + PORT.ToString());
		m_Callback = new RestCallback();
		m_Callback.SetOnSuccess(OnPoseSuccess);
		m_Callback.SetOnError(OnPoseError);
		m_CmdCallback = new RestCallback();
		if (DEBUG_LOG)
		{
			m_DebugFile = FileIO.OpenFile(DEBUG_LOG_PATH, FileMode.WRITE);
			DebugWrite("=== HT_HeadTrackerState init ===");
		}
	}

	protected void DebugWrite(string msg)
	{
		if (!m_DebugFile)
			return;
		float t = System.GetTickCount() / 1000.0;
		m_DebugFile.WriteLine(t.ToString() + " " + msg);
	}

	void OnPoseSuccess()
	{
		m_bPending = false;
		string data = m_Callback.GetData();
		HT_PoseData pose = new HT_PoseData();
		pose.ExpandFromRAW(data);
		m_fTargetYaw   = pose.yaw;
		m_fTargetPitch = pose.pitch;
		m_fTargetRoll  = pose.roll;
		m_bActive      = pose.active;
		DebugWrite("RX yaw=" + pose.yaw.ToString() + " pitch=" + pose.pitch.ToString() + " roll=" + pose.roll.ToString() + " active=" + pose.active.ToString());
	}

	void OnPoseError()
	{
		m_bPending = false;
		m_bActive  = false;
		DebugWrite("RX error");
	}

	void Update(float dt)
	{
		if (!m_RestContext)
			return;

		// Smooth toward latest sample (frame-rate independent lerp)
		float a = Math.Clamp(SMOOTH_RATE * dt, 0.0, 1.0);
		m_fYaw   = Math.Lerp(m_fYaw,   m_fTargetYaw,   a);
		m_fPitch = Math.Lerp(m_fPitch, m_fTargetPitch, a);
		m_fRoll  = Math.Lerp(m_fRoll,  m_fTargetRoll,  a);

		if (m_bPending)
			return;
		m_fPollTimer -= dt;
		if (m_fPollTimer <= 0)
		{
			m_fPollTimer = POLL_INTERVAL;
			m_bPending   = true;
			m_RestContext.GET(m_Callback, "/pose");
		}
	}

	protected float SoftDeadzone(float v)
	{
		// Continuous deadzone: stays 0 up to threshold, then ramps smoothly.
		// Prevents the sudden 0→DEADZONE jump at the boundary.
		float a = Math.AbsFloat(v);
		if (a < DEADZONE)
			return 0.0;
		float sign = 1.0;
		if (v < 0)
			sign = -1.0;
		return sign * (a - DEADZONE);
	}

	float GetYaw()
	{
		if (!m_bEnabled || !m_bActive)
			return 0.0;
		return SoftDeadzone(m_fYaw * YAW_SENSITIVITY);
	}

	float GetPitch()
	{
		if (!m_bEnabled || !m_bActive)
			return 0.0;
		return SoftDeadzone(m_fPitch * PITCH_SENSITIVITY);
	}

	float GetRoll()
	{
		if (!m_bEnabled || !m_bActive)
			return 0.0;
		return SoftDeadzone(m_fRoll * ROLL_SENSITIVITY);
	}

	// Shared applied-angle smoothing state (one camera active at a time)
	protected float m_fAppliedYaw   = 0.0;
	protected float m_fAppliedPitch = 0.0;
	protected float m_fAppliedRoll  = 0.0;

	// Applies head tracking to a camera's transform. Called by modded camera classes
	// AFTER their super.OnUpdate has populated pOutResult.m_CameraTM.
	// adsScale: 1.0 = full, 0.25 = zoomed/ADS
	// Returns true if rotation was applied (caller should assert freelook then).
	bool ApplyToCamera(float pDt, out vector camTM[4], float adsScale)
	{
		float tY = GetYaw()   * adsScale;
		float tP = GetPitch() * adsScale;
		float tR = GetRoll()  * adsScale;

		float a = Math.Clamp(APPLIED_SMOOTH_RATE * pDt, 0.0, 1.0);
		m_fAppliedYaw   = Math.Lerp(m_fAppliedYaw,   tY, a);
		m_fAppliedPitch = Math.Lerp(m_fAppliedPitch, tP, a);
		m_fAppliedRoll  = Math.Lerp(m_fAppliedRoll,  tR, a);

		if (Math.AbsFloat(m_fAppliedYaw) < 0.05
		 && Math.AbsFloat(m_fAppliedPitch) < 0.05
		 && Math.AbsFloat(m_fAppliedRoll) < 0.05)
			return false;

		vector addMat[4];
		Math3D.AnglesToMatrix(Vector(m_fAppliedYaw, m_fAppliedPitch, m_fAppliedRoll), addMat);
		Math3D.MatrixMultiply4(addMat, camTM, camTM);
		return true;
	}

	void PostCommand(string path)
	{
		if (!m_RestContext)
			return;
		m_RestContext.POST(m_CmdCallback, path, "");
		DebugWrite("POST " + path);
	}

	bool IsTracking()
	{
		return m_bEnabled && m_bActive;
	}
}
