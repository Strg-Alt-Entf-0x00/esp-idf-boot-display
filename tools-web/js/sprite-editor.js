"use strict";

function initPixelEditor() {
      PIXEL_EDITOR.canvas = document.getElementById('pixelGridCanvas');
      PIXEL_EDITOR.ctx = PIXEL_EDITOR.canvas.getContext('2d');
      PIXEL_EDITOR.previewCanvas = document.getElementById('previewMiniCanvas');
      PIXEL_EDITOR.previewCtx = PIXEL_EDITOR.previewCanvas.getContext('2d');

      // Populate Palette Swatches
      const swatchesBox = document.getElementById('pixelSwatches');
      swatchesBox.innerHTML = '';
      PIXEL_EDITOR.palette.forEach(color => {
        const sw = document.createElement('div');
        sw.className = `pixel-swatch ${color === PIXEL_EDITOR.currentColor ? 'active' : ''}`;
        sw.style.backgroundColor = color;
        sw.onclick = () => setPixelColor(color);
        swatchesBox.appendChild(sw);
      });

      // Clear grid to transparent
      clearPixelGrid();

      // Mouse & Touch events for drawing
      function getGridCell(e) {
        const rect = PIXEL_EDITOR.canvas.getBoundingClientRect();
        const clientX = e.touches ? e.touches[0].clientX : e.clientX;
        const clientY = e.touches ? e.touches[0].clientY : e.clientY;
        const x = Math.floor(((clientX - rect.left) / rect.width) * PIXEL_EDITOR.gridW);
        const y = Math.floor(((clientY - rect.top) / rect.height) * PIXEL_EDITOR.gridH);
        return {
          x: Math.max(0, Math.min(PIXEL_EDITOR.gridW - 1, x)),
          y: Math.max(0, Math.min(PIXEL_EDITOR.gridH - 1, y))
        };
      }

      function handleDrawEvent(e) {
        const cell = getGridCell(e);
        document.getElementById('lblGridCoords').textContent = `X: ${cell.x}, Y: ${cell.y}`;
        onPixelCellAction(cell.x, cell.y);
      }

      PIXEL_EDITOR.canvas.addEventListener('mousedown', (e) => {
        if (e.button !== 0) return;
        PIXEL_EDITOR.isDrawing = true;
        handleDrawEvent(e);
      });

      window.addEventListener('mousemove', (e) => {
        if (PIXEL_EDITOR.isDrawing) {
          handleDrawEvent(e);
        } else if (e.target === PIXEL_EDITOR.canvas) {
          const cell = getGridCell(e);
          document.getElementById('lblGridCoords').textContent = `X: ${cell.x}, Y: ${cell.y}`;
        }
      });

      window.addEventListener('mouseup', () => {
        PIXEL_EDITOR.isDrawing = false;
      });

      // Touch support
      PIXEL_EDITOR.canvas.addEventListener('touchstart', (e) => {
        e.preventDefault();
        PIXEL_EDITOR.isDrawing = true;
        handleDrawEvent(e);
      }, { passive: false });

      PIXEL_EDITOR.canvas.addEventListener('touchmove', (e) => {
        e.preventDefault();
        if (PIXEL_EDITOR.isDrawing) handleDrawEvent(e);
      }, { passive: false });

      PIXEL_EDITOR.canvas.addEventListener('touchend', () => {
        PIXEL_EDITOR.isDrawing = false;
      });
    }

    function openSpriteStudioModal(layerId = null) {
      PIXEL_EDITOR.targetLayerId = layerId;
      const modal = document.getElementById('spriteStudioModal');
      const saveBtn = document.getElementById('btnSaveSpriteLayer');

      if (layerId) {
        const layer = APP_STATE.layers.find(l => l.id === layerId);
        if (layer && layer.type === 'sprite') {
          PIXEL_EDITOR.gridW = layer.w || 24;
          PIXEL_EDITOR.gridH = layer.h || 24;
          document.getElementById('selGridRes').value = PIXEL_EDITOR.gridW;
          PIXEL_EDITOR.pixels = [...(layer.pixels || [])];
          saveBtn.textContent = 'Save Changes to Layer';
        }
      } else {
        saveBtn.textContent = '+ Insert as Sprite Layer';
      }

      renderPixelGrid();
      renderPreviewMini();
      modal.classList.add('open');
    }

    function closeSpriteStudioModal() {
      document.getElementById('spriteStudioModal').classList.remove('open');
      PIXEL_EDITOR.targetLayerId = null;
    }

    function setPixelTool(tool) {
      PIXEL_EDITOR.currentTool = tool;
      document.querySelectorAll('.pixel-tool-btn').forEach(btn => btn.classList.remove('active'));
      const activeBtn = {
        'pen': 'btnToolPen',
        'eraser': 'btnToolEraser',
        'bucket': 'btnToolBucket',
        'picker': 'btnToolPipette'
      }[tool];
      if (activeBtn) document.getElementById(activeBtn).classList.add('active');
    }

    function setPixelColor(color) {
      PIXEL_EDITOR.currentColor = color;
      document.getElementById('pixelColorInput').value = color;
      document.getElementById('lblCurrentColor').textContent = color;

      document.querySelectorAll('.pixel-swatch').forEach(sw => {
        sw.classList.toggle('active', sw.style.backgroundColor === color || sw.style.backgroundColor === hexToRgbString(color));
      });

      if (PIXEL_EDITOR.currentTool === 'eraser' || PIXEL_EDITOR.currentTool === 'picker') {
        setPixelTool('pen');
      }
    }

    function hexToRgbString(hex) {
      const { r, g, b } = hexToRgb(hex);
      return `rgb(${r}, ${g}, ${b})`;
    }

    function onPixelColorPickerChange(color) {
      setPixelColor(color);
    }

    function onPixelCellAction(x, y) {
      const idx = y * PIXEL_EDITOR.gridW + x;
      if (PIXEL_EDITOR.currentTool === 'pen') {
        PIXEL_EDITOR.pixels[idx] = PIXEL_EDITOR.currentColor;
      } else if (PIXEL_EDITOR.currentTool === 'eraser') {
        PIXEL_EDITOR.pixels[idx] = null;
      } else if (PIXEL_EDITOR.currentTool === 'bucket') {
        floodFill(x, y, PIXEL_EDITOR.currentColor);
      } else if (PIXEL_EDITOR.currentTool === 'picker') {
        const picked = PIXEL_EDITOR.pixels[idx];
        if (picked) setPixelColor(picked);
        setPixelTool('pen');
      }
      renderPixelGrid();
      renderPreviewMini();
    }

    function floodFill(startX, startY, fillColor) {
      const w = PIXEL_EDITOR.gridW;
      const h = PIXEL_EDITOR.gridH;
      const targetColor = PIXEL_EDITOR.pixels[startY * w + startX] || null;
      if (targetColor === fillColor) return;

      const queue = [[startX, startY]];
      const visited = new Uint8Array(w * h);

      while (queue.length > 0) {
        const [x, y] = queue.pop();
        const idx = y * w + x;
        if (visited[idx]) continue;
        visited[idx] = 1;

        if ((PIXEL_EDITOR.pixels[idx] || null) === targetColor) {
          PIXEL_EDITOR.pixels[idx] = fillColor;
          if (x > 0) queue.push([x - 1, y]);
          if (x < w - 1) queue.push([x + 1, y]);
          if (y > 0) queue.push([x, y - 1]);
          if (y < h - 1) queue.push([x, y + 1]);
        }
      }
    }

    function clearPixelGrid() {
      const total = PIXEL_EDITOR.gridW * PIXEL_EDITOR.gridH;
      PIXEL_EDITOR.pixels = new Array(total).fill(null);
      renderPixelGrid();
      renderPreviewMini();
    }

    function invertPixelGrid() {
      for (let i = 0; i < PIXEL_EDITOR.pixels.length; i++) {
        const col = PIXEL_EDITOR.pixels[i];
        if (col) {
          const { r, g, b } = hexToRgb(col);
          const invR = (255 - r).toString(16).padStart(2, '0');
          const invG = (255 - g).toString(16).padStart(2, '0');
          const invB = (255 - b).toString(16).padStart(2, '0');
          PIXEL_EDITOR.pixels[i] = `#${invR}${invG}${invB}`;
        }
      }
      renderPixelGrid();
      renderPreviewMini();
    }

    function shiftPixelGrid(dx, dy) {
      const w = PIXEL_EDITOR.gridW;
      const h = PIXEL_EDITOR.gridH;
      const copy = [...PIXEL_EDITOR.pixels];
      PIXEL_EDITOR.pixels.fill(null);

      for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
          const nx = x + dx;
          const ny = y + dy;
          if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
            PIXEL_EDITOR.pixels[ny * w + nx] = copy[y * w + x];
          }
        }
      }
      renderPixelGrid();
      renderPreviewMini();
    }

    function changeGridResolution(res) {
      const oldW = PIXEL_EDITOR.gridW;
      const oldH = PIXEL_EDITOR.gridH;
      const oldPixels = [...PIXEL_EDITOR.pixels];

      PIXEL_EDITOR.gridW = res;
      PIXEL_EDITOR.gridH = res;
      PIXEL_EDITOR.pixels = new Array(res * res).fill(null);

      // Center old pixels in new grid
      const offsetX = Math.floor((res - oldW) / 2);
      const offsetY = Math.floor((res - oldH) / 2);

      for (let y = 0; y < oldH; y++) {
        for (let x = 0; x < oldW; x++) {
          const nx = x + offsetX;
          const ny = y + offsetY;
          if (nx >= 0 && nx < res && ny >= 0 && ny < res) {
            PIXEL_EDITOR.pixels[ny * res + nx] = oldPixels[y * oldW + x];
          }
        }
      }

      renderPixelGrid();
      renderPreviewMini();
    }

    function renderPixelGrid() {
      if (!PIXEL_EDITOR.ctx) return;
      const c = PIXEL_EDITOR.ctx;
      const cv = PIXEL_EDITOR.canvas;
      const w = cv.width;
      const h = cv.height;
      const gw = PIXEL_EDITOR.gridW;
      const gh = PIXEL_EDITOR.gridH;
      const cellW = w / gw;
      const cellH = h / gh;

      c.clearRect(0, 0, w, h);

      for (let y = 0; y < gh; y++) {
        for (let x = 0; x < gw; x++) {
          const color = PIXEL_EDITOR.pixels[y * gw + x];
          if (color) {
            c.fillStyle = color;
            c.fillRect(x * cellW, y * cellH, cellW, cellH);
          }
          // Grid lines
          c.strokeStyle = 'rgba(255, 255, 255, 0.08)';
          c.lineWidth = 1;
          c.strokeRect(x * cellW, y * cellH, cellW, cellH);
        }
      }
    }

    function renderPreviewMini() {
      if (!PIXEL_EDITOR.previewCtx) return;
      const c = PIXEL_EDITOR.previewCtx;
      const cv = PIXEL_EDITOR.previewCanvas;
      const gw = PIXEL_EDITOR.gridW;
      const gh = PIXEL_EDITOR.gridH;

      c.clearRect(0, 0, cv.width, cv.height);
      const scale = Math.floor(cv.width / Math.max(gw, gh)) || 1;
      const startX = Math.floor((cv.width - gw * scale) / 2);
      const startY = Math.floor((cv.height - gh * scale) / 2);

      for (let y = 0; y < gh; y++) {
        for (let x = 0; x < gw; x++) {
          const color = PIXEL_EDITOR.pixels[y * gw + x];
          if (color) {
            c.fillStyle = color;
            c.fillRect(startX + x * scale, startY + y * scale, scale, scale);
          }
        }
      }
    }

    function loadSpritePreset(key) {
      if (!key) return;

      if (key === 'tomato') {
        // 24x24 Pixel Art of a round juicy tomato with green stem and highlights
        changeGridResolution(24);
        clearPixelGrid();
        const R = '#ef4444'; // Red body
        const D = '#b91c1c'; // Dark red shading
        const H = '#fca5a5'; // Highlight
        const G = '#22c55e'; // Green leaf
        const DG = '#15803d'; // Dark green stem

        const art = [
          [11,2,DG],[12,2,DG],[10,3,G],[11,3,DG],[12,3,DG],[13,3,G],
          [9,4,G],[10,4,G],[11,4,DG],[12,4,DG],[13,4,G],[14,4,G],
          [8,5,G],[9,5,G],[10,5,G],[11,5,DG],[12,5,DG],[13,5,G],[14,5,G],[15,5,G],
          [9,6,R],[10,6,R],[11,6,R],[12,6,R],[13,6,R],[14,6,R],
          [7,7,R],[8,7,R],[9,7,H],[10,7,R],[11,7,R],[12,7,R],[13,7,R],[14,7,R],[15,7,R],[16,7,R],
          [6,8,R],[7,8,R],[8,8,H],[9,8,H],[10,8,R],[11,8,R],[12,8,R],[13,8,R],[14,8,R],[15,8,R],[16,8,R],[17,8,R],
          [5,9,R],[6,9,R],[7,9,H],[8,9,H],[9,9,R],[10,9,R],[11,9,R],[12,9,R],[13,9,R],[14,9,R],[15,9,R],[16,9,R],[17,9,R],[18,9,R],
          [5,10,R],[6,10,R],[7,10,R],[8,10,R],[9,10,R],[10,10,R],[11,10,R],[12,10,R],[13,10,R],[14,10,R],[15,10,R],[16,10,R],[17,10,R],[18,10,R],
          [4,11,R],[5,11,R],[6,11,R],[7,11,R],[8,11,R],[9,11,R],[10,11,R],[11,11,R],[12,11,R],[13,11,R],[14,11,R],[15,11,R],[16,11,R],[17,11,R],[18,11,R],[19,11,R],
          [4,12,R],[5,12,R],[6,12,R],[7,12,R],[8,12,R],[9,12,R],[10,12,R],[11,12,R],[12,12,R],[13,12,R],[14,12,R],[15,12,R],[16,12,R],[17,12,R],[18,12,R],[19,12,R],
          [4,13,R],[5,13,R],[6,13,R],[7,13,R],[8,13,R],[9,13,R],[10,13,R],[11,13,R],[12,13,R],[13,13,R],[14,13,R],[15,13,R],[16,13,R],[17,13,R],[18,13,R],[19,13,R],
          [4,14,R],[5,14,R],[6,14,R],[7,14,R],[8,14,R],[9,14,R],[10,14,R],[11,14,R],[12,14,R],[13,14,R],[14,14,R],[15,14,R],[16,14,R],[17,14,R],[18,14,R],[19,14,R],
          [5,15,R],[6,15,R],[7,15,R],[8,15,R],[9,15,R],[10,15,R],[11,15,R],[12,15,R],[13,15,R],[14,15,R],[15,15,R],[16,15,R],[17,15,D],[18,15,D],
          [5,16,R],[6,16,R],[7,16,R],[8,16,R],[9,16,R],[10,16,R],[11,16,R],[12,16,R],[13,16,R],[14,16,D],[15,16,D],[16,16,D],[17,16,D],[18,16,D],
          [6,17,R],[7,17,R],[8,17,R],[9,17,R],[10,17,R],[11,17,R],[12,17,D],[13,17,D],[14,17,D],[15,17,D],[16,17,D],[17,17,D],
          [7,18,R],[8,18,R],[9,18,R],[10,18,D],[11,18,D],[12,18,D],[13,18,D],[14,18,D],[15,18,D],[16,18,D],
          [8,19,D],[9,19,D],[10,19,D],[11,19,D],[12,19,D],[13,19,D],[14,19,D],[15,19,D]
        ];
        art.forEach(([x, y, col]) => {
          PIXEL_EDITOR.pixels[y * 24 + x] = col;
        });
      } else if (key === 'mascot') {
        // 16x16 Retro Mascot
        changeGridResolution(16);
        clearPixelGrid();
        const W = '#ffffff';
        const B = '#00d4ff';
        const D = '#64748b';
        const mascotPx = [
          [6,1,W],[7,1,W],[8,1,W],[9,1,W],
          [5,2,W],[6,2,W],[7,2,W],[8,2,W],[9,2,W],[10,2,W],
          [4,3,W],[5,3,W],[6,3,W],[7,3,W],[8,3,W],[9,3,W],[10,3,W],[11,3,W],
          [4,4,W],[5,4,B],[6,4,B],[7,4,W],[8,4,W],[9,4,B],[10,4,B],[11,4,W],
          [4,5,W],[5,5,B],[6,5,B],[7,5,W],[8,5,W],[9,5,B],[10,5,B],[11,5,W],
          [3,6,W],[4,6,W],[5,6,W],[6,6,W],[7,6,W],[8,6,W],[9,6,W],[10,6,W],[11,6,W],[12,6,W],
          [3,7,W],[4,7,W],[5,7,W],[6,7,W],[7,7,W],[8,7,W],[9,7,W],[10,7,W],[11,7,W],[12,7,W],
          [3,8,W],[4,8,W],[5,8,W],[6,8,W],[7,8,W],[8,8,W],[9,8,W],[10,8,W],[11,8,W],[12,8,W],
          [3,9,W],[4,9,W],[5,9,W],[6,9,W],[7,9,W],[8,9,W],[9,9,W],[10,9,W],[11,9,W],[12,9,W],
          [3,10,W],[4,10,W],[5,10,W],[6,10,W],[7,10,W],[8,10,W],[9,10,W],[10,10,W],[11,10,W],[12,10,W],
          [3,11,W],[4,11,W],[6,11,W],[7,11,W],[8,11,W],[9,11,W],[11,11,W],[12,11,W],
          [3,12,W],[4,12,W],[7,12,W],[8,12,W],[11,12,W],[12,12,W],
          [3,13,W],[7,13,W],[8,13,W],[12,13,W]
        ];
        mascotPx.forEach(([x, y, col]) => {
          PIXEL_EDITOR.pixels[y * 16 + x] = col;
        });
      } else if (key === 'chip') {
        // 24x24 ESP32 Chip
        changeGridResolution(24);
        clearPixelGrid();
        const B = '#1e293b';
        const G = '#eab308';
        const T = '#38bdf8';
        for (let y = 5; y <= 18; y++) {
          for (let x = 5; x <= 18; x++) {
            PIXEL_EDITOR.pixels[y * 24 + x] = B;
          }
        }
        for (let p = 6; p <= 17; p += 2) {
          PIXEL_EDITOR.pixels[3 * 24 + p] = G;
          PIXEL_EDITOR.pixels[4 * 24 + p] = G;
          PIXEL_EDITOR.pixels[19 * 24 + p] = G;
          PIXEL_EDITOR.pixels[20 * 24 + p] = G;
          PIXEL_EDITOR.pixels[p * 24 + 3] = G;
          PIXEL_EDITOR.pixels[p * 24 + 4] = G;
          PIXEL_EDITOR.pixels[p * 24 + 19] = G;
          PIXEL_EDITOR.pixels[p * 24 + 20] = G;
        }
        PIXEL_EDITOR.pixels[11 * 24 + 11] = T;
        PIXEL_EDITOR.pixels[11 * 24 + 12] = T;
        PIXEL_EDITOR.pixels[12 * 24 + 11] = T;
        PIXEL_EDITOR.pixels[12 * 24 + 12] = T;
      } else if (key === 'heart') {
        // 16x16 Heart
        changeGridResolution(16);
        clearPixelGrid();
        const R = '#ef4444';
        const H = '#fca5a5';
        const heartPx = [
          [4,3,R],[5,3,R],[10,3,R],[11,3,R],
          [3,4,R],[4,4,H],[5,4,R],[6,4,R],[9,4,R],[10,4,R],[11,4,R],[12,4,R],
          [2,5,R],[3,5,H],[4,5,H],[5,5,R],[6,5,R],[7,5,R],[8,5,R],[9,5,R],[10,5,R],[11,5,R],[12,5,R],[13,5,R],
          [2,6,R],[3,6,R],[4,6,R],[5,6,R],[6,6,R],[7,6,R],[8,6,R],[9,6,R],[10,6,R],[11,6,R],[12,6,R],[13,6,R],
          [2,7,R],[3,7,R],[4,7,R],[5,7,R],[6,7,R],[7,7,R],[8,7,R],[9,7,R],[10,7,R],[11,7,R],[12,7,R],[13,7,R],
          [3,8,R],[4,8,R],[5,8,R],[6,8,R],[7,8,R],[8,8,R],[9,8,R],[10,8,R],[11,8,R],[12,8,R],
          [4,9,R],[5,9,R],[6,9,R],[7,9,R],[8,9,R],[9,9,R],[10,9,R],[11,9,R],
          [5,10,R],[6,10,R],[7,10,R],[8,10,R],[9,10,R],[10,10,R],
          [6,11,R],[7,11,R],[8,11,R],[9,11,R],
          [7,12,R],[8,12,R]
        ];
        heartPx.forEach(([x, y, col]) => {
          PIXEL_EDITOR.pixels[y * 16 + x] = col;
        });
      } else if (key === 'star') {
        // 16x16 Star
        changeGridResolution(16);
        clearPixelGrid();
        const Y = '#eab308';
        const starPx = [
          [7,2,Y],[8,2,Y],
          [7,3,Y],[8,3,Y],
          [6,4,Y],[7,4,Y],[8,4,Y],[9,4,Y],
          [6,5,Y],[7,5,Y],[8,5,Y],[9,5,Y],
          [2,6,Y],[3,6,Y],[4,6,Y],[5,6,Y],[6,6,Y],[7,6,Y],[8,6,Y],[9,6,Y],[10,6,Y],[11,6,Y],[12,6,Y],[13,6,Y],
          [3,7,Y],[4,7,Y],[5,7,Y],[6,7,Y],[7,7,Y],[8,7,Y],[9,7,Y],[10,7,Y],[11,7,Y],[12,7,Y],
          [4,8,Y],[5,8,Y],[6,8,Y],[7,8,Y],[8,8,Y],[9,8,Y],[10,8,Y],[11,8,Y],
          [5,9,Y],[6,9,Y],[7,9,Y],[8,9,Y],[9,9,Y],[10,9,Y],
          [5,10,Y],[6,10,Y],[7,10,Y],[8,10,Y],[9,10,Y],[10,10,Y],
          [4,11,Y],[5,11,Y],[6,11,Y],[9,11,Y],[10,11,Y],[11,11,Y],
          [4,12,Y],[5,12,Y],[10,12,Y],[11,12,Y],
          [3,13,Y],[4,13,Y],[11,13,Y],[12,13,Y]
        ];
        starPx.forEach(([x, y, col]) => {
          PIXEL_EDITOR.pixels[y * 16 + x] = col;
        });
      }

      document.getElementById('selSpritePreset').value = '';
      renderPixelGrid();
      renderPreviewMini();
    }

    function handleSpriteImageFile(evt) {
      const file = evt.target.files[0];
      if (!file) return;

      const reader = new FileReader();
      reader.onload = (e) => {
        const img = new Image();
        img.onload = () => {
          const offCv = document.createElement('canvas');
          offCv.width = PIXEL_EDITOR.gridW;
          offCv.height = PIXEL_EDITOR.gridH;
          const offCtx = offCv.getContext('2d');
          offCtx.drawImage(img, 0, 0, PIXEL_EDITOR.gridW, PIXEL_EDITOR.gridH);
          const imgData = offCtx.getImageData(0, 0, PIXEL_EDITOR.gridW, PIXEL_EDITOR.gridH).data;

          const thresh = parseInt(document.getElementById('rngImportThreshold').value, 10) || 128;
          for (let i = 0; i < imgData.length; i += 4) {
            const a = imgData[i + 3];
            const idx = Math.floor(i / 4);
            if (a < 64) {
              PIXEL_EDITOR.pixels[idx] = null;
            } else {
              const r = imgData[i];
              const g = imgData[i + 1];
              const b = imgData[i + 2];
              const avg = 0.299 * r + 0.587 * g + 0.114 * b;
              if (avg < thresh / 2) {
                PIXEL_EDITOR.pixels[idx] = '#000000';
              } else {
                PIXEL_EDITOR.pixels[idx] = rgbToHex(r, g, b);
              }
            }
          }
          renderPixelGrid();
          renderPreviewMini();
          logTerminal(`[SPRITE] Imported image downsampled to ${PIXEL_EDITOR.gridW}x${PIXEL_EDITOR.gridH}.`, 'info');
        };
        img.src = e.target.result;
      };
      reader.readAsDataURL(file);
      evt.target.value = '';
    }

    function rgbToHex(r, g, b) {
      return '#' + [r, g, b].map(x => x.toString(16).padStart(2, '0')).join('');
    }

    function updateImportThreshold(val) {
      document.getElementById('valImportThreshold').textContent = val;
    }

    function saveSpriteToLayer() {
      if (PIXEL_EDITOR.targetLayerId) {
        const layer = APP_STATE.layers.find(l => l.id === PIXEL_EDITOR.targetLayerId);
        if (layer) {
          layer.w = PIXEL_EDITOR.gridW;
          layer.h = PIXEL_EDITOR.gridH;
          layer.pixels = [...PIXEL_EDITOR.pixels];
          renderLayerList();
          renderLayerInspector();
          updateCodeOutput();
          closeSpriteStudioModal();
          logTerminal(`[SPRITE] Updated layer "${layer.name}" with edited pixels.`, 'info');
          return;
        }
      }

      // Create new sprite layer
      const id = 'layer_' + Date.now();
      const newLayer = {
        id,
        type: 'sprite',
        name: `Sprite ${PIXEL_EDITOR.gridW}x${PIXEL_EDITOR.gridH}`,
        visible: true,
        w: PIXEL_EDITOR.gridW,
        h: PIXEL_EDITOR.gridH,
        pixels: [...PIXEL_EDITOR.pixels],
        scale: (PIXEL_EDITOR.gridW <= 16 ? 3 : 2),
        offsetX: 0,
        offsetY: 0,
        rotation: 0,
        modifier: 'none'
      };

      APP_STATE.layers.push(newLayer);
      APP_STATE.selectedLayerId = id;
      closeSpriteStudioModal();
      switchTab('tabComposer');
      renderLayerList();
      renderLayerInspector();
      updateCodeOutput();
      logTerminal(`[COMPOSER] Inserted new sprite layer ${newLayer.name} into composition.`, 'info');
    }

    /* ==========================================================================
     * 3. Interactive Direct Canvas Manipulation (Click, Drag, Snap & Wheel)
     * ========================================================================== */
