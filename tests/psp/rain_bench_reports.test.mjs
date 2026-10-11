import assert from 'node:assert/strict';
import test from 'node:test';
import {
  CONFIG_LAYERS, PETALS, bmpDifference, formatTable, parseReport, validateReports,
} from '../../scripts/psp_rain_bench_report.mjs';

const build = 'Oct 11 2026 19:30:00';
const run = () => {
  const reports = {};
  let scenario = 0;
  for (const petal of PETALS) {
    CONFIG_LAYERS.forEach((layers, config) => {
      scenario += 1;
      reports[`P${petal}_C${config}`] = parseReport(`schema_version=2
build_stamp=${build}
run_start_us=100
report_uptime_us=${1000 + scenario}
scenario=${scenario}
level=1
petal=${petal}
effect_layers=${layers}
total_frames=600
window_frames=600
avg_fps=59.5
p95_frame_us=17000
logic_avg_us=4200
draw_avg_us=${8700 + config * 100}
swap_avg_us=3700
ge_sync_avg_us=1900
draw_calls_avg=155
submitted_vertices_avg=10155
heap_free_min=507904
vram_used_max=1572864
skeleton_joints_avg=92.500
skinned_vertices_avg=1231.000
ent_humanoid_draws_avg=1.000
`);
    });
  }
  return reports;
};

// 2x2 24-bit bottom-up BMP (rows padded to 8 bytes).
const bmp = (pixel) => {
  const buffer = Buffer.alloc(54 + 16);
  buffer.write('BM', 0);
  buffer.writeUInt32LE(54, 10);
  buffer.writeUInt32LE(40, 14);
  buffer.writeInt32LE(2, 18);
  buffer.writeInt32LE(2, 22);
  buffer.writeUInt16LE(24, 28);
  buffer[54] = pixel;
  return buffer;
};

test('accepts the 15 rain scenarios from one identified build/run', () => {
  assert.doesNotThrow(() => validateReports(run(), build));
});

test('rejects old builds, missing scenarios, wrong layers and mixed runs', () => {
  assert.throws(() => validateReports(run(), 'old'));
  const missing = run();
  delete missing.P2_C4;
  assert.throws(() => validateReports(missing, build));
  for (const [key, field, value] of [
    ['P1_C3', 'effect_layers', '3'], ['P1_C3', 'petal', '0'], ['P0_C2', 'run_start_us', '101'],
    ['P2_C0', 'scenario', '1'], ['P0_C1', 'ge_sync_avg_us', ''], ['P2_C1', 'report_uptime_us', '99'],
  ]) {
    const reports = run();
    reports[key][field] = value;
    assert.throws(() => validateReports(reports, build), `${key}.${field}=${value}`);
  }
});

test('compares captures and flags a simulation that differs from C0', () => {
  assert.equal(bmpDifference(bmp(0), bmp(0)), 0);
  assert.equal(bmpDifference(bmp(0), bmp(9)), 0.25);
  const reports = run();
  assert.doesNotMatch(formatTable(reports), /Aviso/);
  reports.P1_C2.skeleton_joints_avg = '120.000';
  assert.match(formatTable(reports), /Aviso: P1 C2 mudou skeleton_joints_avg/);
});
