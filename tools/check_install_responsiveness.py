"""Hardware-only USB installation responsiveness check.

Installs/updates a temporary namespaced package, exercises cancellation/resume,
measures info latency while Lua runs, restores the prior app and removes the test
package. Requires pyserial and the Python dependencies of the pinned SDK tools.
The selected device is modified: save work and close Flare first. JSON contains
build identity and diagnostics, never raw Flash/NVS/bond keys.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
import tempfile
import time
import zlib
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='USB serial port, e.g. COM3 or /dev/ttyACM0')
    parser.add_argument('--output', required=True, help='new JSON result file')
    args = parser.parse_args()
    SDK = Path(__file__).resolve().parents[1] / 'platform'
    sys.path.insert(0, str(SDK / 'tools'))
    from companion import Companion, Reader, text, METHODS
    from compile_ignite import compile_project
    from build_package import build, TYPES
    temporary = tempfile.TemporaryDirectory(prefix='flint-install-check-')
    fixture = Path(temporary.name)
    (fixture / 'main.lua').write_text("local n=0; ignite.schedule(100); return {update=function() n=n+1; ignite.value('count',n); ignite.value('status','Running') end}", encoding='utf-8')
    project = json.loads((SDK / 'examples/counter/project.json').read_text(encoding='utf-8'))
    project.update(id='test.flint.installcheck', publisher='codex', name='Hardware test counter')
    small = compile_project(project, fixture)
    project['version'] = 2
    data = compile_project(project, fixture)
    names = {v: k for k, v in TYPES.items()}
    sections = []
    for i in range(struct.unpack_from('<I', data, 20)[0]):
        ident, kind, _, offset, size, _, _ = struct.unpack_from('<IHHIIII', data, 64 + i * 24)
        sections.append((ident, names[kind], data[offset:offset + size]))
    sections.append((10000, 'resource', bytes(range(256)) * 2800))
    bulk = build(sections)
    (fixture / 'counter-v1.ignit').write_bytes(small)
    (fixture / 'counter-v2.ignit').write_bytes(bulk)
    result = {}
    c = None
    previous = None
    installed = False
    out = Path(args.output)
    if out.exists():
        raise SystemExit('Choose a new --output file; refusing to replace an existing result')
    out.parent.mkdir(parents=True, exist_ok=True)

    def save():
        out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')

    def connect():
        c = Companion(args.port)
        c.serial.timeout = 0.01
        c.hello()
        assert c.capabilities & 896 == 896, "Firmware lacks Lua/Demand/session_close capabilities"
        return c

    def catalog():
        items = []
        while True:
            r = Reader(c.request('packages', struct.pack('<I', len(items))))
            total = r.number()
            count = r.number(2)
            for _ in range(count):
                item = {
                    'id': r.text(), 'name': r.text(), 'publisher': r.text(),
                    'version': r.number(), 'factory': r.number(1),
                }
                item['components'] = [
                    (r.text(), r.number(2), r.number(1)) for _ in range(r.number(2))
                ]
                items.append(item)
            assert r.offset == len(r.data) and len(items) <= total
            if len(items) == total:
                return items
            assert count, 'catalog pagination made no progress'

    def send(method, data=b''):
        c.sequence = c.sequence + 1 & 4294967295 or 1
        wire = struct.pack('<4sBBHII', b'FLCP', 1, 0, METHODS[method], c.sequence, len(data)) + data
        c.serial.write(wire + struct.pack('<I', zlib.crc32(wire)))
        return c.sequence

    def frames():
        c.buffer.extend(c.serial.read(4096))
        result = []
        while len(c.buffer) >= 20:
            assert c.buffer[:4] == b'FLCP', 'unexpected nonbinary bytes in application channel'
            _, version, reply, method, ident, size = struct.unpack_from('<4sBBHII', c.buffer)
            assert version == 1 and size <= 4096
            if len(c.buffer) < size + 20:
                break
            wire = bytes(c.buffer[:size + 20])
            del c.buffer[:size + 20]
            assert zlib.crc32(wire[:-4]) == int.from_bytes(wire[-4:], 'little')
            result.append((reply, method, ident, wire[16:-4]))
        return result

    def count():
        data = c.request('scene')
        match = re.search(b'Count: (\\d+)', data)
        assert match, data
        return int(match[1])
    try:
        c = connect()
        previous = c.info()
        result['before'] = c.diagnostics()
        assert previous['active'] and previous['component'], 'Launch an application before testing so it can be restored'
        assert not any((p['id'] == 'test.flint.installcheck' for p in catalog())), 'test namespace already exists'
        assert c.install(small) == 'test.flint.installcheck'
        installed = True
        c.launch('test.flint.installcheck')
        result['counter_before'] = count()
        c.subscribe()
        result['small_package_bytes'] = len(small)
        result['large_package_bytes'] = len(bulk)
        r = Reader(c.request('install_begin', struct.pack('<I', len(bulk)) + text(hashlib.sha256(bulk).hexdigest())))
        offset = r.number()
        t = time.monotonic()
        while offset < len(bulk):
            chunk = bulk[offset:offset + 4092]
            r = Reader(c.request('install_chunk', struct.pack('<I', offset) + chunk))
            offset = r.number()
        result['upload_seconds'] = time.monotonic() - t
        save()
        commit = send('install_commit')
        close = send('session_close')
        deadline = time.monotonic() + 10
        closed = False
        while time.monotonic() < deadline and (not closed):
            for reply, method, ident, payload in frames():
                if reply and ident == close:
                    assert method == METHODS['session_close'] and payload[:2] == b'\x00\x00'
                    closed = True
        assert closed
        c.serial.close()
        c = connect()
        assert c.info()['active'] == 'test.flint.installcheck'
        assert next((p for p in catalog() if p['id'] == 'test.flint.installcheck'))['version'] == 1
        result['cancel_before_publish'] = True
        r = Reader(c.request('install_begin', struct.pack('<I', len(bulk)) + text(hashlib.sha256(bulk).hexdigest())))
        assert r.number() == len(bulk)
        result['resume_full_upload'] = True
        start_count = count()
        commit = send('install_commit')
        info_id = send('info')
        scene_id = None
        deadline = time.monotonic() + 45
        t = time.monotonic()
        committed = False
        info_count = 0
        scene_counts = []
        events = 0
        info_sent = t
        latencies = []
        while time.monotonic() < deadline and (not committed):
            for reply, method, ident, payload in frames():
                if not reply:
                    events += 1
                    continue
                assert payload[:2] == b'\x00\x00', (method, int.from_bytes(payload[:2], 'little'))
                if ident == commit:
                    assert method == METHODS['install_commit']
                    committed = True
                elif ident == info_id:
                    latencies.append(time.monotonic() - info_sent)
                    info_count += 1
                    info_id = None
                    if not scene_id:
                        scene_id = send('scene')
                elif ident == scene_id:
                    match = re.search(b'Count: (\\d+)', payload)
                    if match:
                        scene_counts.append(int(match[1]))
                    scene_id = None
            if not committed and info_id is None:
                info_sent = time.monotonic()
                info_id = send('info')
        assert committed and info_count > 0 and scene_counts
        result['commit_seconds'] = time.monotonic() - t
        result['info_during_commit'] = info_count
        result['max_info_latency_seconds'] = max(latencies)
        result['info_latency_seconds'] = latencies
        result['counter_start_commit'] = start_count
        result['counter_values_during_commit'] = scene_counts
        result['lua_advanced_during_commit'] = max(scene_counts) > start_count
        assert result['lua_advanced_during_commit']
        settle = time.monotonic() + 2
        while (info_id or scene_id) and time.monotonic() < settle:
            for reply, method, ident, payload in frames():
                if ident == info_id:
                    info_id = None
                if ident == scene_id:
                    scene_id = None
        assert not info_id and (not scene_id)
        result['after'] = c.diagnostics()
        result['after_info'] = c.info()
        result['counter_after'] = count()
        assert next((p for p in catalog() if p['id'] == 'test.flint.installcheck'))['version'] == 2
        result['passed'] = True
        save()
    except Exception as e:
        result['error'] = repr(e)
        save()
        print('FAILED', repr(e))
        raise
    finally:
        cleanup_errors = []
        if c:
            try:
                if previous and previous.get('active'):
                    try:
                        c.launch(previous['active'], previous['component'])
                    except Exception as error:
                        cleanup_errors.append('restore: ' + repr(error))
                if installed:
                    try:
                        c.request('remove', text('test.flint.installcheck'))
                        result['test_package_removed'] = True
                    except Exception as error:
                        cleanup_errors.append('remove test.flint.installcheck: ' + repr(error))
                try:
                    result['cleanup'] = c.diagnostics()
                except Exception as error:
                    cleanup_errors.append('diagnostics: ' + repr(error))
                if cleanup_errors:
                    result['cleanup_errors'] = cleanup_errors
                    result['passed'] = False
                save()
            finally:
                c.close()
        temporary.cleanup()
        if cleanup_errors:
            raise RuntimeError('Hardware cleanup failed; inspect the JSON result: ' + str(out))
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
