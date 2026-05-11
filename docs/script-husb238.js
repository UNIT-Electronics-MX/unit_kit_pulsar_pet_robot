/**
 * HUSB238 USB-C PD Bluetooth Web Control
 * 
 * Control del módulo HUSB238 USB-C Power Delivery via Web Bluetooth API
 * Desarrollado para UNIT Electronics
 * 
 * Funcionalidades:
 * - Monitoreo del estado de conexión USB-C PD
 * - Selección de voltajes disponibles (5V, 9V, 12V, 15V, 18V, 20V)
 * - Lectura de corriente disponible por voltaje
 * - Sweep automático de todos los voltajes
 * - Log de actividad en tiempo real
 */

// ========================================
// CONFIGURACIÓN BLE - UUIDs para HUSB238
// ========================================
const SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const COMMAND_CHARACTERISTIC_UUID = '6d68efe5-04b6-4a85-abc4-c2670b7bf7fd'; // Envío de comandos (Write)
const RESPONSE_CHARACTERISTIC_UUID = 'f27b53ad-c63d-49a0-8c0f-9f297e6cc520'; // Respuestas (Read/Notify)

// Variables globales
let bluetoothDevice = null;
let bluetoothServer = null;
let bluetoothService = null;
let commandCharacteristic = null;
let responseCharacteristic = null;
let isConnected = false;

// Buffer para respuestas
let responseBuffer = '';
let pendingCommands = [];
let waitingForResponse = false;

// Voltajes disponibles
const VOLTAGES = [5, 9, 12, 15, 18, 20];
let availableVoltages = new Set();
let currentSelectedVoltage = null;

// Referencias DOM
const connectBtn = document.getElementById('connect-btn');
const disconnectBtn = document.getElementById('disconnect-btn');
const connectionStatus = document.getElementById('connection-status');
const statusIndicator = document.getElementById('status-indicator');
const attachmentStatus = document.getElementById('attachment-status');
const currentVoltage = document.getElementById('current-voltage');
const currentCurrent = document.getElementById('current-current');
const refreshStatusBtn = document.getElementById('refresh-status-btn');
const voltageGrid = document.getElementById('voltage-grid');
const scanVoltagesBtn = document.getElementById('scan-voltages-btn');
const sweepBtn = document.getElementById('sweep-btn');
const powerInfo = document.getElementById('power-info');
const activityLog = document.getElementById('activity-log');
const clearLogBtn = document.getElementById('clear-log-btn');

// ========================================
// FUNCIONES DE LOG
// ========================================
function log(message, type = 'info') {
  const timestamp = new Date().toLocaleTimeString();
  const color = {
    'info': 'text-gray-700',
    'success': 'text-green-600',
    'error': 'text-red-600',
    'warning': 'text-yellow-600',
    'command': 'text-blue-600'
  }[type] || 'text-gray-700';
  
  const logEntry = document.createElement('div');
  logEntry.className = color;
  logEntry.innerHTML = `<span class="text-gray-400">[${timestamp}]</span> ${message}`;
  activityLog.appendChild(logEntry);
  activityLog.scrollTop = activityLog.scrollHeight;
  
  console.log(`[${type.toUpperCase()}] ${message}`);
}

clearLogBtn.addEventListener('click', () => {
  activityLog.innerHTML = '';
  log('Registro limpiado', 'info');
});

// ========================================
// FUNCIONES DE CONEXIÓN BLE
// ========================================
async function connectBluetooth() {
  try {
    log('🔍 Buscando dispositivos HUSB238...', 'info');
    updateConnectionStatus('connecting');

    // Solicitar dispositivo
    bluetoothDevice = await navigator.bluetooth.requestDevice({
      filters: [{ 
        name: 'HUSB238',
        services: [SERVICE_UUID]
      }]
    });

    log(`✅ Dispositivo encontrado: ${bluetoothDevice.name}`, 'success');

    // Conectar al servidor GATT
    bluetoothServer = await bluetoothDevice.gatt.connect();
    log('🔗 Conectado al servidor GATT', 'success');

    // Obtener servicio
    bluetoothService = await bluetoothServer.getPrimaryService(SERVICE_UUID);
    log('📡 Servicio obtenido', 'success');

    // Obtener características
    commandCharacteristic = await bluetoothService.getCharacteristic(COMMAND_CHARACTERISTIC_UUID);
    responseCharacteristic = await bluetoothService.getCharacteristic(RESPONSE_CHARACTERISTIC_UUID);
    log('✅ Características obtenidas', 'success');

    // Habilitar notificaciones para respuestas
    await responseCharacteristic.startNotifications();
    responseCharacteristic.addEventListener('characteristicvaluechanged', handleResponseNotification);
    log('🔔 Notificaciones habilitadas', 'success');

    // Escuchar desconexión
    bluetoothDevice.addEventListener('gattserverdisconnected', onDisconnected);

    isConnected = true;
    updateConnectionStatus('connected');
    log('✅ Conexión establecida completamente', 'success');

    // Obtener información del dispositivo
    await getDeviceInfo();
    await refreshStatus();

  } catch (error) {
    log(`❌ Error de conexión: ${error}`, 'error');
    updateConnectionStatus('disconnected');
    isConnected = false;
  }
}

function onDisconnected() {
  log('⚠️ Dispositivo desconectado', 'warning');
  isConnected = false;
  updateConnectionStatus('disconnected');
  resetUI();
}

async function disconnect() {
  if (bluetoothDevice && bluetoothDevice.gatt.connected) {
    bluetoothDevice.gatt.disconnect();
    log('🔌 Desconectado manualmente', 'info');
  }
  isConnected = false;
  updateConnectionStatus('disconnected');
  resetUI();
}

function updateConnectionStatus(status) {
  const statusConfig = {
    'connected': {
      text: 'Conectado',
      class: 'text-green-600',
      indicator: 'status-connected'
    },
    'connecting': {
      text: 'Conectando...',
      class: 'text-yellow-600',
      indicator: 'status-connecting'
    },
    'disconnected': {
      text: 'Desconectado',
      class: 'text-red-600',
      indicator: 'status-disconnected'
    }
  };

  const config = statusConfig[status];
  connectionStatus.textContent = config.text;
  connectionStatus.className = `text-sm font-medium ${config.class}`;
  statusIndicator.className = `status-indicator ${config.indicator}`;

  if (status === 'connected') {
    connectBtn.classList.add('hidden');
    disconnectBtn.classList.remove('hidden');
  } else {
    connectBtn.classList.remove('hidden');
    disconnectBtn.classList.add('hidden');
  }
}

function resetUI() {
  attachmentStatus.textContent = '---';
  attachmentStatus.className = 'text-xl font-bold text-gray-400';
  currentVoltage.textContent = '---';
  currentCurrent.textContent = '---';
  availableVoltages.clear();
  renderVoltageCards();
  powerInfo.textContent = 'Desconectado';
}

// ========================================
// COMUNICACIÓN CON EL DISPOSITIVO
// ========================================
function handleResponseNotification(event) {
  const value = event.target.value;
  const decoder = new TextDecoder('utf-8');
  const chunk = decoder.decode(value);
  
  responseBuffer += chunk;
  
  // Procesar líneas completas
  const lines = responseBuffer.split('\n');
  responseBuffer = lines.pop() || ''; // Guardar línea incompleta
  
  for (const line of lines) {
    if (line.trim()) {
      log(`📥 ${line.trim()}`, 'info');
      processResponse(line.trim());
    }
  }
}

function processResponse(response) {
  // Procesar respuesta según el tipo
  if (response.startsWith('UNIT-DEVLAB')) {
    log(`📟 Dispositivo identificado: ${response}`, 'success');
  } else if (response === 'ATTACHED' || response === 'UNATTACHED') {
    updateAttachmentStatus(response === 'ATTACHED');
  } else if (response.includes('V ')) {
    // Lista de voltajes disponibles
    parseAvailableVoltages(response);
  } else if (response.startsWith('PD=')) {
    const voltage = response.substring(3);
    updateCurrentVoltage(voltage);
  } else if (response.startsWith('CURR=')) {
    const current = response.substring(5);
    updateCurrentCurrent(current);
  } else if (response.startsWith('MAX_CURR@')) {
    parsePowerInfo(response);
  } else if (response.startsWith('OK:SET')) {
    log(`✅ ${response}`, 'success');
  } else if (response.startsWith('ERR:')) {
    log(`❌ Error: ${response}`, 'error');
  } else if (response.startsWith('SWEEP')) {
    log(`⚡ ${response}`, 'info');
  }
  
  waitingForResponse = false;
  processNextCommand();
}

async function sendCommand(command) {
  if (!isConnected || !commandCharacteristic) {
    log('❌ No conectado', 'error');
    return;
  }

  try {
    log(`📤 CMD: ${command}`, 'command');
    const encoder = new TextEncoder();
    const data = encoder.encode(command + '\n');
    await commandCharacteristic.writeValue(data);
  } catch (error) {
    log(`❌ Error enviando comando: ${error}`, 'error');
  }
}

function queueCommand(command, delay = 0) {
  pendingCommands.push({ command, delay });
  if (!waitingForResponse) {
    processNextCommand();
  }
}

async function processNextCommand() {
  if (pendingCommands.length === 0) return;
  
  waitingForResponse = true;
  const { command, delay } = pendingCommands.shift();
  
  if (delay > 0) {
    await new Promise(resolve => setTimeout(resolve, delay));
  }
  
  await sendCommand(command);
}

// ========================================
// COMANDOS ESPECÍFICOS
// ========================================
async function getDeviceInfo() {
  queueCommand('*IDN?');
}

async function refreshStatus() {
  queueCommand('STAT?', 100);
  queueCommand('PD:GET?', 100);
  queueCommand('CURR:GET?', 100);
}

async function scanAvailableVoltages() {
  log('🔍 Escaneando voltajes disponibles...', 'info');
  availableVoltages.clear();
  queueCommand('PD:LIST?');
}

async function setVoltage(voltage) {
  log(`🔧 Estableciendo voltaje a ${voltage}V...`, 'info');
  currentSelectedVoltage = voltage;
  queueCommand(`PD:SET${voltage}`);
  
  // Actualizar estado después de un momento
  setTimeout(() => {
    refreshStatus();
  }, 500);
}

async function sweepVoltages() {
  log('⚡ Iniciando sweep de voltajes...', 'warning');
  queueCommand('PD:SWEEP');
}

async function getMaxCurrentForVoltage(voltage) {
  queueCommand(`CURR:MAX?${voltage}`);
}

// ========================================
// ACTUALIZACIÓN DE UI
// ========================================
function updateAttachmentStatus(attached) {
  attachmentStatus.textContent = attached ? 'CONECTADO' : 'DESCONECTADO';
  attachmentStatus.className = attached 
    ? 'text-xl font-bold text-green-600' 
    : 'text-xl font-bold text-red-600';
}

function updateCurrentVoltage(voltage) {
  currentVoltage.textContent = voltage;
}

function updateCurrentCurrent(current) {
  currentCurrent.textContent = current;
}

function parseAvailableVoltages(response) {
  availableVoltages.clear();
  const voltages = response.match(/(\d+)V/g);
  if (voltages) {
    voltages.forEach(v => {
      const voltage = parseInt(v);
      availableVoltages.add(voltage);
    });
    log(`✅ Voltajes disponibles: ${Array.from(availableVoltages).join('V, ')}V`, 'success');
    renderVoltageCards();
  }
}

function parsePowerInfo(response) {
  // MAX_CURR@12V=3.0A
  powerInfo.textContent = response.replace('MAX_CURR@', 'Corriente máxima @ ').replace('=', ': ');
}

function renderVoltageCards() {
  voltageGrid.innerHTML = '';
  
  VOLTAGES.forEach(voltage => {
    const card = document.createElement('div');
    const isAvailable = availableVoltages.has(voltage);
    const isSelected = currentSelectedVoltage === voltage;
    
    let classes = 'voltage-card bg-white p-4 rounded-lg shadow text-center ';
    if (isSelected) {
      classes += 'selected ';
    } else if (isAvailable) {
      classes += 'available ';
    } else {
      classes += 'unavailable ';
    }
    
    card.className = classes;
    card.innerHTML = `
      <div class="text-3xl font-bold ${isAvailable ? 'text-blue-600' : 'text-gray-400'}">${voltage}V</div>
      <div class="text-xs ${isAvailable ? 'text-green-600' : 'text-gray-400'} mt-1">
        ${isAvailable ? '✅ Disponible' : '❌ No disponible'}
      </div>
    `;
    
    if (isAvailable) {
      card.addEventListener('click', () => {
        setVoltage(voltage);
        getMaxCurrentForVoltage(voltage);
      });
    }
    
    voltageGrid.appendChild(card);
  });
}

// ========================================
// EVENT LISTENERS
// ========================================
connectBtn.addEventListener('click', connectBluetooth);
disconnectBtn.addEventListener('click', disconnect);
refreshStatusBtn.addEventListener('click', refreshStatus);
scanVoltagesBtn.addEventListener('click', scanAvailableVoltages);
sweepBtn.addEventListener('click', sweepVoltages);

// ========================================
// INICIALIZACIÓN
// ========================================
document.addEventListener('DOMContentLoaded', () => {
  log('🚀 Aplicación HUSB238 Web Bluetooth iniciada', 'success');
  log('ℹ️ Haz clic en "Conectar BLE" para empezar', 'info');
  renderVoltageCards();
  
  // Verificar compatibilidad con Web Bluetooth
  if (!navigator.bluetooth) {
    log('❌ Web Bluetooth no está soportado en este navegador', 'error');
    log('ℹ️ Usa Chrome, Edge o un navegador compatible', 'warning');
    connectBtn.disabled = true;
  }
});
