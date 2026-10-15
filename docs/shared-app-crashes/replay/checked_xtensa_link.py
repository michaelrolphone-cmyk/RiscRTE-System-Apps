"""Bounded handling of one pinned Xtensa GCC linker abort signature."""
import hashlib
import json
from pathlib import Path
import subprocess


def recognized_linker_abort(output):
    """Reject partial signatures and any additional unexplained diagnostics."""
    lines = [line.strip() for line in output.decode('utf-8', errors='replace').splitlines() if line.strip()]
    expected = [
        'collect2: fatal error: ld terminated with signal 6 [Aborted]',
        'compilation terminated.',
        'double free or corruption (out)',
    ]
    if expected[0] + ', core dumped' in lines:
        lines[lines.index(expected[0] + ', core dumped')] = expected[0]
    return sorted(lines) == sorted(expected)


def compile_checked(command, target, log, max_attempts=6):
    """Retry only that exact abort, with immutable argv and no stale output."""
    if not 1 <= max_attempts <= 6:
        raise ValueError('max_attempts must be between one and six')
    command = tuple(command)
    target, log = Path(target), Path(log)
    target.parent.mkdir(parents=True, exist_ok=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    attempts_dir = log.parent / (log.stem + '-link-attempts')
    # Reusing an output directory must not overwrite an earlier failed log.
    suffix = 1
    while attempts_dir.exists():
        suffix += 1
        attempts_dir = log.parent / (log.stem + '-link-attempts-' + str(suffix))
    attempts_dir.mkdir(parents=True)
    history = []
    for attempt in range(1, max_attempts + 1):
        target.unlink(missing_ok=True)
        process = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = process.stdout
        attempt_log = attempts_dir / ('attempt-%02d.log' % attempt)
        attempt_log.write_bytes(output)
        log.write_bytes(output)
        retryable = process.returncode != 0 and recognized_linker_abort(output)
        row = {'attempt': attempt, 'returncode': process.returncode,
               'log': str(attempt_log), 'log_sha256': hashlib.sha256(output).hexdigest(),
               'recognized_linker_abort': retryable}
        history.append(row)
        if process.returncode:
            target.unlink(missing_ok=True)
        (attempts_dir / 'receipt.json').write_text(json.dumps(
            {'command': list(command), 'max_attempts': max_attempts, 'attempts': history}, indent=2) + '\n')
        if process.returncode == 0:
            if not target.is_file():
                raise RuntimeError('Compiler returned success without creating the ELF')
            return history
        if not retryable or attempt == max_attempts:
            raise subprocess.CalledProcessError(process.returncode, command, output=output)
    raise AssertionError('unreachable')
