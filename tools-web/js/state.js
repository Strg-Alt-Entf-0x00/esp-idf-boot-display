"use strict";

/* ==========================================================================
     * 1. Application State & Universal Mini-LVGL Engine
     * ========================================================================== */
    const APP_STATE = {
      engine: 'lp',            // 'lp' (ULP Autonomous) or 'hp' (DMA SPI)
      brightness: 100,         // 0..100%
      bootProgress: 0,         // 0..100%
      bootMode: 3,             // 0=CONTINUOUS, 1=STATIC_FRAME, 2=SINGLE_CYCLE, 3=EVENT_DRIVEN
      thresholdPercent: 85,    // Trigger celebration
      holdDurationMs: 600,     // Hold completion screen
      isPlaying: false,
      selectedLayerId: null,

      // Global Canvas Background
      bgColor: '#000000',

      // Universal Layer Stack (Initialized completely blank as requested by user)
      layers: []
    };

    // Canvas & Stage references
    let canvas, ctx;
    let animFrameId = null;
    let lastTimestamp = 0;
    let singleCycleActive = false;

    // Web Serial State
    let serialPort = null;
    let serialReader = null;
    let isConnected = false;

    // Simulator State
    let simTimer = null;
    let simCycles = 0;
    let simFrames = 0;

    // Canvas Interactive Manipulation State
    let isCanvasDragging = false;
    let canvasDragMode = null; // 'move' | 'resize' | 'radius'
    let canvasDragTarget = null;
    let canvasDragStartMouse = { x: 0, y: 0 };
    let canvasDragStartVals = {};
    let snapCrosshair = { x: null, y: null };

    /* ==========================================================================
     * 2. In-Browser Pixel & Sprite Studio (Mini Paint)
     * ========================================================================== */
    const PIXEL_EDITOR = {
      gridW: 24,
      gridH: 24,
      pixels: [],            // Array of length gridW * gridH (hex color string or null for transparent)
      currentTool: 'pen',    // 'pen' | 'eraser' | 'bucket' | 'picker'
      currentColor: '#00d4ff',
      isDrawing: false,
      targetLayerId: null,   // null if creating new, or layer id if editing existing
      canvas: null,
      ctx: null,
      previewCanvas: null,
      previewCtx: null,
      palette: [
        '#ffffff', '#000000', '#00d4ff', '#38bdf8', '#3b82f6', '#6366f1',
        '#a855f7', '#ec4899', '#ef4444', '#f97316', '#eab308', '#22c55e',
        '#10b981', '#64748b', '#334155', '#1e293b'
      ]
    };
