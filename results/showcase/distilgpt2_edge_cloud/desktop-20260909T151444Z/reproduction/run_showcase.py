import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

repo = Path('/home/anonymous/Projects/infra')
setup = Path((repo / 'tmp/latest-showcase-setup.txt').read_text().strip())
run_id = 'desktop-' + datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
run = repo / 'results/showcase/distilgpt2_edge_cloud' / run_id
latest = repo / 'results/showcase/latest.json'
if latest.exists():
    shutil.copyfile(latest, setup / ('latest-before-' + run_id + '.json'))
p = setup / 'build-llama.sh'
p.write_text(p.read_text().replace('llama-bench --version', 'llama-bench --help'))
cmd = ['python3', 'scripts/run_edge_cloud_showcase.py', '--config',
       str(setup / 'showcase.config.json'), '--run-id', run_id, '--execute']
receipt = {'command': cmd, 'run_directory': str(run)}
receipt_path = setup / (run_id + '-execution.json')
receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
code = 1
try:
    with (setup / 'logs' / (run_id + '.log')).open('w') as log:
        process = subprocess.Popen(cmd, cwd=repo, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, text=True, bufsize=1)
        for line in process.stdout:
            print(line, end='', flush=True)
            log.write(line)
            log.flush()
        code = process.wait()
finally:
    containers = subprocess.run(['docker', 'ps', '-aq', '--filter',
                                 'label=com.inferlab.setup=' + setup.name],
                                capture_output=True, text=True, check=True).stdout.split()
    receipt['leftover_containers_before_cleanup'] = containers
    for name in containers:
        subprocess.run(['docker', 'rm', '-f', name], check=True)
    receipt['exit_code'] = code
    if (run / 'run_manifest.json').exists():
        manifest = json.loads((run / 'run_manifest.json').read_text())
        receipt['manifest_status'] = manifest.get('status')
        receipt['all_stages_passed'] = all(s.get('status') == 'passed' for s in manifest['stages'])
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    (setup / 'latest-attempt.txt').write_text(str(run) + '\n')
print(json.dumps(receipt, indent=2), flush=True)
raise SystemExit(code)
