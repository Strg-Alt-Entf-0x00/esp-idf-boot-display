"use strict";

function renderDisplay() {
      const w = canvas.width;
      const h = canvas.height;
      const cx = w / 2;
      const cy = h / 2;

      // Fill circular dial background
      ctx.fillStyle = APP_STATE.bgColor;
      ctx.fillRect(0, 0, w, h);

      // If composition is empty, show subtle guide reticle
      if (APP_STATE.layers.length === 0) {
        ctx.save();
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 6]);
        ctx.beginPath();
        ctx.arc(cx, cy, 110, 0, Math.PI * 2);
        ctx.stroke();

        ctx.font = '10px "JetBrains Mono", monospace';
        ctx.fillStyle = 'rgba(255, 255, 255, 0.25)';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('[ EMPTY COMPOSITION ]', cx, cy - 8);
        ctx.font = '9px system-ui, sans-serif';
        ctx.fillText('Select a Template or click + Add Layer', cx, cy + 10);
        ctx.restore();
        return;
      }

      // Render visible layers in ascending order
      for (const layer of APP_STATE.layers) {
        if (!layer.visible) continue;

        ctx.save();
        if (layer.type === 'sprite') {
          renderSpriteLayer(ctx, cx, cy, layer, APP_STATE.bootProgress);
        } else if (layer.type === 'gauge' || layer.type === 'ring') {
          renderGaugeLayer(ctx, cx, cy, layer, APP_STATE.bootProgress);
        } else if (layer.type === 'text') {
          renderTextLayer(ctx, cx, cy, layer, APP_STATE.bootProgress);
        } else if (layer.type === 'mascot') {
          renderMascotLayerLegacy(ctx, cx, cy, layer, APP_STATE.bootProgress);
        } else if (layer.type === 'bitmap') {
          renderBitmapLayerLegacy(ctx, cx, cy, layer);
        }
        ctx.restore();
      }

      // Render Snap Crosshairs
      if (snapCrosshair.x !== null) {
        ctx.save();
        ctx.strokeStyle = 'rgba(56, 189, 248, 0.75)';
        ctx.lineWidth = 1;
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.moveTo(120, 10);
        ctx.lineTo(120, 230);
        ctx.stroke();
        ctx.restore();
      }
      if (snapCrosshair.y !== null) {
        ctx.save();
        ctx.strokeStyle = 'rgba(56, 189, 248, 0.75)';
        ctx.lineWidth = 1;
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.moveTo(10, 120);
        ctx.lineTo(230, 120);
        ctx.stroke();
        ctx.restore();
      }

      // Render Selection Overlay
      renderSelectionOverlay(ctx, cx, cy);
    }

    function renderSpriteLayer(c, cx, cy, layer, progress) {
      if (!layer.pixels || !layer.w || !layer.h) return;
      const scale = layer.scale || 2;
      const baseRot = ((layer.rotation || 0) * Math.PI) / 180;
      let offsetX = layer.offsetX || 0;
      let offsetY = layer.offsetY || 0;

      // Apply Dynamic Boot Modifier
      let modScale = 1.0;
      let modRot = 0;
      let eyeShift = 0;

      if (layer.modifier === 'spin') {
        modRot = (progress / 100) * Math.PI * 2;
      } else if (layer.modifier === 'pulse') {
        modScale = 1.0 + 0.22 * Math.sin((progress / 100) * Math.PI * 4);
      } else if (layer.modifier === 'bounce') {
        offsetY += -Math.abs(Math.sin((progress / 100) * Math.PI * 6)) * 14;
      } else if (layer.modifier === 'grow') {
        modScale = Math.max(0.1, progress / 100);
      } else if (layer.modifier === 'eye_track') {
        eyeShift = progress > 50 ? 1 : 0;
      }

      const finalScale = scale * modScale;
      c.translate(cx + offsetX, cy + offsetY);
      c.rotate(baseRot + modRot);

      const startX = -(layer.w * finalScale) / 2;
      const startY = -(layer.h * finalScale) / 2;

      for (let y = 0; y < layer.h; y++) {
        for (let x = 0; x < layer.w; x++) {
          const color = layer.pixels[y * layer.w + x];
          if (color && color !== 'transparent') {
            c.fillStyle = color;
            const drawX = startX + (x + eyeShift) * finalScale;
            const drawY = startY + y * finalScale;
            c.fillRect(drawX, drawY, finalScale, finalScale);
          }
        }
      }
    }

    function renderGaugeLayer(c, cx, cy, layer, progress) {
      const radius = layer.radius || 112;
      const thickness = layer.thickness || 8;
      const segments = layer.segments || 48;
      const startAngleDeg = layer.startAngle !== undefined ? layer.startAngle : 270;
      const spanAngleDeg = layer.spanAngle !== undefined ? layer.spanAngle : 360;
      const pRatio = Math.max(0, Math.min(1, progress / 100));

      const startRad = (startAngleDeg * Math.PI) / 180;
      const totalSpanRad = (spanAngleDeg * Math.PI) / 180;
      const activeRad = totalSpanRad * pRatio;

      if (layer.style === 'solid') {
        c.lineWidth = thickness;
        c.lineCap = 'round';

        // Background Track
        c.beginPath();
        c.arc(cx, cy, radius, startRad, startRad + totalSpanRad);
        c.strokeStyle = layer.trackColor || 'rgba(255, 255, 255, 0.1)';
        c.stroke();

        // Active Arc
        if (pRatio > 0.005) {
          c.beginPath();
          c.arc(cx, cy, radius, startRad, startRad + activeRad);
          c.strokeStyle = layer.color;
          c.shadowColor = layer.color;
          c.shadowBlur = 8;
          c.stroke();
        }
      } else if (layer.style === 'ticks') {
        const tickLen = thickness * 1.6;
        for (let i = 0; i < segments; i++) {
          const segRatio = i / (spanAngleDeg === 360 ? segments : (segments - 1 || 1));
          const angle = startRad + segRatio * totalSpanRad;
          const rInner = radius - tickLen / 2;
          const rOuter = radius + tickLen / 2;

          const x1 = cx + rInner * Math.cos(angle);
          const y1 = cy + rInner * Math.sin(angle);
          const x2 = cx + rOuter * Math.cos(angle);
          const y2 = cy + rOuter * Math.sin(angle);

          c.beginPath();
          c.moveTo(x1, y1);
          c.lineTo(x2, y2);
          c.lineWidth = Math.max(1.5, thickness / 3.5);
          if (segRatio <= pRatio) {
            c.strokeStyle = layer.color;
            c.shadowColor = layer.color;
            c.shadowBlur = 6;
          } else {
            c.strokeStyle = layer.trackColor || 'rgba(255, 255, 255, 0.12)';
            c.shadowBlur = 0;
          }
          c.stroke();
        }
      } else {
        // Segmented Dots
        const dotRadius = Math.max(1.5, thickness / 2);
        const activeDots = Math.floor(segments * pRatio);

        for (let i = 0; i < segments; i++) {
          const segRatio = i / (spanAngleDeg === 360 ? segments : (segments - 1 || 1));
          const angle = startRad + segRatio * totalSpanRad;
          const x = cx + radius * Math.cos(angle);
          const y = cy + radius * Math.sin(angle);

          c.beginPath();
          c.arc(x, y, dotRadius, 0, Math.PI * 2);

          if (i < activeDots) {
            c.fillStyle = layer.color;
            c.shadowColor = layer.color;
            c.shadowBlur = (i === activeDots - 1) ? 8 : 3;
          } else {
            c.fillStyle = layer.trackColor || 'rgba(255, 255, 255, 0.12)';
            c.shadowBlur = 0;
          }
          c.fill();
        }
      }
    }

    function renderTextLayer(c, cx, cy, layer, progress) {
      let text = layer.text || '';
      text = text.replace('{progress}', Math.round(progress));

      const fontSize = layer.fontSize || 12;
      c.font = `bold ${fontSize}px "JetBrains Mono", Consolas, monospace`;
      c.fillStyle = layer.color || '#ffffff';
      c.textAlign = 'center';
      c.textBaseline = 'middle';

      if (layer.pathMode === 'straight') {
        const x = cx + (layer.offsetX || 0);
        const y = cy + (layer.offsetY || 0);
        const rot = ((layer.rotation || 0) * Math.PI) / 180;

        c.translate(x, y);
        c.rotate(rot);

        if (layer.modifier === 'pulse') {
          const pScale = 1.0 + 0.15 * Math.sin((progress / 100) * Math.PI * 4);
          c.scale(pScale, pScale);
        } else if (layer.modifier === 'blink') {
          if (Math.floor(progress / 15) % 2 === 1) return;
        }

        c.fillText(text, 0, 0);
        return;
      }

      // Circular Arc Path
      const radius = layer.radius || 88;
      const startDeg = layer.startAngle !== undefined ? layer.startAngle : 90;
      const spacingDeg = layer.letterSpacing || 8;
      const dir = layer.direction || 'ltr'; // 'ltr' (auto natural reading) | 'cw' | 'ccw'
      const flip = !!layer.flip;
      const inward = layer.inward !== false; // Default: facing inward towards center

      const totalAngle = (text.length - 1) * spacingDeg;
      const centerAngle = (startDeg * Math.PI) / 180;
      const halfTotal = (totalAngle * Math.PI) / 360;

      // Determine progression direction:
      // In the lower hemisphere (0 < startDeg < 180, e.g. 90 deg bottom), increasing angle moves right-to-left.
      // Natural LTR reading requires decreasing angle!
      const isLowerHemisphere = (startDeg > 0 && startDeg < 180);
      let stepSign = 1;
      let startOffset = -halfTotal;

      if (dir === 'ltr') {
        if (isLowerHemisphere) {
          stepSign = -1;
          startOffset = halfTotal;
        } else {
          stepSign = 1;
          startOffset = -halfTotal;
        }
      } else if (dir === 'ccw') {
        stepSign = -1;
        startOffset = halfTotal;
      } else { // 'cw'
        stepSign = 1;
        startOffset = -halfTotal;
      }

      for (let i = 0; i < text.length; i++) {
        const charAngle = centerAngle + startOffset + (stepSign * i * spacingDeg * Math.PI) / 180;
        const x = cx + radius * Math.cos(charAngle);
        const y = cy + radius * Math.sin(charAngle);

        c.save();
        c.translate(x, y);

        // Compute orientation: inward vs outward + 180 deg flip fix
        let rot = charAngle + (inward ? -Math.PI / 2 : Math.PI / 2);
        if (flip) {
          rot += Math.PI;
        }
        c.rotate(rot);
        c.fillText(text[i], 0, 0);
        c.restore();
      }
    }

    // Legacy renderers for backward compatibility
    function renderMascotLayerLegacy(c, cx, cy, layer, progress) {
      const scale = layer.scale || 3;
      const x = cx + (layer.offsetX || 0);
      const y = cy + (layer.offsetY || 0);
      const px = [
        [0,0,0,0,1,1,1,1,0,0,0,0],[0,0,1,1,1,1,1,1,1,1,0,0],[0,1,1,1,1,1,1,1,1,1,1,0],
        [1,1,1,1,1,1,1,1,1,1,1,1],[1,1,2,2,1,1,1,1,2,2,1,1],[1,1,2,2,1,1,1,1,2,2,1,1],
        [1,1,1,1,1,1,1,1,1,1,1,1],[1,1,1,1,1,1,1,1,1,1,1,1],[1,1,1,1,1,1,1,1,1,1,1,1],
        [1,1,1,1,1,1,1,1,1,1,1,1],[1,1,1,1,1,1,1,1,1,1,1,1],[1,1,1,1,1,1,1,1,1,1,1,1],
        [1,1,0,1,1,0,0,1,1,0,1,1],[1,0,0,0,1,0,0,1,0,0,0,1]
      ];
      const startX = x - (6 * scale);
      const startY = y - (7 * scale);
      for (let r = 0; r < 14; r++) {
        for (let col = 0; col < 12; col++) {
          const val = px[r][col];
          if (val === 1) {
            c.fillStyle = layer.bodyColor || '#ffffff';
            c.fillRect(startX + col * scale, startY + r * scale, scale, scale);
          } else if (val === 2) {
            c.fillStyle = layer.accentColor || '#00d4ff';
            const eyeOffset = (progress > 50) ? 1 : 0;
            c.fillRect(startX + (col + eyeOffset) * scale, startY + r * scale, scale, scale);
          }
        }
      }
    }

    function renderBitmapLayerLegacy(c, cx, cy, layer) {
      if (!layer.data || !layer.w || !layer.h) return;
      const scale = layer.scale || 1;
      const startX = cx - (layer.w * scale) / 2 + (layer.offsetX || 0);
      const startY = cy - (layer.h * scale) / 2 + (layer.offsetY || 0);
      c.fillStyle = layer.color || '#38bdf8';
      for (let y = 0; y < layer.h; y++) {
        for (let x = 0; x < layer.w; x++) {
          if (layer.data[y * layer.w + x] === 1) {
            c.fillRect(startX + x * scale, startY + y * scale, scale, scale);
          }
        }
      }
    }

    function mainAnimationLoop(ts) {
      const dt = ts - lastTimestamp;
      lastTimestamp = ts;

      if (APP_STATE.isPlaying) {
        let p = APP_STATE.bootProgress + (dt * 0.04);
        if (p >= 100) {
          if (singleCycleActive) {
            p = 100;
            APP_STATE.isPlaying = false;
            singleCycleActive = false;
            updatePlayButtonUI();
          } else {
            p = 0;
            simCycles++;
          }
        }
        setBootProgress(p);
      }

      renderDisplay();
      animFrameId = requestAnimationFrame(mainAnimationLoop);
    }

    /* ==========================================================================
     * 6. Playback & Simulation Controls
     * ========================================================================== */
