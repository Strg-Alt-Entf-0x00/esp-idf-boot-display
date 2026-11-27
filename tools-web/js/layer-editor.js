"use strict";

function createMascotPixelArray() {
      const arr = new Array(16 * 16).fill(null);
      const W = '#ffffff';
      const B = '#00d4ff';
      const mascotPx = [
        [6,1,W],[7,1,W],[8,1,W],[9,1,W],[5,2,W],[6,2,W],[7,2,W],[8,2,W],[9,2,W],[10,2,W],
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
        arr[y * 16 + x] = col;
      });
      return arr;
    }

    function renderLayerList() {
      const container = document.getElementById('layerListContainer');
      container.innerHTML = '';

      if (APP_STATE.layers.length === 0) {
        container.innerHTML = `
          <div class="empty-layers-msg">
            No layers in composition.<br>
            Click <strong>+ Add Layer</strong> or select a <strong>Template</strong> above.
          </div>
        `;
        return;
      }

      APP_STATE.layers.forEach((layer, index) => {
        const item = document.createElement('div');
        item.className = `layer-item ${layer.id === APP_STATE.selectedLayerId ? 'selected' : ''} ${!layer.visible ? 'hidden-layer' : ''}`;
        item.onclick = () => selectLayer(layer.id);

        let iconSvg = '';
        if (layer.type === 'sprite') {
          iconSvg = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 19l7-7 3 3-7 7-3-3z"></path><path d="M18 13l-1.5-7.5L2 2l3.5 14.5L13 18l5-5z"></path></svg>';
        } else if (layer.type === 'gauge' || layer.type === 'ring') {
          iconSvg = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"></circle><polyline points="12 6 12 12 16 14"></polyline></svg>';
        } else if (layer.type === 'text') {
          iconSvg = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="4 7 4 4 20 4 20 7"></polyline><line x1="9" y1="20" x2="15" y2="20"></line><line x1="12" y1="4" x2="12" y2="20"></line></svg>';
        } else {
          iconSvg = '<svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="18" height="18" rx="2" ry="2"></rect><circle cx="8.5" cy="8.5" r="1.5"></circle><polyline points="21 15 16 10 5 21"></polyline></svg>';
        }

        item.innerHTML = `
          <div class="layer-meta">
            <span class="layer-type-icon">${iconSvg}</span>
            <span class="layer-name">${layer.name}</span>
          </div>
          <div class="layer-actions">
            <button class="layer-btn ${layer.visible ? 'active-vis' : ''}" title="Toggle Visibility" onclick="toggleLayerVis('${layer.id}', event)">
              <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
                <circle cx="12" cy="12" r="3"></circle>
              </svg>
            </button>
            <button class="layer-btn" title="Move Up" onclick="moveLayer('${layer.id}', -1, event)" ${index === 0 ? 'disabled' : ''}>
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="18 15 12 9 6 15"></polyline></svg>
            </button>
            <button class="layer-btn" title="Move Down" onclick="moveLayer('${layer.id}', 1, event)" ${index === APP_STATE.layers.length - 1 ? 'disabled' : ''}>
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="6 9 12 15 18 9"></polyline></svg>
            </button>
            <button class="layer-btn" title="Delete Layer" onclick="deleteLayer('${layer.id}', event)">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>
            </button>
          </div>
        `;
        container.appendChild(item);
      });
    }

    function selectLayer(id) {
      APP_STATE.selectedLayerId = id;
      renderLayerList();
      renderLayerInspector();
    }

    function toggleLayerVis(id, evt) {
      evt.stopPropagation();
      const layer = APP_STATE.layers.find(l => l.id === id);
      if (layer) {
        layer.visible = !layer.visible;
        renderLayerList();
      }
    }

    function moveLayer(id, delta, evt) {
      evt.stopPropagation();
      const idx = APP_STATE.layers.findIndex(l => l.id === id);
      if (idx < 0) return;
      const targetIdx = idx + delta;
      if (targetIdx < 0 || targetIdx >= APP_STATE.layers.length) return;

      const [removed] = APP_STATE.layers.splice(idx, 1);
      APP_STATE.layers.splice(targetIdx, 0, removed);
      renderLayerList();
      updateCodeOutput();
    }

    function deleteLayer(id, evt) {
      evt.stopPropagation();
      APP_STATE.layers = APP_STATE.layers.filter(l => l.id !== id);
      if (APP_STATE.selectedLayerId === id) {
        APP_STATE.selectedLayerId = APP_STATE.layers[0] ? APP_STATE.layers[0].id : null;
      }
      renderLayerList();
      renderLayerInspector();
      updateCodeOutput();
    }

    function addSelectedElement() {
      const type = document.getElementById('selAddElementType').value;
      const id = 'layer_' + Date.now();

      if (type === 'sprite') {
        APP_STATE.layers.push({
          id,
          type: 'sprite',
          name: `Sprite Graphic ${APP_STATE.layers.length + 1}`,
          visible: true,
          w: 24,
          h: 24,
          scale: 2,
          offsetX: 0,
          offsetY: 0,
          rotation: 0,
          modifier: 'none',
          pixels: new Array(24 * 24).fill(null)
        });
        APP_STATE.selectedLayerId = id;
        openSpriteStudioModal(id);
      } else if (type === 'gauge') {
        APP_STATE.layers.push({
          id,
          type: 'gauge',
          name: `Progress Gauge ${APP_STATE.layers.length + 1}`,
          visible: true,
          radius: 100,
          thickness: 6,
          style: 'dots',
          segments: 40,
          startAngle: 270,
          spanAngle: 360,
          color: '#38bdf8',
          trackColor: 'rgba(255, 255, 255, 0.12)'
        });
        APP_STATE.selectedLayerId = id;
      } else if (type === 'text') {
        APP_STATE.layers.push({
          id,
          type: 'text',
          name: `Text ${APP_STATE.layers.length + 1}`,
          visible: true,
          text: 'SYSTEM {progress}%',
          pathMode: 'straight',
          fontSize: 12,
          color: '#ffffff',
          offsetX: 0,
          offsetY: 0,
          rotation: 0,
          modifier: 'none',
          radius: 88,
          startAngle: 90,
          letterSpacing: 8,
          inward: true,
          flip: false,
          direction: 'cw'
        });
        APP_STATE.selectedLayerId = id;
      }

      renderLayerList();
      renderLayerInspector();
      updateCodeOutput();
      logTerminal(`[COMPOSER] Added ${type} primitive layer.`, 'info');
    }

    function renderLayerInspector() {
      const layer = APP_STATE.layers.find(l => l.id === APP_STATE.selectedLayerId);
      const title = document.getElementById('inspectorTitle');
      const container = document.getElementById('inspectorFields');

      if (!layer) {
        title.textContent = 'LAYER PROPERTIES';
        container.innerHTML = '<span style="font-size:11px; color:var(--text-muted);">No layer selected.</span>';
        return;
      }

      title.textContent = `${layer.name.toUpperCase()} (${layer.type.toUpperCase()})`;
      let html = `
        <div class="field-group">
          <label class="field-label">Layer Name</label>
          <input type="text" class="input-field" value="${layer.name}" onchange="updateProp('name', this.value)">
        </div>
      `;

      if (layer.type === 'sprite') {
        html += `
          <div class="field-row">
            <div class="field-group">
              <label class="field-label">Scale Factor</label>
              <input type="number" id="inpScale" class="input-field" value="${layer.scale || 2}" min="1" max="8" onchange="updateProp('scale', parseInt(this.value))">
            </div>
            <div class="field-group">
              <label class="field-label">Rotation Angle (deg)</label>
              <input type="number" id="inpRotation" class="input-field" value="${layer.rotation || 0}" min="0" max="360" onchange="updateProp('rotation', parseInt(this.value))">
            </div>
          </div>
          <div class="field-row">
            <div class="field-group">
              <label class="field-label">Offset X (px)</label>
              <input type="number" id="inpOffsetX" class="input-field" value="${layer.offsetX || 0}" min="-100" max="100" onchange="updateProp('offsetX', parseInt(this.value))">
            </div>
            <div class="field-group">
              <label class="field-label">Offset Y (px)</label>
              <input type="number" id="inpOffsetY" class="input-field" value="${layer.offsetY || 0}" min="-100" max="100" onchange="updateProp('offsetY', parseInt(this.value))">
            </div>
          </div>
          <div class="field-group">
            <label class="field-label">Dynamic Boot Modifier</label>
            <select class="select-field" onchange="updateProp('modifier', this.value)">
              <option value="none" ${layer.modifier === 'none' ? 'selected' : ''}>None (Static Graphic)</option>
              <option value="spin" ${layer.modifier === 'spin' ? 'selected' : ''}>Spin (0 -> 360 deg with Progress)</option>
              <option value="pulse" ${layer.modifier === 'pulse' ? 'selected' : ''}>Pulse / Breathe (Oscillate Scale)</option>
              <option value="bounce" ${layer.modifier === 'bounce' ? 'selected' : ''}>Bounce / Hop (Vertical Motion)</option>
              <option value="grow" ${layer.modifier === 'grow' ? 'selected' : ''}>Grow with Progress (0% -> 100%)</option>
              <option value="eye_track" ${layer.modifier === 'eye_track' ? 'selected' : ''}>Eye Tracking (Pupils Shift Right)</option>
            </select>
          </div>
          <button class="btn btn-primary" style="margin-top: 4px;" onclick="openSpriteStudioModal('${layer.id}')">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 19l7-7 3 3-7 7-3-3z"></path><path d="M18 13l-1.5-7.5L2 2l3.5 14.5L13 18l5-5z"></path></svg>
            Edit in Sprite Studio
          </button>
        `;
      } else if (layer.type === 'gauge' || layer.type === 'ring') {
        html += `
          <div class="field-row">
            <div class="field-group">
              <label class="field-label">Radius (px)</label>
              <input type="number" id="inpRadius" class="input-field" value="${layer.radius || 112}" min="20" max="114" onchange="updateProp('radius', parseInt(this.value))">
            </div>
            <div class="field-group">
              <label class="field-label">Thickness (px)</label>
              <input type="number" class="input-field" value="${layer.thickness || 8}" min="1" max="24" onchange="updateProp('thickness', parseInt(this.value))">
            </div>
          </div>
          <div class="field-row">
            <div class="field-group">
              <label class="field-label">Track Style</label>
              <select class="select-field" onchange="updateProp('style', this.value)">
                <option value="dots" ${layer.style === 'dots' ? 'selected' : ''}>Segmented Dots</option>
                <option value="solid" ${layer.style === 'solid' ? 'selected' : ''}>Continuous Solid Arc</option>
                <option value="ticks" ${layer.style === 'ticks' ? 'selected' : ''}>Radial Ticks / Dashes</option>
              </select>
            </div>
            <div class="field-group">
              <label class="field-label">Segments Count</label>
              <input type="number" class="input-field" value="${layer.segments || 48}" min="8" max="72" onchange="updateProp('segments', parseInt(this.value))">
            </div>
          </div>
          <div class="field-row">
            <div class="field-group">
              <label class="field-label">Start Angle (deg)</label>
              <input type="number" id="inpStartAngle" class="input-field" value="${layer.startAngle !== undefined ? layer.startAngle : 270}" min="0" max="360" onchange="updateProp('startAngle', parseInt(this.value))">
            </div>
            <div class="field-group">
              <label class="field-label">Span Angle (deg)</label>
              <input type="number" class="input-field" value="${layer.spanAngle !== undefined ? layer.spanAngle : 360}" min="45" max="360" onchange="updateProp('spanAngle', parseInt(this.value))">
            </div>
          </div>
          <div class="field-group">
            <label class="field-label">Active Color</label>
            <div class="color-row">
              <input type="color" class="color-picker" value="${layer.color || '#00d4ff'}" onchange="updateProp('color', this.value)">
              <span class="color-hex">${layer.color || '#00d4ff'} (${getRgb565Hex(layer.color || '#00d4ff')})</span>
            </div>
          </div>
        `;
      } else if (layer.type === 'text') {
        const isStraight = layer.pathMode === 'straight';
        html += `
          <div class="field-group">
            <label class="field-label">Display String ({progress} Token Supported)</label>
            <input type="text" class="input-field" value="${layer.text || ''}" onchange="updateProp('text', this.value)">
          </div>
          <div class="field-row">
            <div class="field-group">
              <label class="field-label">Path Mode</label>
              <select class="select-field" onchange="updateProp('pathMode', this.value)">
                <option value="straight" ${isStraight ? 'selected' : ''}>Straight (Horizontal)</option>
                <option value="arc" ${!isStraight ? 'selected' : ''}>Circular Arc</option>
              </select>
            </div>
            <div class="field-group">
              <label class="field-label">Font Size</label>
              <select class="select-field" onchange="updateProp('fontSize', parseInt(this.value))">
                <option value="8" ${layer.fontSize === 8 ? 'selected' : ''}>8 px (Micro)</option>
                <option value="12" ${layer.fontSize === 12 || !layer.fontSize ? 'selected' : ''}>12 px (Regular)</option>
                <option value="16" ${layer.fontSize === 16 ? 'selected' : ''}>16 px (Medium)</option>
                <option value="22" ${layer.fontSize === 22 ? 'selected' : ''}>22 px (Headline)</option>
              </select>
            </div>
          </div>
        `;

        if (isStraight) {
          html += `
            <div class="field-row">
              <div class="field-group">
                <label class="field-label">Offset X (px)</label>
                <input type="number" id="inpOffsetX" class="input-field" value="${layer.offsetX || 0}" min="-100" max="100" onchange="updateProp('offsetX', parseInt(this.value))">
              </div>
              <div class="field-group">
                <label class="field-label">Offset Y (px)</label>
                <input type="number" id="inpOffsetY" class="input-field" value="${layer.offsetY || 0}" min="-100" max="100" onchange="updateProp('offsetY', parseInt(this.value))">
              </div>
            </div>
            <div class="field-group">
              <label class="field-label">Rotation Angle (deg)</label>
              <input type="number" id="inpRotation" class="input-field" value="${layer.rotation || 0}" min="0" max="360" onchange="updateProp('rotation', parseInt(this.value))">
            </div>
          `;
        } else {
          html += `
            <div class="field-row">
              <div class="field-group">
                <label class="field-label">Arc Radius (px)</label>
                <input type="number" id="inpRadius" class="input-field" value="${layer.radius || 88}" min="20" max="114" onchange="updateProp('radius', parseInt(this.value))">
              </div>
              <div class="field-group">
                <label class="field-label">Start Angle (deg)</label>
                <input type="number" id="inpStartAngle" class="input-field" value="${layer.startAngle !== undefined ? layer.startAngle : 90}" min="0" max="360" onchange="updateProp('startAngle', parseInt(this.value))">
              </div>
            </div>
            <div class="field-row">
              <div class="field-group">
                <label class="field-label">Letter Spacing (deg)</label>
                <input type="number" class="input-field" value="${layer.letterSpacing || 8}" min="2" max="25" onchange="updateProp('letterSpacing', parseInt(this.value))">
              </div>
              <div class="field-group">
                <label class="field-label">Direction</label>
                <select class="select-field" onchange="updateProp('direction', this.value)">
                  <option value="ltr" ${layer.direction === 'ltr' || !layer.direction ? 'selected' : ''}>Natural LTR (Auto)</option>
                  <option value="cw" ${layer.direction === 'cw' ? 'selected' : ''}>Clockwise (CW)</option>
                  <option value="ccw" ${layer.direction === 'ccw' ? 'selected' : ''}>Counter-CW (CCW)</option>
                </select>
              </div>
            </div>
            <div style="display: flex; gap: 14px; margin-top: 4px;">
              <label style="display: flex; align-items: center; gap: 6px; font-size: 11px; color: var(--text-dim); cursor: pointer;">
                <input type="checkbox" ${layer.inward !== false ? 'checked' : ''} onchange="updateProp('inward', this.checked)">
                <span>Facing Inward</span>
              </label>
              <label style="display: flex; align-items: center; gap: 6px; font-size: 11px; color: var(--text-dim); cursor: pointer;">
                <input type="checkbox" ${layer.flip ? 'checked' : ''} onchange="updateProp('flip', this.checked)">
                <span style="color: var(--accent-ice);">Flip 180° (Upside-Down Fix)</span>
              </label>
            </div>
          `;
        }

        html += `
          <div class="field-group" style="margin-top: 6px;">
            <label class="field-label">Text Color</label>
            <div class="color-row">
              <input type="color" class="color-picker" value="${layer.color || '#ffffff'}" onchange="updateProp('color', this.value)">
              <span class="color-hex">${layer.color || '#ffffff'} (${getRgb565Hex(layer.color || '#ffffff')})</span>
            </div>
          </div>
          <div class="field-group">
            <label class="field-label">Dynamic Modifier</label>
            <select class="select-field" onchange="updateProp('modifier', this.value)">
              <option value="none" ${layer.modifier === 'none' ? 'selected' : ''}>None (Static Text)</option>
              <option value="pulse" ${layer.modifier === 'pulse' ? 'selected' : ''}>Pulse (Scale Oscillate)</option>
              <option value="blink" ${layer.modifier === 'blink' ? 'selected' : ''}>Blink / Flash</option>
            </select>
          </div>
        `;
      }

      container.innerHTML = html;
    }

    function syncInspectorInputs(layer) {
      if (!layer || layer.id !== APP_STATE.selectedLayerId) return;
      const fX = document.getElementById('inpOffsetX');
      if (fX && layer.offsetX !== undefined) fX.value = layer.offsetX;
      const fY = document.getElementById('inpOffsetY');
      if (fY && layer.offsetY !== undefined) fY.value = layer.offsetY;
      const fScale = document.getElementById('inpScale');
      if (fScale && layer.scale !== undefined) fScale.value = layer.scale;
      const fRadius = document.getElementById('inpRadius');
      if (fRadius && layer.radius !== undefined) fRadius.value = layer.radius;
      const fStartAngle = document.getElementById('inpStartAngle');
      if (fStartAngle && layer.startAngle !== undefined) fStartAngle.value = layer.startAngle;
      const fRot = document.getElementById('inpRotation');
      if (fRot && layer.rotation !== undefined) fRot.value = layer.rotation;
    }

    function updateProp(prop, val) {
      const layer = APP_STATE.layers.find(l => l.id === APP_STATE.selectedLayerId);
      if (!layer) return;
      layer[prop] = val;
      renderLayerList();
      renderLayerInspector();
      updateCodeOutput();
    }
