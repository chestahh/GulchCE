"""Generate a Windows adapter for the existing browser game-admission oracle.

Uses a privately extracted Mosquitto on loopback. Production game/browser code
is unchanged. Private evidence is retained for review/cleanup. Provide
M0_CHROMIUM and NODE_PATH; extract Mosquitto in build/m0/internet/mosquitto.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / 'build/m0/internet'

NATIVE = r'''
let nativeNumber = 0;
function native(name, test, extra = {}) {
  const log = side(name);
  const folder = path.join(work, `${name}-${++nativeNumber}`);
  mkdirSync(folder, {recursive: true});
  copyFileSync(halo, path.join(folder, 'halo.exe'));
  copyFileSync(path.join(path.dirname(halo), 'SDL3.dll'), path.join(folder, 'SDL3.dll'));
  writeFileSync(path.join(folder, 'config.toml'), '[display]\nfullscreen=false\n');
  const inherited = Object.fromEntries(Object.entries(process.env).filter(([k]) => !k.startsWith('HALO_')));
  const env = {...inherited, HALO_DATA_ROOT: path.dirname(maps), HALO_SAVE_ROOT: path.join(folder, 'saves'),
    HALO_LOG_ROOT: folder, HALO_NETWORK_TEST: test, HALO_NET_BROKERS_FILE: path.join(work, 'brokers.txt'),
    HALO_NULL_RENDERER: '1', HALO_HIDDEN_WINDOW: '1', SDL_VIDEO_DRIVER: 'dummy', SDL_AUDIO_DRIVER: 'dummy',
    HALO_NO_AUDIO: '1', HALO_NET_ONLINE: '1', HALO_NET_ALLOW_UPNP: '0', HALO_DISCORD_APPLICATION: '',
    HALO_NET_JOIN_FROM_CLIPBOARD: '0', HALO_NET_ADDRESS: '127.0.0.240', HALO_NET_BROADCAST: '127.0.0.241',
    HALO_NET_LISTINGS: 'brokers', HALO_NET_MAP_SHARE: '0', HALO_NET_MAP_DOWNLOAD: '0', HALO_NET_MAP_SEED: '0',
    HALO_SESSION_COMPATIBILITY: 'opence_crossplay', HALO_UPDATE_AUTO: '0', HALO_EXIT_AFTER: '240',
    ...extra};
  const child = spawn(path.join(folder, 'halo.exe'), test === 'server' ? ['--server'] : [],
    {cwd: folder, env, windowsHide: true});
  let offset = 0;
  const collect = () => {
    const filename = path.join(folder, 'halo.log');
    if (existsSync(filename)) {
      const text = readFileSync(filename, 'utf8');
      log.add(text.slice(offset));
      offset = text.length;
    }
  };
  const timer = setInterval(collect, 250);
  child.stdout.on('data', data => log.add(data));
  child.stderr.on('data', data => log.add(data));
  log.stop = async () => {
    clearInterval(timer);
    collect();
    if (child.exitCode === null && child.signalCode === null) {
      const stopped = new Promise(resolve => child.once('exit', resolve));
      child.kill();
      await stopped;
    }
  };
  running.push(log);
  return log;
}
function dedicated(name) {
  const log = native(name, 'server', {HALO_SERVER_LIST_ON_BROKERS: '1', HALO_SERVER_MASTER_PUBLISH: '0',
    HALO_SERVER_WEB_PORT: '0', HALO_SERVER_LOBBY_SECONDS: '3', HALO_SERVER_ROTATION: 'bloodgulch:slayer',
    HALO_SERVER_NAME: 'M0 Local Web Test', HALO_NET_TUNNEL_PORT: String(tcpPort + 2000)});
  log.ready = () => log.lines.some(line => /server: ready/.test(line));
  log.joined = () => log.lines.some(line => /player: .* joined/.test(line)) &&
    log.lines.some(line => /game: started/.test(line));
  return log;
}
'''


def once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'Unexpected source anchor: {old[:90]}')
    return text.replace(old, new)


def main():
    arguments = sys.argv[1:]
    cooperative = '--coop' in arguments
    generate_only = '--generate-only' in arguments
    arguments = [argument for argument in arguments if argument not in ('--coop', '--generate-only')]
    WORK.mkdir(parents=True, exist_ok=True)
    original = ROOT / 'port/web/tests/internet.mjs'
    script = original.read_text()
    script = once(script, 'copyFileSync, existsSync, mkdirSync', 'appendFileSync, copyFileSync, existsSync, readFileSync, mkdirSync')
    script = once(script, "const root = path.resolve(here, '../../..');", 'const root = ' + json.dumps(ROOT.as_posix()) + ';')
    script = once(script, "const work = mkdtempSync(path.join(tmpdir(), 'opence-internet-'));",
                  'const work = mkdtempSync(path.join(' + json.dumps(WORK.as_posix()) + ", 'run-'));\nconsole.log('Evidence: ' + work);")
    script = once(script, "console.log('Evidence: ' + work);",
                  "console.log('Evidence: ' + work);\ncopyFileSync(" + json.dumps(str(WORK / 'manifest.json')) +
                  ", path.join(work, 'adapter-manifest.json'));")
    script = once(script, "const broker = spawn('mosquitto', ['-c', path.join(work, 'mosquitto.conf')], { stdio: 'ignore' });",
                  'const broker = spawn(' + json.dumps(str(WORK / 'mosquitto/mosquitto.exe')) +
                  ", ['-c', path.join(work, 'mosquitto.conf')], {windowsHide: true});\n" +
                  "broker.stderr.on('data', d => appendFileSync(path.join(work, 'broker.log'), d));")
    script = once(script, "spawn('python3',", 'spawn(' + json.dumps(sys.executable) + ',')
    script = once(script, "headless: true, viewport:", "executablePath: process.env.M0_CHROMIUM, headless: true, viewport:")
    script = once(script, '  const page = context.pages()[0] || await context.newPage();',
                  '  try {\n  const page = context.pages()[0] || await context.newPage();')
    script = once(script, '  return { context, page, log };\n}',
                  '  return { context, page, log };\n  } catch (error) {\n' +
                  '    await context.close();\n    throw error;\n  }\n}')
    script = once(script, 'set=HALO_NET_JOIN_FROM_CLIPBOARD=0&brokers=',
                  'set=HALO_NET_JOIN_FROM_CLIPBOARD=0&set=HALO_NET_LISTINGS=brokers&set=HALO_NET_MAP_SHARE=0&set=HALO_NET_MAP_DOWNLOAD=0&brokers=')
    script = once(script, 'for (let waited = 0; waited < seconds && !test(); waited++) await sleep(1000);',
                  "for (let waited = 0; waited < seconds && !test(); waited++) {\n" +
                  "    if (existsSync(path.join(work, 'stop'))) throw Error('M0 fixture stopped for review');\n" +
                  "    await sleep(1000);\n  }")
    script = once(script, '      result.lines.push(line);',
                  "      result.lines.push(line);\n      appendFileSync(path.join(work, name + '-all.log'), line + '\\n');")
    # Keep page HTTP requests local; broker-only listing is configured above.
    script = once(script, '  await page.goto(pageUrl);', '''  await page.route('**/*', route => {
    const u = new URL(route.request().url());
    return ['localhost', '127.0.0.1'].includes(u.hostname) ? route.continue() : route.abort();
  });
  await page.goto(pageUrl);''')
    first = script.index('function native(')
    last = script.index('async function until(', first)
    script = script[:first] + NATIVE + '\n' + script[last:]
    script = script.replace('  host.stop();', '  await host.stop();').replace('    joiner.stop();', '    await joiner.stop();')
    script = once(script, 'for (const side of running) side.stop();', 'for (const side of running) await side.stop();')
    script = once(script, '  const second = await browser(\'second\');\n  opened.push(first, second);',
                  "  opened.push(first);\n  const second = await browser('second');\n  opened.push(second);")
    script = once(script, '  for (const game of opened) await game.context.close().catch(() => {});', '''  try { for (const game of opened) {
    await game.page.goto(pageUrl);
    const saves = await game.page.evaluate(async () => {
      const output = [];
      async function collect(dir, prefix) {
        for await (const [name, handle] of dir.entries()) {
          if (/^cache\\d+\\.map$/i.test(name) || (prefix === 'save/z/' && name === 'maps')) continue;
          if (handle.kind === 'directory') await collect(handle, prefix + name + '/');
          else {
            const bytes = new Uint8Array(await (await handle.getFile()).arrayBuffer());
            let binary = '';
            for (let i = 0; i < bytes.length; i += 16384) binary += String.fromCharCode(...bytes.subarray(i, i + 16384));
            output.push({name: prefix + name, base64: btoa(binary)});
          }
        }
      }
      const root = await navigator.storage.getDirectory();
      try { await collect(await root.getDirectoryHandle('save'), 'save/'); }
      catch (e) { if (e.name !== 'NotFoundError') throw e; }
      return output;
    });
    const base = path.join(work, game.log.name + '-saves');
    for (const save of saves) {
      const file = path.resolve(base, save.name);
      if (!file.startsWith(base + path.sep)) throw Error('Save escaped the private fixture');
      mkdirSync(path.dirname(file), {recursive: true});
      writeFileSync(file, Buffer.from(save.base64, 'base64'));
    }
    await game.context.close();
  } } finally {
    for (const game of opened) await game.context.close().catch(() => {});
    server.kill();
    broker.kill();
  }''')
    script = once(script, '  rmSync(work, { recursive: true, force: true });',
                  "  for (const game of opened) writeFileSync(path.join(work, game.log.name + '.log'), game.log.lines.join('\\n'));\n" +
                  "  console.log('Retained private logs, saves and browser profiles: ' + work);")
    if cooperative:
        script = once(script, "native('native', 'host:bloodgulch',",
                      "native('native', 'host:levels\\\\a30\\\\a30:coop',")
        script = script.replace('set=HALO_NETWORK_TEST=host:bloodgulch',
                                'set=HALO_NETWORK_TEST=host:levels%5Ca30%5Ca30:coop')
        first = script.index("  console.log('the native build as a dedicated server,")
        last = script.index('  if (!process.exitCode)', first)
        script = script[:first] + script[last:]
    target = WORK / 'internet_windows.mjs'
    target.write_text(script)
    (WORK / 'manifest.json').write_text(json.dumps({
        'mode': 'A30 co-op admission (three peer arrangements)' if cooperative else 'original four multiplayer admission cases',
        'source': str(original.relative_to(ROOT)), 'source_sha256': hashlib.sha256(original.read_bytes()).hexdigest(),
        'adapter_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
        'broker': 'Official Mosquitto 2.1.2, extracted only; localhost TCP and WebSocket listeners',
        'broker_package_url': 'https://mosquitto.org/files/binary/win64/mosquitto-2.1.2-install-windows-x64.exe',
        'broker_package_sha256': '58008ad7a22ada0b4073afa415801746e027c5f583e4fa52d0f4e9193b98d6aa'}, indent=2))
    if not generate_only:
        subprocess.run(['node', str(target), *arguments], cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
