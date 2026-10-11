import { existsSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

// Rain benchmark (PSP_BENCH_RAIN): phase 1, petals 0-2, configurations C0-C4.
// Must match kScenarios in src/psp/psp_debugui.cpp and kConfigLayers in
// src/psp/psp_rain.h. Offline only: read the reports after the run.
export const PETALS = [0, 1, 2];
export const CONFIG_LAYERS = [0, 1, 3, 7, 15];
const LAYER_NAMES = ['gotas', 'respingos', 'reflexo das luzes', 'poças'];

export function parseReport(text) {
  return Object.fromEntries(text.split(/\r?\n/)
    .filter(line => line.includes('='))
    .map(line => [line.slice(0, line.indexOf('=')), line.slice(line.indexOf('=') + 1)]));
}

export function layerNames(mask) {
  const names = LAYER_NAMES.filter((_, bit) => mask & (1 << bit));
  return names.length ? names.join(' + ') : 'sem chuva';
}

// Every report of one run, in scenario order, with the expected petal and layers.
export function validateReports(reports, expectedBuild) {
  if (!expectedBuild) throw new Error('Expected build stamp required');
  let run;
  let previousUptime = 0;
  let scenario = 0;
  for (const petal of PETALS) {
    for (let config = 0; config < CONFIG_LAYERS.length; ++config) {
      scenario += 1;
      const name = `P${petal} C${config}`;
      const values = reports[`P${petal}_C${config}`];
      if (!values) throw new Error(`${name}: missing report`);
      if (values.schema_version !== '2' || values.build_stamp !== expectedBuild
          || Number(values.scenario) !== scenario || values.level !== '1'
          || values.total_frames !== '600' || values.window_frames !== '600') {
        throw new Error(`${name}: incomplete, stale or wrong-scenario report`);
      }
      if (Number(values.petal) !== petal || Number(values.effect_layers) !== CONFIG_LAYERS[config]) {
        throw new Error(`${name}: petal/effect_layers do not match the scenario table`);
      }
      run ??= values.run_start_us;
      if (!/^\d+$/.test(values.run_start_us ?? '') || values.run_start_us !== run) {
        throw new Error(`${name}: missing run identity or mixed runs`);
      }
      const uptime = Number(values.report_uptime_us);
      if (!Number.isFinite(uptime) || uptime <= Math.max(previousUptime, Number(run))) {
        throw new Error(`${name}: invalid report order`);
      }
      previousUptime = uptime;
      for (const key of ['avg_fps', 'p95_frame_us', 'logic_avg_us', 'draw_avg_us', 'swap_avg_us',
        'ge_sync_avg_us', 'draw_calls_avg', 'submitted_vertices_avg', 'heap_free_min',
        'vram_used_max', 'skeleton_joints_avg', 'skinned_vertices_avg']) {
        // Number('') is 0: an empty value is as invalid as a missing one.
        if (!(key in values) || values[key].trim() === '' || !Number.isFinite(Number(values[key]))) {
          throw new Error(`${name}: missing/invalid ${key}`);
        }
      }
    }
  }
}

// 24-bit bottom-up BMP from WritePspLastFrame: share of pixels that differ.
export function bmpDifference(a, b) {
  const read = buffer => ({
    offset: buffer.readUInt32LE(10), width: buffer.readInt32LE(18),
    height: Math.abs(buffer.readInt32LE(22)), bpp: buffer.readUInt16LE(28),
  });
  const ha = read(a);
  const hb = read(b);
  if (ha.bpp !== 24 || hb.bpp !== 24 || ha.width !== hb.width || ha.height !== hb.height) {
    throw new Error('Captures must be 24-bit BMPs of the same size');
  }
  const stride = Math.ceil((ha.width * 3) / 4) * 4;
  let differing = 0;
  for (let y = 0; y < ha.height; ++y) {
    for (let x = 0; x < ha.width; ++x) {
      const ia = ha.offset + y * stride + x * 3;
      const ib = hb.offset + y * stride + x * 3;
      if (a[ia] !== b[ib] || a[ia + 1] !== b[ib + 1] || a[ia + 2] !== b[ib + 2]) differing += 1;
    }
  }
  return differing / (ha.width * ha.height);
}

const number = (value, digits = 0) => Number(value).toLocaleString('pt-BR',
  { minimumFractionDigits: digits, maximumFractionDigits: digits });
const signed = (value, digits = 0) => `${value >= 0 ? '+' : '−'}${number(Math.abs(value), digits)}`;

export function formatTable(reports, shots = {}, baselineShots = {}) {
  const lines = [];
  for (const petal of PETALS) {
    const base = reports[`P${petal}_C0`];
    const baseCpu = Number(base.logic_avg_us) + Number(base.draw_avg_us);
    lines.push(`\n## Petal ${petal}\n`);
    lines.push('| Config | Camadas | FPS | p95 (ms) | CPU (ms) | Δ CPU | Espera GE (ms) | Draws | Vértices | Heap livre mín. (KB) | VRAM (KB) | Captura ≠ C0 |');
    lines.push('|---|---|---|---|---|---|---|---|---|---|---|---|');
    for (let config = 0; config < CONFIG_LAYERS.length; ++config) {
      const values = reports[`P${petal}_C${config}`];
      const cpu = Number(values.logic_avg_us) + Number(values.draw_avg_us);
      const key = `P${petal}C${config}`;
      let shot = '—';
      if (config === 0 && shots[key] && baselineShots[key]) {
        shot = `${number(bmpDifference(shots[key], baselineShots[key]) * 100, 2)}% (vs. linha de base)`;
      }
      else if (config > 0 && shots[key] && shots[`P${petal}C0`]) {
        shot = `${number(bmpDifference(shots[key], shots[`P${petal}C0`]) * 100, 2)}%`;
      }
      lines.push(`| C${config} | ${layerNames(CONFIG_LAYERS[config])} | ${number(values.avg_fps, 2)} `
        + `| ${number(Number(values.p95_frame_us) / 1000, 1)} | ${number(cpu / 1000, 2)} `
        + `| ${config === 0 ? '—' : signed((cpu - baseCpu) / 1000, 2)} `
        + `| ${number(Number(values.ge_sync_avg_us) / 1000, 2)} | ${number(values.draw_calls_avg)} `
        + `| ${number(values.submitted_vertices_avg)} | ${number(Number(values.heap_free_min) / 1024)} `
        + `| ${number(Number(values.vram_used_max) / 1024)} | ${shot} |`);
    }
    // The rain must not change the game: same characters, joints and skinning.
    for (let config = 1; config < CONFIG_LAYERS.length; ++config) {
      const values = reports[`P${petal}_C${config}`];
      for (const key of ['skeleton_joints_avg', 'skinned_vertices_avg', 'ent_humanoid_draws_avg']) {
        if (key in base && values[key] !== base[key]) {
          lines.push(`\nAviso: P${petal} C${config} mudou ${key} (${base[key]} → ${values[key]}): a simulação não é a mesma da C0.`);
        }
      }
    }
  }
  return lines.join('\n');
}

export function loadRun(directory) {
  const reports = {};
  const shots = {};
  for (const petal of PETALS) {
    for (let config = 0; config < CONFIG_LAYERS.length; ++config) {
      const report = join(directory, `PSP_RAIN_P${petal}_C${config}.TXT`);
      if (existsSync(report)) reports[`P${petal}_C${config}`] = parseReport(readFileSync(report, 'utf8'));
      const shot = join(directory, `SHOT_P${petal}C${config}.BMP`);
      if (existsSync(shot)) shots[`P${petal}C${config}`] = readFileSync(shot);
    }
  }
  return { reports, shots };
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  const [, , directory, buildStamp, baselineDirectory] = process.argv;
  try {
    if (!directory || !buildStamp) {
      throw new Error('Usage: node scripts/psp_rain_bench_report.mjs DIRECTORY "build_stamp" [BASELINE_DIRECTORY]');
    }
    const { reports, shots } = loadRun(directory);
    validateReports(reports, buildStamp);
    const baselineShots = baselineDirectory ? loadRun(baselineDirectory).shots : {};
    console.log(`# Benchmark da chuva: ${buildStamp}`);
    console.log(formatTable(reports, shots, baselineShots));
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
