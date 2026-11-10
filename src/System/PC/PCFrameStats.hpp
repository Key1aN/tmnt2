#pragma once

#if defined(TMNT2_DEBUG_TOOLS)

// Real (wall-clock) frame timing for Debug Tools.
//
// CScreen::Framerate() is the configured refresh rate divided by the vsync
// interval and never changes at runtime, so it cannot show slowdowns. This
// recorder measures each main-loop iteration (Move -> Render -> Flip) with the
// high-resolution performance counter and keeps a short history.
class CPCFrameStats
{
public:
    struct SAMPLE
    {
        float  m_fFrameMs;      // end of previous frame -> end of this frame
        float  m_fUpdateMs;     // Framework().Move()  (game logic)
        float  m_fRenderMs;     // Framework().Render() (draw submission)
        float  m_fPresentMs;    // Framework().Flip()   (present / vsync wait)
        bool   m_bDrawn;        // false when the frame-skip controller skipped drawing
        uint32 m_uIndex;
    };

    enum
    {
        HISTORY = 256,
    };

    static void FrameBegin(void);
    static void UpdateDone(void);
    static void SetDrawSkipped(bool bSkipped);
    static void RenderDone(void);
    static void FrameEnd(void);

    static uint32 GetFrameIndex(void);          // number of completed frames
    static int32 GetHistoryCount(void);
    static const SAMPLE& GetSample(int32 ago);  // 0 = most recent completed frame
};


// Append-only text logs beside TMNT2.exe for Debug Tools (TMNT2_perf.log,
// TMNT2_moves.log). Lines are buffered and flushed at least twice per second.
class CPCDebugLogFile
{
public:
    explicit CPCDebugLogFile(const char* pszFileName);
    ~CPCDebugLogFile(void);
    void Write(const char* pszFormat, ...);
    void FlushIfDue(void);
    void Flush(void);

private:
    bool Open(void);

    char  m_szFileName[64];
    void* m_pFile;
    bool  m_bOpenFailed;
    uint64 m_uLastFlush;
};

#endif /* defined(TMNT2_DEBUG_TOOLS) */
