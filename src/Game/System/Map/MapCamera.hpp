#pragma once


class CCamera;


class CMapCamera
{
public:
    class CIntroduction;
    class CVibration;
    
    enum MODE
    {
        MODE_MANUAL = 0,
        MODE_AUTOCHANGE,
        MODE_INTRODUCTION,

        MODEMAX,
    };
    
    enum PATHMODE
    {
        PATHMODE_SINGLEPLAYER = 0,
        PATHMODE_MULTIPLAYER,

        PATHMODEMAX,
    };

    /*
     *  Enhanced framing ("nice zoom" replacement).
     *
     *  All values are tunable at runtime (see debug menu) and m_bEnable is
     *  the master A/B switch - when it is false every code path below
     *  behaves exactly like the original camera.
     */
    struct TUNING
    {
        bool  m_bEnable;            /* master switch: false = original camera            */
        float m_fSafeMargin;        /* fraction of half screen targets must stay inside  */
        float m_fReturnMargin;      /* tighter margin that must be met before zooming in */
        float m_fSecondaryMargin;   /* margin for secondary targets (bosses)             */
        float m_fZoomMax;           /* max dolly multiplier for path cameras             */
        float m_fFovZoomMax;        /* max view window widening (set/fixed/blocked)      */
        float m_fZoomOutSpeed;      /* 1/sec, exponential                                */
        float m_fZoomInSpeed;       /* 1/sec, exponential                                */
        float m_fZoomInDelay;       /* sec to wait before returning to closer framing    */
        float m_fTargetHalfHeight;  /* vertical extent of player around body position    */
        float m_fJumpWindow;        /* height player may gain before lookat follows      */
        float m_fGroundFollowSpeed; /* 1/sec, lookat height catch-up after landing       */
        float m_fLookAheadDist;     /* max single player look-ahead, world units (0=off) */
        float m_fLookAheadTime;     /* sec of movement to look ahead                     */
        float m_fLookAheadSpeed;    /* 1/sec, exponential                                */
        float m_fBossWeightMax;     /* max pull of bosses on the lookat point (0..1)     */
        float m_fBossRange;         /* bosses farther than this stop pulling the lookat  */
    };

    static const int32 FRAMETARGET_MAX = 8;

    struct FRAMETARGET
    {
        RwV3d m_vPos;
        float m_fUp;                /* extent above m_vPos */
        float m_fDown;              /* extent below m_vPos */
        bool  m_bPrimary;           /* players = true, bosses = false */
    };

    /* state of last framed update, for diagnostics only */
    struct FRAMEDIAG
    {
        bool  m_bFraming;
        int32 m_nNumTarget;
        int32 m_nNumOutside;        /* targets outside the safe area */
        float m_fZoom;              /* current smoothed dolly zoom   */
        float m_fZoomRequest;       /* unclamped requested zoom      */
        float m_fFovZoom;           /* current view window scale     */
        float m_fMoveRadius;
        float m_afScreenX[FRAMETARGET_MAX]; /* -1..1 across the screen */
        float m_afScreenY[FRAMETARGET_MAX];
    };

public:
    static float m_fDefaultViewSize;
    static TUNING m_tuning;

    static void ResetTuning(void);
    static const TUNING& DefaultTuning(void);
    static bool IsEnhanced(void);   /* m_bEnable and, on PC, Debug Tools mod is switched on */
    
    CMapCamera(void);
    virtual ~CMapCamera(void);
    bool BeginScene(void);
    void EndScene(void);
    void WorldAddCamera(RpWorld* pWorld);
    void WorldRemoveCamera(RpWorld* pWorld);
    void Update(const RwV3d* pvAt);
    void Update(const RwV3d* pvAt, float fZoom);
    void UpdateFraming(const RwV3d* pvAt, const FRAMETARGET* aTarget, int32 nNumTarget, float fZoomScale);
    void PostUpdate(void);
    void UpdateMoment(const RwV3d* pvAt);
    void UpdateAutoChangeCamera(const RwV3d* pvAt);
    void ResetCameraForCameraAreaChange(const RwV3d* pvAt);
    void MakePathAtCameraName(char* pszPathAtName, const char* pszCameraAreaName);
    void MakePathEyeCameraName(char* pszPathEyeName, const char* pszCameraAreaName);
    void MakeSetCameraName(char* pszSetCameraName, const char* pszCameraAreaName);
    void UpdateManualCamera(const RwV3d* pvAt);
    void UpdatePathCamera(const RwV3d* pvAt, bool bExPath);
    void UpdateSetCamera(const RwV3d* pvAt);
    void UpdateFixedCamera(const RwV3d* pvAt);
    void UpdateLookat(void);
    void LookAt(const RwV3d* pvEye, const RwV3d* pvAt, const RwV3d* pvUp);
    void Skip(void);
    void SetLookat(const RwV3d* pvEye, const RwV3d* pvAt);
    void SetCameraVibration(float fPower, float fTime, int32 nFreq);
    void StopCameraVibration(void);
    void SetFarClipPlane(float fFarClipPlane);
    void SetNearClipPlane(float fNearClipPlane);
    void SetFogDistance(float fFogDist);
    void SetViewWindow(float fViewSize);
    void SetViewWindow(float fViewSize, float fAspect);
    void SetCameraMode(MODE mode);
    MODE GetCameraMode(void) const;
    void SetPathMode(PATHMODE pathmode);
#if defined(TMNT2_DEBUG_TOOLS)
    void DebugBeginShowcase(void);
    void DebugResetShowcase(PATHMODE pathmode);
#endif /* defined(TMNT2_DEBUG_TOOLS) */
    bool IsPosVisible(const RwV3d* pvPos);
    float CalcNiceZoom(RwV3d* avPos, int32 nNumPos);
    RwCamera* GetRwCamera(void) const;
    bool IsIntroductionEnd(void) const;
    float GetViewRatio(void) const;
    void GetEye(RwV3d* pvEye) const;
    void GetLookat(RwV3d* pvLookat) const;
    float GetViewAreaRadius(void) const;
    bool FrustumSphereTest(const RwV3d* pos, float fRadius) const;
    bool FrustumSphereTest(const RwSphere* sphere) const;
    const FRAMEDIAG& GetFrameDiag(void) const;
    const char* GetCameraAreaName(void) const;

private:
    struct FRAMEBASIS;

    bool IsFramingActive(void) const;
    bool MakeFrameBasis(FRAMEBASIS* pBasis, const RwV3d* pvEye, const RwV3d* pvAt, float fViewSize) const;
    float CalcFrameDollyZoom(const FRAMEBASIS& rBasis, float fMargin) const;
    float CalcFrameExtent(const FRAMEBASIS& rBasis, float fDollyZoom, float fMargin, bool bDiag);
    float CalcFrameReach(const FRAMEBASIS& rBasis, float fDollyZoom, float fFovZoom) const;
    void StepFrameZoom(float* pfZoom, float fRequestOut, float fRequestIn, float fZoomMax);
    void IntroductionCreate(void);
    void IntroductionDestroy(void);
    void IntroductionUpdate(void);
    void VibrationCreate(float fPower, float fTime, int32 nFreq);
    void VibrationDestroy(void);
    void VibrationUpdate(void);

private:
    static float m_fDefaultClipFar;
    static float m_fDefaultClipNear;
    static float m_fDefaultFogDist;
    static RwV3d m_vDefaultLookat;
    static RwV3d m_vDefaultEye;
    CCamera* m_pCamera;
    RwV3d m_vEye;
    RwV3d m_vAt;
    float m_fDist;
    float m_fViewSize;
    float m_fRotY;
    float m_fHeight;
    float m_fLookatOffsetY;
    float m_fPathTime;
    float m_fPrePathTime;
    float m_fLookatViewAreaRadius;
    float m_fChangeTimer;
    float m_fRequestZoom;
    MODE m_mode;
    PATHMODE m_pathmode;
    char m_szCameraAreaName[32];
    bool m_bChangeMoment;
    CIntroduction* m_pIntroduction;
    CVibration* m_pVibration;
    FRAMETARGET m_aFrameTarget[FRAMETARGET_MAX];
    int32 m_nNumFrameTarget;
    bool m_bFramingRequest;
    float m_fFrameZoom;
    float m_fFrameFovZoom;
    float m_fFrameZoomHold;
    float m_fFrameReach;
    FRAMEDIAG m_frameDiag;
};
