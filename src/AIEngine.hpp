    std::atomic<bool>     m_enabled       { false };
    std::atomic<bool>     m_sessionActive { false };

    // Hazard cache — rebuilt every SCAN_INTERVAL_FRAMES frames
    std::vector<HazardInfo> m_hazards;
    int                     m_frameCounter { 0 };
    static constexpr int    SCAN_INTERVAL_FRAMES = 3; // rescan every 3 frames

    // Human-timing helper
    HumanTimer  m_humanTimer;
    bool        m_jumpScheduled   { false };
    bool        m_holdingJump     { false };

    // Settings cache (re-read each session start for performance)
    float m_lookahead           { 320.f };
    float m_framePerfectThresh  { 22.f  };
    int   m_humanVarianceMs     { 15    };
};

} // namespace GDAI
