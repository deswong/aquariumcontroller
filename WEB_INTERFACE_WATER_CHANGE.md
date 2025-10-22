# Water Change Web Interface - Quick Reference

## API Endpoints Summary

### Water Change Operations

#### Get Status
```http
GET /api/waterchange/status
```
**Response:**
```json
{
  "inProgress": false,
  "phase": 0,
  "phaseDescription": "Idle",
  "elapsedTime": 0,
  "systemsPaused": false,
  "currentVolume": 0,
  "tankVolume": 100.0,
  "schedule": 3,
  "scheduledVolumePercent": 25.0,
  "scheduledVolume": 25.0,
  "daysSinceLastChange": 5,
  "daysUntilNextChange": 2,
  "isOverdue": false,
  "daysSinceFilterMaintenance": 14
}
```

**If inProgress=true, also includes:**
```json
{
  "tempBefore": 25.5,
  "phBefore": 7.2,
  "tdsBefore": 350
}
```

#### Start Water Change
```http
POST /api/waterchange/start
Content-Type: application/json

{
  "volume": 20.0  // Optional, omit to use scheduled volume
}
```
**Response:**
```json
{"status": "ok"}
```

#### End Water Change
```http
POST /api/waterchange/end
```
**Response:**
```json
{"status": "ok"}
```

#### Cancel Water Change
```http
POST /api/waterchange/cancel
```
**Response:**
```json
{"status": "ok"}
```

#### Get History
```http
GET /api/waterchange/history?count=10
```
**Response:**
```json
[
  {
    "startTime": 1699123456,
    "endTime": 1699125256,
    "volume": 25.0,
    "tempBefore": 25.5,
    "tempAfter": 24.8,
    "phBefore": 7.2,
    "phAfter": 7.3,
    "tdsBefore": 350,
    "tdsAfter": 320,
    "duration": 30,
    "successful": true
  }
]
```

### Filter Maintenance

#### Record Maintenance
```http
POST /api/maintenance/filter
Content-Type: application/json

{
  "notes": "Replaced filter cartridge"  // Optional
}
```
**Response:**
```json
{"status": "ok"}
```

#### Get Maintenance History
```http
GET /api/maintenance/filter?count=10
```
**Response:**
```json
[
  {
    "timestamp": 1699123456,
    "notes": "Replaced filter cartridge"
  },
  {
    "timestamp": 1696531456,
    "notes": "Cleaned impeller"
  }
]
```

## Minimal HTML Example

```html
<!-- Water Change Section -->
<div id="waterChangeSection" class="card">
  <h2>Water Change</h2>
  
  <!-- Status -->
  <div class="status-display">
    <div class="status-item">
      <label>Status:</label>
      <span id="wcStatus" class="badge">Idle</span>
    </div>
    <div class="status-item">
      <label>Systems:</label>
      <span id="wcSystems" class="badge">Active</span>
    </div>
    <div class="status-item">
      <label>Elapsed:</label>
      <span id="wcElapsed">0</span> minutes
    </div>
  </div>
  
  <!-- Initial Readings (shown during water change) -->
  <div id="beforeReadings" class="readings-panel" style="display:none">
    <h3>Initial Readings</h3>
    <div class="reading">Temp: <span id="tempBefore">--</span>°C</div>
    <div class="reading">pH: <span id="phBefore">--</span></div>
    <div class="reading">TDS: <span id="tdsBefore">--</span> ppm</div>
  </div>
  
  <!-- Controls -->
  <div class="controls">
    <div id="startControls">
      <input type="number" id="volumeInput" 
             placeholder="Volume (L)" 
             step="0.1" min="0" />
      <button onclick="startWaterChange()" class="btn-primary">
        Start Water Change
      </button>
    </div>
    
    <div id="activeControls" style="display:none">
      <button onclick="endWaterChange()" class="btn-success">
        Complete Water Change
      </button>
      <button onclick="cancelWaterChange()" class="btn-danger">
        Cancel
      </button>
    </div>
  </div>
  
  <!-- Schedule Info -->
  <div class="info-panel">
    <p>Next scheduled: <span id="daysUntil">--</span> days</p>
    <p>Tank volume: <span id="tankVolume">--</span> L</p>
  </div>
</div>

<!-- Filter Maintenance Section -->
<div id="filterSection" class="card">
  <h2>Filter Maintenance</h2>
  
  <div class="status-display">
    <p>Last cleaned: <span id="daysSinceFilter">--</span> days ago</p>
  </div>
  
  <div class="controls">
    <input type="text" id="maintenanceNotes" 
           placeholder="Optional notes (e.g., 'Replaced cartridge')" />
    <button onclick="recordFilterMaintenance()" class="btn-primary">
      Record Maintenance
    </button>
  </div>
  
  <h3>Maintenance History</h3>
  <table id="filterHistory" class="data-table">
    <thead>
      <tr>
        <th>Date</th>
        <th>Notes</th>
      </tr>
    </thead>
    <tbody id="filterHistoryBody">
      <tr><td colspan="2">Loading...</td></tr>
    </tbody>
  </table>
</div>

<!-- Water Change History -->
<div id="historySection" class="card">
  <h2>Water Change History</h2>
  <table id="wcHistory" class="data-table">
    <thead>
      <tr>
        <th>Date</th>
        <th>Duration</th>
        <th>Volume</th>
        <th>Temp Δ</th>
        <th>pH Δ</th>
        <th>TDS Δ</th>
      </tr>
    </thead>
    <tbody id="wcHistoryBody">
      <tr><td colspan="6">Loading...</td></tr>
    </tbody>
  </table>
</div>
```

## Complete JavaScript

```javascript
// ===========================
// Water Change Functions
// ===========================

let updateInterval = null;

async function startWaterChange() {
  const volumeInput = document.getElementById('volumeInput');
  const volume = parseFloat(volumeInput.value) || 0; // 0 = use scheduled
  
  try {
    const response = await fetch('/api/waterchange/start', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({volume: volume})
    });
    
    if (response.ok) {
      volumeInput.value = '';
      await updateWaterChangeStatus();
      // Start polling for updates
      if (!updateInterval) {
        updateInterval = setInterval(updateWaterChangeStatus, 5000);
      }
    } else {
      alert('Failed to start water change');
    }
  } catch (error) {
    console.error('Error starting water change:', error);
    alert('Error starting water change');
  }
}

async function endWaterChange() {
  if (!confirm('Complete water change? Systems will be re-enabled.')) {
    return;
  }
  
  try {
    const response = await fetch('/api/waterchange/end', {
      method: 'POST'
    });
    
    if (response.ok) {
      await updateWaterChangeStatus();
      await loadWaterChangeHistory();
      // Stop polling
      if (updateInterval) {
        clearInterval(updateInterval);
        updateInterval = null;
      }
    } else {
      alert('Failed to complete water change');
    }
  } catch (error) {
    console.error('Error completing water change:', error);
    alert('Error completing water change');
  }
}

async function cancelWaterChange() {
  if (!confirm('Cancel water change? No data will be saved.')) {
    return;
  }
  
  try {
    const response = await fetch('/api/waterchange/cancel', {
      method: 'POST'
    });
    
    if (response.ok) {
      await updateWaterChangeStatus();
      // Stop polling
      if (updateInterval) {
        clearInterval(updateInterval);
        updateInterval = null;
      }
    } else {
      alert('Failed to cancel water change');
    }
  } catch (error) {
    console.error('Error cancelling water change:', error);
    alert('Error cancelling water change');
  }
}

async function updateWaterChangeStatus() {
  try {
    const response = await fetch('/api/waterchange/status');
    const data = await response.json();
    
    // Update status badge
    const statusEl = document.getElementById('wcStatus');
    statusEl.textContent = data.phaseDescription;
    statusEl.className = 'badge ' + (data.inProgress ? 'badge-warning' : 'badge-success');
    
    // Update systems status
    const systemsEl = document.getElementById('wcSystems');
    systemsEl.textContent = data.systemsPaused ? 'PAUSED' : 'Active';
    systemsEl.className = 'badge ' + (data.systemsPaused ? 'badge-danger' : 'badge-success');
    
    // Update elapsed time
    document.getElementById('wcElapsed').textContent = data.elapsedTime || 0;
    
    // Show/hide control sections
    document.getElementById('startControls').style.display = 
      data.inProgress ? 'none' : 'block';
    document.getElementById('activeControls').style.display = 
      data.inProgress ? 'block' : 'none';
    
    // Show/hide initial readings
    const beforeReadings = document.getElementById('beforeReadings');
    if (data.inProgress && data.tempBefore !== undefined) {
      beforeReadings.style.display = 'block';
      document.getElementById('tempBefore').textContent = data.tempBefore.toFixed(1);
      document.getElementById('phBefore').textContent = data.phBefore.toFixed(2);
      document.getElementById('tdsBefore').textContent = data.tdsBefore.toFixed(0);
    } else {
      beforeReadings.style.display = 'none';
    }
    
    // Update schedule info
    document.getElementById('daysUntil').textContent = data.daysUntilNextChange;
    document.getElementById('tankVolume').textContent = data.tankVolume.toFixed(1);
    
    // Update filter maintenance
    document.getElementById('daysSinceFilter').textContent = 
      data.daysSinceFilterMaintenance;
    
  } catch (error) {
    console.error('Error updating water change status:', error);
  }
}

async function loadWaterChangeHistory() {
  try {
    const response = await fetch('/api/waterchange/history?count=10');
    const history = await response.json();
    
    const tbody = document.getElementById('wcHistoryBody');
    
    if (history.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6">No history available</td></tr>';
      return;
    }
    
    tbody.innerHTML = history.map(record => {
      const date = new Date(record.endTime * 1000).toLocaleString();
      const tempDelta = (record.tempAfter - record.tempBefore).toFixed(1);
      const phDelta = (record.phAfter - record.phBefore).toFixed(2);
      const tdsDelta = (record.tdsAfter - record.tdsBefore).toFixed(0);
      
      return `
        <tr>
          <td>${date}</td>
          <td>${record.duration} min</td>
          <td>${record.volume.toFixed(1)} L</td>
          <td>${tempDelta > 0 ? '+' : ''}${tempDelta}°C</td>
          <td>${phDelta > 0 ? '+' : ''}${phDelta}</td>
          <td>${tdsDelta > 0 ? '+' : ''}${tdsDelta} ppm</td>
        </tr>
      `;
    }).join('');
    
  } catch (error) {
    console.error('Error loading water change history:', error);
    document.getElementById('wcHistoryBody').innerHTML = 
      '<tr><td colspan="6">Error loading history</td></tr>';
  }
}

// ===========================
// Filter Maintenance Functions
// ===========================

async function recordFilterMaintenance() {
  const notesInput = document.getElementById('maintenanceNotes');
  const notes = notesInput.value.trim();
  
  try {
    const response = await fetch('/api/maintenance/filter', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({notes: notes})
    });
    
    if (response.ok) {
      notesInput.value = '';
      await loadFilterHistory();
      await updateWaterChangeStatus(); // Refresh days count
      alert('Filter maintenance recorded');
    } else {
      alert('Failed to record filter maintenance');
    }
  } catch (error) {
    console.error('Error recording filter maintenance:', error);
    alert('Error recording filter maintenance');
  }
}

async function loadFilterHistory() {
  try {
    const response = await fetch('/api/maintenance/filter?count=10');
    const history = await response.json();
    
    const tbody = document.getElementById('filterHistoryBody');
    
    if (history.length === 0) {
      tbody.innerHTML = '<tr><td colspan="2">No maintenance records</td></tr>';
      return;
    }
    
    tbody.innerHTML = history.map(record => {
      const date = new Date(record.timestamp * 1000).toLocaleString();
      return `
        <tr>
          <td>${date}</td>
          <td>${record.notes || '(No notes)'}</td>
        </tr>
      `;
    }).join('');
    
  } catch (error) {
    console.error('Error loading filter history:', error);
    document.getElementById('filterHistoryBody').innerHTML = 
      '<tr><td colspan="2">Error loading history</td></tr>';
  }
}

// ===========================
// Initialization
// ===========================

// Call on page load
document.addEventListener('DOMContentLoaded', function() {
  updateWaterChangeStatus();
  loadWaterChangeHistory();
  loadFilterHistory();
  
  // Refresh status every 30 seconds when not actively changing water
  setInterval(() => {
    if (!updateInterval) {
      updateWaterChangeStatus();
    }
  }, 30000);
});
```

## Simple CSS Styling

```css
.card {
  background: #fff;
  border-radius: 8px;
  padding: 20px;
  margin-bottom: 20px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.status-display {
  display: flex;
  gap: 20px;
  margin-bottom: 15px;
  flex-wrap: wrap;
}

.status-item {
  display: flex;
  gap: 8px;
  align-items: center;
}

.badge {
  padding: 4px 12px;
  border-radius: 4px;
  font-size: 14px;
  font-weight: bold;
}

.badge-success {
  background: #4caf50;
  color: white;
}

.badge-warning {
  background: #ff9800;
  color: white;
}

.badge-danger {
  background: #f44336;
  color: white;
}

.readings-panel {
  background: #f5f5f5;
  padding: 15px;
  border-radius: 4px;
  margin: 15px 0;
}

.reading {
  margin: 5px 0;
  font-size: 16px;
}

.controls {
  margin: 15px 0;
}

.controls input[type="number"],
.controls input[type="text"] {
  padding: 8px;
  margin-right: 10px;
  border: 1px solid #ddd;
  border-radius: 4px;
  width: 200px;
}

button {
  padding: 10px 20px;
  border: none;
  border-radius: 4px;
  cursor: pointer;
  font-size: 14px;
  margin-right: 10px;
}

.btn-primary {
  background: #2196f3;
  color: white;
}

.btn-success {
  background: #4caf50;
  color: white;
}

.btn-danger {
  background: #f44336;
  color: white;
}

button:hover {
  opacity: 0.9;
}

.data-table {
  width: 100%;
  border-collapse: collapse;
  margin-top: 10px;
}

.data-table th {
  background: #f5f5f5;
  padding: 10px;
  text-align: left;
  border-bottom: 2px solid #ddd;
}

.data-table td {
  padding: 8px;
  border-bottom: 1px solid #eee;
}

.data-table tr:hover {
  background: #fafafa;
}

.info-panel {
  margin-top: 15px;
  padding-top: 15px;
  border-top: 1px solid #eee;
  color: #666;
  font-size: 14px;
}
```

## Testing Checklist

### Functional Tests
- [ ] Start water change with custom volume
- [ ] Start water change with scheduled volume (empty input)
- [ ] Complete water change
- [ ] Cancel water change
- [ ] Record filter maintenance with notes
- [ ] Record filter maintenance without notes
- [ ] History displays correctly
- [ ] "Days since" counters update
- [ ] Button states toggle correctly
- [ ] Initial readings show/hide properly

### UI/UX Tests
- [ ] Responsive layout on mobile
- [ ] Buttons disabled during API calls
- [ ] Loading indicators shown
- [ ] Error messages displayed
- [ ] Confirmation dialogs work
- [ ] Status badges color-coded correctly

### Error Handling
- [ ] Network timeout
- [ ] Server error (500)
- [ ] Invalid data
- [ ] Concurrent water changes (should prevent)

## Notes

- Status updates automatically during water change (5 second polling)
- Slower polling (30 seconds) when idle
- All timestamps in Unix epoch seconds
- Temperature deltas can be negative (cooling)
- pH and TDS deltas show change direction with +/- sign
- Filter maintenance doesn't interrupt water changes
- Both operations can be performed independently
