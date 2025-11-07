import { SerialPort } from 'serialport';
import { ReadlineParser } from '@serialport/parser-readline';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const START_MARKER = '===== Payload recibido =====';
const END_MARKER = '============================';
const DEFAULT_BAUD_RATE = 115200;
const DEFAULT_OUTPUT = path.join(__dirname, '..', 'lora_payload_sample.json');
const ARDUINO_VID = new Set(['2341', '2a03']);
const COMMON_USB_SERIAL_VID = new Set(['1a86', '10c4']);
const KEYWORDS = ['arduino', 'nano', 'ch340', 'wch', 'cp210', 'silicon labs', 'usb-serial'];

const normalizeHex = (value) => {
  if (!value) return null;
  const normalized = value.toString().replace(/^0x/i, '').toLowerCase();
  return normalized.length ? normalized : null;
};

const enrichPort = (port) => {
  const pathName = port.path || port.device || port.comName;
  const manufacturer = port.manufacturer || port.pnpId || port.friendlyName || '';
  const description = port.friendlyName || port.pnpId || '';
  const vendorId = normalizeHex(port.vendorId || port.vendorID);
  const productId = normalizeHex(port.productId || port.productID);

  const lowerMeta = `${manufacturer} ${description}`.toLowerCase();
  const keywordHit = KEYWORDS.some((keyword) => lowerMeta.includes(keyword));
  const vidHit = Boolean(vendorId && (ARDUINO_VID.has(vendorId) || COMMON_USB_SERIAL_VID.has(vendorId)));
  const deviceHit =
    typeof pathName === 'string' &&
    ['ttyusb', 'ttyacm', 'usbserial', 'wchusb', 'com'].some((slug) => pathName.toLowerCase().includes(slug));

  return {
    path: pathName,
    manufacturer: port.manufacturer ?? null,
    serialNumber: port.serialNumber ?? null,
    locationId: port.locationId ?? null,
    vendorId,
    productId,
    friendlyName: port.friendlyName ?? null,
    pnpId: port.pnpId ?? null,
    preferred: Boolean(keywordHit || vidHit || deviceHit),
  };
};

export class SerialManager extends EventEmitter {
  constructor({ outputPath = DEFAULT_OUTPUT, baudRate = DEFAULT_BAUD_RATE } = {}) {
    super();
    this.outputPath = outputPath;
    this.baudRate = baudRate;
    this.port = null;
    this.parser = null;
    this.capturing = false;
    this.topic = null;
    this.boundHandleLine = (line) => {
      try {
        this.handleLine(line);
      } catch (error) {
        this.emit('error', error);
      }
    };
  }

  async listPorts() {
    const ports = await SerialPort.list();
    const enriched = ports.map(enrichPort).filter((port) => Boolean(port.path));
    enriched.sort((a, b) => {
      if (a.preferred === b.preferred) {
        return (a.path || '').localeCompare(b.path || '');
      }
      return a.preferred ? -1 : 1;
    });
    const auto = this.selectAutoPort(enriched);
    return { ports: enriched, auto };
  }

  selectAutoPort(entries) {
    if (!entries?.length) {
      return null;
    }
    const preferred = entries.find((port) => port.preferred);
    return (preferred || entries[0]).path;
  }

  async connect(pathName) {
    if (!pathName) {
      throw new Error('Puerto serial inválido');
    }
    await this.disconnect();

    return new Promise((resolve, reject) => {
      const port = new SerialPort({ path: pathName, baudRate: this.baudRate, autoOpen: false });
      const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

      const onError = (error) => {
        port.off('error', onError);
        parser.off('data', this.boundHandleLine);
        reject(error);
      };

      port.once('error', onError);
      parser.on('data', this.boundHandleLine);

      port.open((error) => {
        if (error) {
          parser.off('data', this.boundHandleLine);
          reject(error);
          return;
        }

        port.off('error', onError);
        this.port = port;
        this.parser = parser;
        port.on('close', () => {
          this.port = null;
          this.parser = null;
          this.emit('stopped');
        });
        port.on('error', (err) => {
          this.emit('error', err);
        });

        resolve();
      });
    });
  }

  async disconnect() {
    if (!this.port) {
      return;
    }

    const port = this.port;
    const parser = this.parser;
    this.port = null;
    this.parser = null;

    if (parser) {
      parser.off('data', this.boundHandleLine);
    }

    await new Promise((resolve) => {
      port.close(() => resolve());
    });
  }

  handleLine(raw) {
    const rawText = typeof raw === 'string' ? raw : raw?.toString?.();
    if (!rawText) {
      return;
    }

    const text = rawText.replace(/[\r\n]+$/, '');
    const trimmed = text.trim();
    this.emit('line', {
      text,
      trimmed,
      raw: rawText,
      timestamp: new Date().toISOString(),
    });

    const line = trimmed;
    if (!line) {
      return;
    }

    if (line.startsWith(START_MARKER)) {
      this.capturing = true;
      this.topic = null;
      return;
    }

    if (!this.capturing) {
      return;
    }

    if (line.startsWith('Topic:')) {
      this.topic = line.split(':', 1)[1]?.trim() || null;
      return;
    }

    if (line.startsWith('{') || line.startsWith('[')) {
      this.handlePayload(line).catch((error) => this.emit('error', error));
      return;
    }

    if (line.startsWith(END_MARKER)) {
      this.capturing = false;
      this.topic = null;
    }
  }

  async handlePayload(text) {
    try {
      const payload = JSON.parse(text);
      if (payload && typeof payload === 'object') {
        const meta = (payload._meta = payload._meta && typeof payload._meta === 'object' ? payload._meta : {});
        meta.topic = this.topic || meta.topic || 'sensors';
        meta.received_at = new Date().toISOString();
      }
      await fs.writeFile(this.outputPath, JSON.stringify(payload, null, 2), 'utf-8');
      this.emit('payload', payload);
    } catch (error) {
      this.emit('error', new Error(`No se pudo parsear el payload JSON: ${error.message}`));
    }
  }
}
