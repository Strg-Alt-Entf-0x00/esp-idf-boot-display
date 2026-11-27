"use strict";

async function toggleConnection() {
      if (isConnected) {
        disconnectSerial();
      } else {
        connectSerial();
      }
    }

    async function connectSerial() {
      if (!('serial' in navigator)) {
        logTerminal('[WARN] Web Serial not supported in this browser. Running in Offline Hardware Emulation mode.', 'err');
        setUIConnected(true, true);
        return;
      }

      try {
        const btn = document.getElementById('btnConnect');
        btn.className = 'icon-btn connecting';

        serialPort = await navigator.serial.requestPort();
        await serialPort.open({ baudRate: 115200 });

        setUIConnected(true, false);
        logTerminal('[SERIAL] Connected to ESP32 device @ 115200 Baud.', 'rx');

        readSerialLoop();
      } catch (err) {
        logTerminal(`[SERIAL] Connection failed: ${err.message}`, 'err');
        setUIConnected(false, false);
      }
    }

    async function disconnectSerial() {
      if (serialReader) {
        try { await serialReader.cancel(); } catch (e) {}
      }
      if (serialPort) {
        try { await serialPort.close(); } catch (e) {}
      }
      setUIConnected(false, false);
      logTerminal('[SERIAL] Disconnected from ESP32.', 'info');
    }

    function setUIConnected(connected, simulated) {
      isConnected = connected;
      const btn = document.getElementById('btnConnect');
      if (connected) {
        btn.className = 'icon-btn connected';
        btn.setAttribute('data-tooltip', simulated ? 'Simulated ESP32 (Offline Mode)' : 'Connected ESP32 (115200 Baud)');
      } else {
        btn.className = 'icon-btn';
        btn.setAttribute('data-tooltip', 'Connect ESP32 (115200 Baud)');
      }
    }

    async function readSerialLoop() {
      const textDecoder = new TextDecoderStream();
      serialPort.readable.pipeTo(textDecoder.writable);
      serialReader = textDecoder.readable.getReader();

      let buffer = '';
      try {
        while (true) {
          const { value, done } = await serialReader.read();
          if (done) break;
          buffer += value;
          const lines = buffer.split('\n');
          buffer = lines.pop();
          for (const line of lines) {
            parseIncomingLine(line.trim());
          }
        }
      } catch (err) {
        logTerminal(`[SERIAL] Read error: ${err.message}`, 'err');
      } finally {
        serialReader.releaseLock();
      }
    }

    function parseIncomingLine(line) {
      if (!line) return;
      logTerminal(line, 'rx');

      // Parse firmware console prints
      if (line.includes('State:')) {
        logTerminal(`[SYNC] Hardware State update parsed.`, 'info');
      } else if (line.includes('Progress:')) {
        const m = line.match(/(\d+)%/);
        if (m) {
          setBootProgress(parseInt(m[1], 10));
        }
      }
    }

    async function sendSerialCmd(cmd) {
      logTerminal(`> ${cmd}`, 'tx');
      if (isConnected && serialPort && serialPort.writable) {
        const encoder = new TextEncoder();
        const writer = serialPort.writable.getWriter();
        await writer.write(encoder.encode(cmd + '\n'));
        writer.releaseLock();
      } else {
        // Handle in Simulator
        handleSimulatorCommand(cmd);
      }
    }

    function sendCustomTermCmd() {
      const input = document.getElementById('termCmdInput');
      const cmd = input.value.trim();
      if (!cmd) return;
      sendSerialCmd(cmd);
      input.value = '';
    }

    function handleSimulatorCommand(cmd) {
      const parts = cmd.split(' ');
      const action = parts[0].toLowerCase();

      if (action === 'info') {
        logTerminal('=== Boot Display Driver Info ===', 'rx');
        logTerminal(`State: ${APP_STATE.bootProgress >= 100 ? 'READY' : (APP_STATE.bootProgress > 0 ? 'ANIMATING' : 'IDLE')}`, 'rx');
        logTerminal(`Mode: ${APP_STATE.bootMode} (0=CONT, 1=STATIC, 2=SINGLE, 3=EVENT)`, 'rx');
        logTerminal(`Progress: ${Math.round(APP_STATE.bootProgress)}%`, 'rx');
        logTerminal(`Cycles: ${simCycles}`, 'rx');
        logTerminal('Display init: 14220 us (LP-Core cold reset)', 'rx');
        logTerminal('Longest frame: 820 us (DMA buffer fill)', 'rx');
      } else if (action === 'state') {
        const st = (APP_STATE.bootProgress >= 100) ? 'READY' : (APP_STATE.bootProgress > 0 ? 'ANIMATING' : 'IDLE');
        logTerminal(`State: ${st}`, 'rx');
      } else if (action === 'handoff') {
        logTerminal('[LP_BOOT] Releasing LP display bus lock...', 'rx');
        logTerminal('[HP_BOOT] SPI2_HOST DMA bus acquired by FreeRTOS task.', 'rx');
        setEngine('hp');
      } else if (action === 'restart') {
        logTerminal('[LP_BOOT] Resetting frame index to 0. Autonomous cycle restarted.', 'rx');
        setEngine('lp');
        setBootProgress(0);
        APP_STATE.isPlaying = true;
        updatePlayButtonUI();
      } else if (action === 'progress') {
        const val = parseInt(parts[1], 10);
        if (!isNaN(val)) {
          setBootProgress(val);
          logTerminal(`[BOOT] Set progress to ${val}%`, 'rx');
        }
      } else if (action === 'mode') {
        const val = parseInt(parts[1], 10);
        if (!isNaN(val) && val >= 0 && val <= 3) {
          onBootModeChange(val);
          document.getElementById('selBootMode').value = val;
          logTerminal(`[BOOT] Mode set to ${val}`, 'rx');
        }
      } else {
        logTerminal(`[CLI] Unknown command: "${cmd}". Type "info" or "state".`, 'err');
      }
    }

    function startSimulator() {
      simTimer = setInterval(() => {
        simFrames++;
      }, 40);
    }

    /* ==========================================================================
     * 10. Drawer & Navigation Utilities
     * ========================================================================== */
    function switchTab(tabId) {
      document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
      document.querySelectorAll('.tab-body').forEach(tab => tab.classList.remove('active'));

      const activeBtn = (tabId === 'tabComposer') ? document.getElementById('btnTabComposer') : document.getElementById('btnTabStates');
      const activeBody = document.getElementById(tabId);

      if (activeBtn) activeBtn.classList.add('active');
      if (activeBody) activeBody.classList.add('active');
    }

    function toggleTerminalDrawer(forceState) {
      const drawer = document.getElementById('bottomTerminalDrawer');
      const isOpen = forceState !== undefined ? forceState : !drawer.classList.contains('open');
      drawer.classList.toggle('open', isOpen);
      document.getElementById('btnToggleDrawer').classList.toggle('active', isOpen);
    }

    function logTerminal(msg, type = 'info') {
      const term = document.getElementById('termLog');
      const row = document.createElement('div');
      row.className = `log-${type}`;
      row.textContent = msg;
      term.appendChild(row);
      term.scrollTop = term.scrollHeight;
    }

    function readTerminalLog() {
      const term = document.getElementById('termLog');
      return Array.from(term.children, row => row.textContent).join('\n');
    }

    async function copyTerminalLog() {
      try {
        const text = readTerminalLog();
        if (navigator.clipboard && window.isSecureContext) {
          await navigator.clipboard.writeText(text);
          return;
        }

        const textarea = document.createElement('textarea');
        textarea.value = text;
        textarea.setAttribute('readonly', '');
        textarea.style.position = 'fixed';
        textarea.style.opacity = '0';
        document.body.appendChild(textarea);
        let copied;
        try {
          textarea.select();
          copied = document.execCommand('copy');
        } finally {
          textarea.remove();
        }
        if (!copied) throw new Error('Clipboard access is unavailable in this browser.');
      } catch (err) {
        logTerminal(`Could not copy serial log: ${err.message}`, 'err');
      }
    }

    function downloadTerminalLog() {
      let objectUrl;
      try {
        const blob = new Blob([readTerminalLog()], { type: 'text/plain;charset=utf-8' });
        objectUrl = URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = objectUrl;
        link.download = 'boot-display-serial-console-log.txt';
        document.body.appendChild(link);
        link.click();
        link.remove();
      } catch (err) {
        logTerminal(`Could not download serial log: ${err.message}`, 'err');
      } finally {
        if (objectUrl) window.setTimeout(() => URL.revokeObjectURL(objectUrl), 1000);
      }
    }

    function clearTerminal() {
      document.getElementById('termLog').innerHTML = '';
    }

    document.getElementById('btnCopyTerm').addEventListener('click', copyTerminalLog);
    document.getElementById('btnDownloadTerm').addEventListener('click', downloadTerminalLog);

    function openHelpModal() {
      document.getElementById('helpModal').classList.add('open');
    }

    function closeHelpModal() {
      document.getElementById('helpModal').classList.remove('open');
    }
