"use strict";

function togglePlayBoot() {
      APP_STATE.isPlaying = !APP_STATE.isPlaying;
      singleCycleActive = false;
      updatePlayButtonUI();
    }

    function updatePlayButtonUI() {
      const icon = document.getElementById('iconPlayPause');
      if (APP_STATE.isPlaying) {
        icon.innerHTML = '<rect x="6" y="4" width="4" height="16"></rect><rect x="14" y="4" width="4" height="16"></rect>';
      } else {
        icon.innerHTML = '<polygon points="5 3 19 12 5 21 5 3"></polygon>';
      }
    }

    function triggerSingleCycle() {
      setBootProgress(0);
      APP_STATE.isPlaying = true;
      singleCycleActive = true;
      updatePlayButtonUI();
      logTerminal('[SIM] Triggered single-cycle boot (0% -> 100%).', 'info');
    }

    function restartBootAnimation() {
      setBootProgress(0);
      APP_STATE.isPlaying = true;
      singleCycleActive = false;
      updatePlayButtonUI();
      sendSerialCmd('restart');
    }

    function triggerHandoff() {
      sendSerialCmd('handoff');
    }

    function onScrubberInput(val) {
      APP_STATE.isPlaying = false;
      singleCycleActive = false;
      updatePlayButtonUI();
      setBootProgress(parseFloat(val));
    }

    function setBootProgress(val) {
      APP_STATE.bootProgress = Math.max(0, Math.min(100, val));
      document.getElementById('rngBootProgress').value = Math.round(APP_STATE.bootProgress);
      updateBootStateDisplay(APP_STATE.bootProgress);
    }

    function updateBootStateDisplay(p) {
      const lbl = document.getElementById('lblBootState');
      const val = document.getElementById('valBootProgress');
      val.textContent = `${Math.round(p)}%`;

      if (p === 0) {
        lbl.textContent = 'BOOT_STATE_INIT';
        lbl.style.color = 'var(--text-dim)';
      } else if (p < APP_STATE.thresholdPercent) {
        lbl.textContent = 'BOOT_STATE_BOOTING';
        lbl.style.color = 'var(--accent-ice)';
      } else if (p < 100) {
        lbl.textContent = 'BOOT_STATE_COMPLETING';
        lbl.style.color = 'var(--accent-amber-soft)';
      } else {
        lbl.textContent = 'BOOT_STATE_READY';
        lbl.style.color = 'var(--accent-emerald-soft)';
      }
    }

    function setBrightness(val) {
      APP_STATE.brightness = parseInt(val, 10);
      document.getElementById('valBrightness').textContent = `${APP_STATE.brightness}%`;
      const filterVal = (APP_STATE.brightness / 100);
      canvas.style.filter = `brightness(${filterVal})`;
    }

    function setEngine(mode) {
      APP_STATE.engine = mode;
      document.getElementById('btnEngineLp').classList.toggle('active', mode === 'lp');
      document.getElementById('btnEngineHp').classList.toggle('active', mode === 'hp');
      logTerminal(`[ENGINE] Display engine set to: ${mode === 'lp' ? 'LP-Core Autonomous' : 'HP-Core DMA SPI'}`, 'info');
      updateCodeOutput();
    }

    /* ==========================================================================
     * 7. Preset Templates & Layer Management
     * ========================================================================== */
    function clearAllLayers() {
      APP_STATE.layers = [];
      APP_STATE.selectedLayerId = null;
      renderLayerList();
      renderLayerInspector();
      updateCodeOutput();
      logTerminal('[COMPOSER] Cleared all layers. Canvas is blank.', 'info');
    }

    function loadPresetTemplate(templateKey) {
      if (!templateKey) return;

      if (templateKey === 'blank') {
        clearAllLayers();
      } else if (templateKey === 'mascot_ring') {
        // Preset: Retro Mascot + Progress Ring + Arc Wordmark
        APP_STATE.layers = [
          {
            id: 'layer_' + Date.now() + '_1',
            type: 'gauge',
            name: 'Outer Progress Ring',
            visible: true,
            radius: 112,
            thickness: 8,
            style: 'dots',
            segments: 48,
            startAngle: 270,
            spanAngle: 360,
            color: '#00d4ff',
            trackColor: 'rgba(255, 255, 255, 0.12)'
          },
          {
            id: 'layer_' + Date.now() + '_2',
            type: 'sprite',
            name: 'Retro Mascot',
            visible: true,
            w: 16,
            h: 16,
            scale: 3,
            offsetX: 0,
            offsetY: -6,
            rotation: 0,
            modifier: 'eye_track',
            pixels: createMascotPixelArray()
          },
          {
            id: 'layer_' + Date.now() + '_3',
            type: 'text',
            name: 'Wordmark Arc',
            visible: true,
            text: 'ECLIPSE',
            pathMode: 'arc',
            radius: 88,
            startAngle: 90,
            letterSpacing: 8,
            fontSize: 12,
            color: '#ffffff',
            inward: true,
            flip: false,
            direction: 'ltr'
          }
        ];
        APP_STATE.selectedLayerId = APP_STATE.layers[1].id;
        logTerminal('[TEMPLATE] Loaded "Retro Mascot & Ring" template.', 'info');
      } else if (templateKey === 'minimal_gauge') {
        // Preset: Minimal Arc Gauge + Dynamic Metrics
        APP_STATE.layers = [
          {
            id: 'layer_' + Date.now() + '_1',
            type: 'gauge',
            name: 'Speedometer Arc',
            visible: true,
            radius: 104,
            thickness: 6,
            style: 'solid',
            segments: 36,
            startAngle: 135,
            spanAngle: 270,
            color: '#38bdf8',
            trackColor: 'rgba(255, 255, 255, 0.1)'
          },
          {
            id: 'layer_' + Date.now() + '_2',
            type: 'text',
            name: 'Metrics Percentage',
            visible: true,
            text: 'BOOTING {progress}%',
            pathMode: 'straight',
            fontSize: 16,
            color: '#ffffff',
            offsetX: 0,
            offsetY: -8,
            rotation: 0,
            modifier: 'pulse'
          },
          {
            id: 'layer_' + Date.now() + '_3',
            type: 'text',
            name: 'Sub-Status',
            visible: true,
            text: 'LP-CORE READY',
            pathMode: 'straight',
            fontSize: 10,
            color: '#94a3b8',
            offsetX: 0,
            offsetY: 22,
            rotation: 0,
            modifier: 'none'
          }
        ];
        APP_STATE.selectedLayerId = APP_STATE.layers[0].id;
        logTerminal('[TEMPLATE] Loaded "Minimal Arc Gauge & Metrics" template.', 'info');
      } else if (templateKey === 'cyberpunk') {
        // Preset: Cyberpunk Dial + Ticks + Chip
        APP_STATE.layers = [
          {
            id: 'layer_' + Date.now() + '_1',
            type: 'gauge',
            name: 'Outer Ticks Dial',
            visible: true,
            radius: 110,
            thickness: 8,
            style: 'ticks',
            segments: 36,
            startAngle: 0,
            spanAngle: 360,
            color: '#f59e0b',
            trackColor: 'rgba(255, 255, 255, 0.12)'
          },
          {
            id: 'layer_' + Date.now() + '_2',
            type: 'gauge',
            name: 'Inner Progress Arc',
            visible: true,
            radius: 76,
            thickness: 4,
            style: 'solid',
            segments: 24,
            startAngle: 180,
            spanAngle: 180,
            color: '#ec4899',
            trackColor: 'rgba(255, 255, 255, 0.08)'
          },
          {
            id: 'layer_' + Date.now() + '_3',
            type: 'text',
            name: 'Status Arc Text',
            visible: true,
            text: 'INITIALIZING',
            pathMode: 'arc',
            radius: 92,
            startAngle: 90,
            letterSpacing: 6,
            fontSize: 10,
            color: '#e2e8f0',
            inward: true,
            flip: false,
            direction: 'ltr'
          }
        ];
        APP_STATE.selectedLayerId = APP_STATE.layers[0].id;
        logTerminal('[TEMPLATE] Loaded "Cyberpunk Dial & Ticks" template.', 'info');
      }

      document.getElementById('selTemplate').value = '';
      renderLayerList();
      renderLayerInspector();
      updateCodeOutput();
    }
