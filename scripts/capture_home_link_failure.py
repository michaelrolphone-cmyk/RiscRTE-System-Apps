#!/usr/bin/env python3
"""Diagnostic replay only: retain the first failed Home compiler invocation.

This never grants qualification. It uses the failed invocation's generated
inputs and wrapper, adding only verbose diagnostics and saved intermediates.
"""
import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import subprocess


def failed_compiler_command(log):
    for line in log.splitlines():
        match = re.search(r"CalledProcessError: Command '(\[.*\])' returned non-zero", line)
        if not match:
            continue
        command = ast.literal_eval(match.group(1))
        if (isinstance(command, list) and command and
                all(isinstance(value, str) for value in command) and
                Path(command[0]).name == 'xtensa-esp32s3-elf-gcc' and
                '-shared' in command and '-o' in command):
            return command
    raise ValueError('No failed shared Home compiler invocation in retained log')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qualification', type=Path, required=True)
    args = parser.parse_args()
    root = args.qualification.resolve()
    diagnostic = root / 'link-diagnostic'
    diagnostic.mkdir(exist_ok=True)
    receipt = {'diagnostic_only': True, 'qualified': False, 'state': 'prepared'}
    def save():
        (diagnostic / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    save()
    try:
        command = failed_compiler_command((root / 'no-ivopts/build.log').read_text())
        # Only reuse files from this run; reject a command pointing outside it.
        for value in (command[0], command[command.index('-o') + 1]):
            Path(value).resolve().relative_to(root)
        receipt['original_command'] = command
        original_output = Path(command[command.index('-o') + 1])
        output = diagnostic / 'default.elf'
        replay = command.copy()
        replay[replay.index('-o') + 1] = str(output)
        replay += ['-save-temps=obj', '-v']
        receipt.update(command=replay, state='running', original_output=str(original_output))
        save()
        with (diagnostic / 'replay.log').open('w') as log:
            result = subprocess.run(replay, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=300, check=False)
        receipt.update(returncode=result.returncode, state='replayed')
        # Includes preprocessed sources, assembly and actual driver-produced
        # objects in a separate output directory, archived by the workflow.
        receipt['intermediates'] = [
            {'path': str(path.relative_to(root)), 'bytes': path.stat().st_size,
             'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
            for path in sorted(output.parent.iterdir())
            if path.is_file() and path.suffix in ('.o', '.i', '.s', '.elf', '.map')
        ]
        save()
    except BaseException as error:
        receipt.update(state='diagnostic-error', error=type(error).__name__, message=str(error))
        save()
        raise


if __name__ == '__main__':
    main()
