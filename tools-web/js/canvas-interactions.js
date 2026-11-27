"use strict";

function getCanvasCoords(e) {
      const rect = canvas.getBoundingClientRect();
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      const scaleX = canvas.width / rect.width;
      const scaleY = canvas.height / rect.height;
      return {
        x: (clientX - rect.left) * scaleX,
        y: (clientY - rect.top) * scaleY
      };
    }

    function getLayerBounds(layer) {
      if (!layer) return null;

      if (layer.type === 'sprite' || layer.type === 'mascot' || layer.type === 'bitmap') {
        const scale = layer.scale || 2;
        const w = (layer.w || (layer.type === 'mascot' ? 12 : 24)) * scale;
        const h = (layer.h || (layer.type === 'mascot' ? 14 : 24)) * scale;
        const cx = 120 + (layer.offsetX || 0);
        const cy = 120 + (layer.offsetY || 0);
        return { type: 'box', cx, cy, x: cx - w / 2, y: cy - h / 2, w, h };
      } else if (layer.type === 'text') {
        if (layer.pathMode === 'straight') {
          const fontSize = layer.fontSize || 12;
          const textLen = (layer.text || '').length || 1;
          const w = textLen * (fontSize * 0.65) + 16;
          const h = fontSize + 10;
          const cx = 120 + (layer.offsetX || 0);
          const cy = 120 + (layer.offsetY || 0);
          return { type: 'box', cx, cy, x: cx - w / 2, y: cy - h / 2, w, h };
        } else {
          return { type: 'text_arc', cx: 120, cy: 120, radius: layer.radius || 88, startAngle: layer.startAngle !== undefined ? layer.startAngle : 90, letterSpacing: layer.letterSpacing || 8 };
        }
      } else if (layer.type === 'gauge' || layer.type === 'ring') {
        return { type: 'ring', cx: 120, cy: 120, radius: layer.radius || 112, thickness: layer.thickness || 8 };
      }
      return null;
    }

    function hitTestCanvas(pt) {
      const selLayer = APP_STATE.layers.find(l => l.id === APP_STATE.selectedLayerId);

      // 1. Check selection handles first
      if (selLayer && selLayer.visible) {
        const b = getLayerBounds(selLayer);
        if (b && b.type === 'box') {
          const handles = [
            { key: 'tl', x: b.x - 2, y: b.y - 2, cursor: 'nwse-resize' },
            { key: 'tr', x: b.x + b.w + 2, y: b.y - 2, cursor: 'nesw-resize' },
            { key: 'bl', x: b.x - 2, y: b.y + b.h + 2, cursor: 'nesw-resize' },
            { key: 'br', x: b.x + b.w + 2, y: b.y + b.h + 2, cursor: 'nwse-resize' }
          ];
          for (const h of handles) {
            if (Math.hypot(pt.x - h.x, pt.y - h.y) <= 8) {
              return { layer: selLayer, handle: h.key, cursor: h.cursor };
            }
          }
        } else if (selLayer.type === 'gauge' || selLayer.type === 'ring') {
          const hx = 120 + selLayer.radius;
          const hy = 120;
          if (Math.hypot(pt.x - hx, pt.y - hy) <= 8) {
            return { layer: selLayer, handle: 'radius', cursor: 'ew-resize' };
          }
        } else if (selLayer.type === 'text' && selLayer.pathMode !== 'straight') {
          const rad = selLayer.radius || 88;
          const midRad = (((selLayer.startAngle !== undefined ? selLayer.startAngle : 90)) * Math.PI) / 180;
          const hx = 120 + rad * Math.cos(midRad);
          const hy = 120 + rad * Math.sin(midRad);
          if (Math.hypot(pt.x - hx, pt.y - hy) <= 8) {
            return { layer: selLayer, handle: 'radius', cursor: 'ew-resize' };
          }
        }
      }

      // 2. Hit-test layers in reverse order (topmost first)
      for (let i = APP_STATE.layers.length - 1; i >= 0; i--) {
        const layer = APP_STATE.layers[i];
        if (!layer.visible) continue;
        const b = getLayerBounds(layer);
        if (!b) continue;

        if (b.type === 'box') {
          if (pt.x >= b.x - 4 && pt.x <= b.x + b.w + 4 && pt.y >= b.y - 4 && pt.y <= b.y + b.h + 4) {
            return { layer, handle: null, cursor: 'grab' };
          }
        } else if (b.type === 'text_arc') {
          const dist = Math.hypot(pt.x - 120, pt.y - 120);
          if (Math.abs(dist - b.radius) <= 16) {
            return { layer, handle: null, cursor: 'grab' };
          }
        } else if (b.type === 'ring') {
          const dist = Math.hypot(pt.x - 120, pt.y - 120);
          if (Math.abs(dist - b.radius) <= Math.max(12, b.thickness + 4)) {
            return { layer, handle: null, cursor: 'grab' };
          }
        }
      }

      return null;
    }

    function renderSelectionOverlay(c, cx, cy) {
      const selLayer = APP_STATE.layers.find(l => l.id === APP_STATE.selectedLayerId);
      if (!selLayer || !selLayer.visible) return;

      const b = getLayerBounds(selLayer);
      if (!b) return;

      c.save();
      if (b.type === 'box') {
        c.strokeStyle = 'rgba(56, 189, 248, 0.85)';
        c.lineWidth = 1;
        c.setLineDash([4, 4]);
        c.strokeRect(b.x - 2, b.y - 2, b.w + 4, b.h + 4);
        c.setLineDash([]);

        // 4 Corner Handles
        const corners = [
          [b.x - 2, b.y - 2],
          [b.x + b.w + 2, b.y - 2],
          [b.x - 2, b.y + b.h + 2],
          [b.x + b.w + 2, b.y + b.h + 2]
        ];
        c.fillStyle = '#38bdf8';
        c.strokeStyle = '#ffffff';
        c.lineWidth = 1.5;
        for (const [hx, hy] of corners) {
          c.fillRect(hx - 3, hy - 3, 6, 6);
          c.strokeRect(hx - 3, hy - 3, 6, 6);
        }
      } else if (b.type === 'ring') {
        c.strokeStyle = 'rgba(56, 189, 248, 0.55)';
        c.lineWidth = 1;
        c.setLineDash([4, 4]);
        c.beginPath();
        c.arc(cx, cy, selLayer.radius, 0, Math.PI * 2);
        c.stroke();
        c.setLineDash([]);

        const hx = cx + selLayer.radius;
        const hy = cy;
        c.fillStyle = '#38bdf8';
        c.strokeStyle = '#ffffff';
        c.lineWidth = 1.5;
        c.beginPath();
        c.arc(hx, hy, 4, 0, Math.PI * 2);
        c.fill();
        c.stroke();
      } else if (b.type === 'text_arc') {
        const rad = selLayer.radius || 88;
        const startDeg = selLayer.startAngle !== undefined ? selLayer.startAngle : 90;
        const spacing = selLayer.letterSpacing || 8;
        const totalAngle = ((selLayer.text || '').length - 1) * spacing;
        const startRad = ((startDeg - totalAngle / 2) * Math.PI) / 180;
        const endRad = ((startDeg + totalAngle / 2) * Math.PI) / 180;

        c.strokeStyle = 'rgba(56, 189, 248, 0.55)';
        c.lineWidth = 1;
        c.setLineDash([4, 4]);
        c.beginPath();
        c.arc(cx, cy, rad, startRad - 0.1, endRad + 0.1);
        c.stroke();
        c.setLineDash([]);

        const midRad = (startDeg * Math.PI) / 180;
        const hx = cx + rad * Math.cos(midRad);
        const hy = cy + rad * Math.sin(midRad);
        c.fillStyle = '#38bdf8';
        c.strokeStyle = '#ffffff';
        c.lineWidth = 1.5;
        c.beginPath();
        c.arc(hx, hy, 4, 0, Math.PI * 2);
        c.fill();
        c.stroke();
      }
      c.restore();
    }

    function setupCanvasInteractions() {
      function onPointerDown(e) {
        if (e.button === 2) return;
        const pt = getCanvasCoords(e);
        const hit = hitTestCanvas(pt);

        if (hit) {
          isCanvasDragging = true;
          canvasDragTarget = hit.layer;
          canvasDragStartMouse = { x: pt.x, y: pt.y };

          if (hit.layer.id !== APP_STATE.selectedLayerId) {
            selectLayer(hit.layer.id);
          }

          if (hit.handle && (hit.handle === 'tl' || hit.handle === 'tr' || hit.handle === 'bl' || hit.handle === 'br')) {
            canvasDragMode = 'resize';
            const b = getLayerBounds(hit.layer);
            canvasDragStartVals = {
              scale: hit.layer.scale || 2,
              initialDist: Math.hypot(pt.x - b.cx, pt.y - b.cy) || 1
            };
          } else if (hit.handle === 'radius') {
            canvasDragMode = 'radius';
            canvasDragStartVals = { radius: hit.layer.radius || 88 };
          } else {
            canvasDragMode = 'move';
            canvasDragStartVals = {
              offsetX: hit.layer.offsetX || 0,
              offsetY: hit.layer.offsetY || 0,
              radius: hit.layer.radius || 88
            };
            canvas.style.cursor = 'grabbing';
          }
        }
      }

      function onPointerMove(e) {
        const pt = getCanvasCoords(e);

        if (isCanvasDragging && canvasDragTarget) {
          const b = getLayerBounds(canvasDragTarget);

          if (canvasDragMode === 'move') {
            if (b && b.type === 'box') {
              let newX = canvasDragStartVals.offsetX + (pt.x - canvasDragStartMouse.x);
              let newY = canvasDragStartVals.offsetY + (pt.y - canvasDragStartMouse.y);

              // Circular dial boundary clamping: maximum distance inside circular GC9A01 bezel
              const maxDim = Math.max(b.w, b.h) / 2;
              const maxDist = Math.max(10, 114 - maxDim);
              const dist = Math.hypot(newX, newY);
              if (dist > maxDist) {
                const angle = Math.atan2(newY, newX);
                newX = Math.round(Math.cos(angle) * maxDist);
                newY = Math.round(Math.sin(angle) * maxDist);
              }

              // Center snapping (+- 4px)
              if (Math.abs(newX) <= 4) {
                newX = 0;
                snapCrosshair.x = 120;
              } else {
                snapCrosshair.x = null;
              }
              if (Math.abs(newY) <= 4) {
                newY = 0;
                snapCrosshair.y = 120;
              } else {
                snapCrosshair.y = null;
              }

              canvasDragTarget.offsetX = newX;
              canvasDragTarget.offsetY = newY;
              syncInspectorInputs(canvasDragTarget);
            } else if (canvasDragTarget.type === 'gauge' || canvasDragTarget.type === 'ring' || canvasDragTarget.type === 'text') {
              const curR = Math.round(Math.hypot(pt.x - 120, pt.y - 120));
              canvasDragTarget.radius = Math.max(20, Math.min(114, curR));
              syncInspectorInputs(canvasDragTarget);
            }
          } else if (canvasDragMode === 'resize') {
            const curDist = Math.hypot(pt.x - b.cx, pt.y - b.cy);
            const ratio = curDist / (canvasDragStartVals.initialDist || 1);
            let newScale = Math.round(canvasDragStartVals.scale * ratio);
            newScale = Math.max(1, Math.min(8, newScale));

            canvasDragTarget.scale = newScale;
            syncInspectorInputs(canvasDragTarget);
          } else if (canvasDragMode === 'radius') {
            const curR = Math.round(Math.hypot(pt.x - 120, pt.y - 120));
            canvasDragTarget.radius = Math.max(20, Math.min(114, curR));
            syncInspectorInputs(canvasDragTarget);
          }
        } else {
          // Hover cursor inspection
          const hit = hitTestCanvas(pt);
          canvas.style.cursor = hit ? (hit.cursor || 'grab') : 'default';
        }
      }

      function onPointerUp() {
        if (isCanvasDragging) {
          isCanvasDragging = false;
          canvasDragMode = null;
          canvasDragTarget = null;
          snapCrosshair.x = null;
          snapCrosshair.y = null;
          canvas.style.cursor = 'default';
          updateCodeOutput();
        }
      }

      canvas.addEventListener('mousedown', onPointerDown);
      window.addEventListener('mousemove', onPointerMove);
      window.addEventListener('mouseup', onPointerUp);

      canvas.addEventListener('touchstart', onPointerDown, { passive: false });
      window.addEventListener('touchmove', (e) => {
        if (isCanvasDragging) e.preventDefault();
        onPointerMove(e);
      }, { passive: false });
      window.addEventListener('touchend', onPointerUp);

      // Mouse wheel scaling & shift-wheel rotation
      canvas.addEventListener('wheel', (e) => {
        e.preventDefault();
        const pt = getCanvasCoords(e);
        const hit = hitTestCanvas(pt);
        const target = (hit && hit.layer) ? hit.layer : APP_STATE.layers.find(l => l.id === APP_STATE.selectedLayerId);
        if (!target) return;

        if (target.id !== APP_STATE.selectedLayerId) {
          selectLayer(target.id);
        }

        const delta = e.deltaY < 0 ? 1 : -1;

        if (e.shiftKey) {
          // Rotate target
          if (target.type === 'sprite' || (target.type === 'text' && target.pathMode === 'straight')) {
            target.rotation = ((target.rotation || 0) + delta * 5 + 360) % 360;
          } else if (target.type === 'gauge' || target.type === 'ring' || target.type === 'text') {
            target.startAngle = ((target.startAngle !== undefined ? target.startAngle : 270) + delta * 5 + 360) % 360;
          }
        } else {
          // Scale or radius
          if (target.type === 'sprite' || target.type === 'mascot' || target.type === 'bitmap') {
            target.scale = Math.max(1, Math.min(8, (target.scale || 2) + delta));
          } else if (target.type === 'gauge' || target.type === 'ring') {
            target.radius = Math.max(20, Math.min(114, (target.radius || 112) + (delta * 2)));
          } else if (target.type === 'text') {
            if (target.pathMode === 'straight') {
              target.fontSize = Math.max(8, Math.min(32, (target.fontSize || 12) + delta * 2));
            } else {
              target.radius = Math.max(20, Math.min(112, (target.radius || 88) + (delta * 2)));
            }
          }
        }

        syncInspectorInputs(target);
        updateCodeOutput();
      }, { passive: false });
    }

    /* ==========================================================================
     * 4. Display Engine Initialization & Lifecycle
     * ========================================================================== */
    window.addEventListener('DOMContentLoaded', () => {
      canvas = document.getElementById('cv');
      ctx = canvas.getContext('2d');

      // Initialize Pixel Editor modal components
      initPixelEditor();

      // Setup Canvas Interactive Manipulation (Drag, Snap & Wheel Resize)
      setupCanvasInteractions();

      // Render Empty Layer List & Inspector
      renderLayerList();
      renderLayerInspector();

      // Start display rendering loop
      requestAnimationFrame(mainAnimationLoop);

      // Start firmware emulator
      startSimulator();

      logTerminal('[SYS] Boot Display Studio initialized for GC9A01 240x240 Circular Display.', 'info');
    });

    /* ==========================================================================
     * 5. Universal Rendering Pipeline
     * ========================================================================== */
