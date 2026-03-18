#!/usr/bin/env python3
"""
Simplex Playground Server

A Flask-based backend for compiling and running Simplex code safely.
Designed for deployment at play.simplex-lang.org.

Security features:
- Sandboxed execution with resource limits
- Temporary file cleanup
- Timeout enforcement
- No network access from user code (when running in Docker)
"""

import os
import sys
import uuid
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from functools import wraps

from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS

app = Flask(__name__, static_folder='.', static_url_path='')
CORS(app)

# Configuration
CONFIG = {
    # Compiler settings
    'SXC_PATH': os.environ.get('SXC_PATH', '/usr/local/bin/sxc'),
    'RUNTIME_PATH': os.environ.get('RUNTIME_PATH', '/usr/local/lib/simplex/standalone_runtime.c'),

    # Execution limits
    'COMPILE_TIMEOUT': int(os.environ.get('COMPILE_TIMEOUT', 30)),  # seconds
    'RUN_TIMEOUT': int(os.environ.get('RUN_TIMEOUT', 10)),  # seconds
    'MAX_OUTPUT_SIZE': int(os.environ.get('MAX_OUTPUT_SIZE', 65536)),  # bytes
    'MAX_CODE_SIZE': int(os.environ.get('MAX_CODE_SIZE', 102400)),  # bytes (100KB)

    # Resource limits (for ulimit)
    'MAX_MEMORY': int(os.environ.get('MAX_MEMORY', 256 * 1024 * 1024)),  # 256MB
    'MAX_FILE_SIZE': int(os.environ.get('MAX_FILE_SIZE', 10 * 1024 * 1024)),  # 10MB
    'MAX_PROCESSES': int(os.environ.get('MAX_PROCESSES', 32)),

    # Temp directory
    'TEMP_BASE': os.environ.get('TEMP_BASE', '/tmp/simplex-playground'),

    # Rate limiting
    'RATE_LIMIT_REQUESTS': int(os.environ.get('RATE_LIMIT_REQUESTS', 10)),
    'RATE_LIMIT_WINDOW': int(os.environ.get('RATE_LIMIT_WINDOW', 60)),  # seconds
}

# Ensure temp directory exists
os.makedirs(CONFIG['TEMP_BASE'], exist_ok=True)

# Simple in-memory rate limiting
rate_limit_store = {}


def rate_limit(func):
    """Simple rate limiting decorator based on IP address."""
    @wraps(func)
    def wrapper(*args, **kwargs):
        ip = request.remote_addr or 'unknown'
        now = time.time()

        # Clean old entries
        rate_limit_store[ip] = [
            t for t in rate_limit_store.get(ip, [])
            if now - t < CONFIG['RATE_LIMIT_WINDOW']
        ]

        # Check limit
        if len(rate_limit_store.get(ip, [])) >= CONFIG['RATE_LIMIT_REQUESTS']:
            return jsonify({
                'success': False,
                'error': f'Rate limit exceeded. Please wait before trying again.'
            }), 429

        # Record request
        rate_limit_store.setdefault(ip, []).append(now)

        return func(*args, **kwargs)
    return wrapper


def create_temp_dir():
    """Create a unique temporary directory for code execution."""
    run_id = str(uuid.uuid4())[:8]
    temp_dir = os.path.join(CONFIG['TEMP_BASE'], run_id)
    os.makedirs(temp_dir, exist_ok=True)
    return temp_dir, run_id


def cleanup_temp_dir(temp_dir):
    """Remove temporary directory and all contents."""
    try:
        shutil.rmtree(temp_dir, ignore_errors=True)
    except Exception as e:
        app.logger.warning(f"Failed to cleanup temp dir {temp_dir}: {e}")


def truncate_output(output, max_size):
    """Truncate output if it exceeds max size."""
    if len(output) > max_size:
        return output[:max_size] + f"\n... (output truncated at {max_size} bytes)"
    return output


def run_with_limits(cmd, cwd, timeout, capture=True):
    """
    Run a command with resource limits.

    In production (Docker), additional sandboxing is provided by the container.
    This function adds process-level limits as an additional safety layer.
    """
    # Build the command with resource limits (Linux/macOS)
    if sys.platform == 'linux':
        # Use timeout and ulimit on Linux
        limited_cmd = [
            'timeout', '--signal=KILL', str(timeout),
            'sh', '-c',
            f'ulimit -v {CONFIG["MAX_MEMORY"] // 1024} -f {CONFIG["MAX_FILE_SIZE"] // 1024} -u {CONFIG["MAX_PROCESSES"]} && exec "$@"',
            '--'
        ] + cmd
    elif sys.platform == 'darwin':
        # macOS has limited ulimit support, use gtimeout if available
        limited_cmd = cmd
        # Try to use gtimeout from coreutils
        try:
            subprocess.run(['gtimeout', '--version'], capture_output=True, check=True)
            limited_cmd = ['gtimeout', '--signal=KILL', str(timeout)] + cmd
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass
    else:
        limited_cmd = cmd

    try:
        result = subprocess.run(
            limited_cmd,
            cwd=cwd,
            capture_output=capture,
            timeout=timeout + 5,  # Grace period beyond signal timeout
            text=True
        )
        return {
            'returncode': result.returncode,
            'stdout': truncate_output(result.stdout, CONFIG['MAX_OUTPUT_SIZE']),
            'stderr': truncate_output(result.stderr, CONFIG['MAX_OUTPUT_SIZE'])
        }
    except subprocess.TimeoutExpired:
        return {
            'returncode': -1,
            'stdout': '',
            'stderr': f'Execution timed out after {timeout} seconds'
        }
    except Exception as e:
        return {
            'returncode': -1,
            'stdout': '',
            'stderr': f'Execution error: {str(e)}'
        }


def compile_code(code, temp_dir):
    """
    Compile Simplex code to an executable.

    Returns (success, executable_path or error_message)
    """
    source_file = os.path.join(temp_dir, 'main.sx')

    # Write source code
    with open(source_file, 'w') as f:
        f.write(code)

    # Compile using sxc
    result = run_with_limits(
        [CONFIG['SXC_PATH'], 'build', source_file, '-o', 'main'],
        cwd=temp_dir,
        timeout=CONFIG['COMPILE_TIMEOUT']
    )

    executable = os.path.join(temp_dir, 'main')

    if result['returncode'] == 0 and os.path.exists(executable):
        return True, executable
    else:
        error = result['stderr'] or result['stdout'] or 'Unknown compilation error'
        return False, error


def run_executable(executable, temp_dir):
    """
    Run the compiled executable with safety limits.

    Returns (success, stdout, stderr, exit_code)
    """
    result = run_with_limits(
        [executable],
        cwd=temp_dir,
        timeout=CONFIG['RUN_TIMEOUT']
    )

    return (
        result['returncode'] == 0,
        result['stdout'],
        result['stderr'],
        result['returncode']
    )


@app.route('/')
def index():
    """Serve the main HTML page."""
    return send_from_directory('.', 'index.html')


@app.route('/<path:path>')
def static_files(path):
    """Serve static files."""
    return send_from_directory('.', path)


@app.route('/api/run', methods=['POST'])
@rate_limit
def api_run():
    """
    Compile and run Simplex code.

    Request body:
    {
        "code": "fn main() { println(\"Hello\"); }"
    }

    Response:
    {
        "success": true/false,
        "stdout": "...",
        "stderr": "...",
        "exit_code": 0,
        "compile_error": "..." (if compilation failed)
    }
    """
    try:
        data = request.get_json()
        if not data or 'code' not in data:
            return jsonify({
                'success': False,
                'error': 'No code provided'
            }), 400

        code = data['code']

        # Validate code size
        if len(code) > CONFIG['MAX_CODE_SIZE']:
            return jsonify({
                'success': False,
                'error': f'Code too large (max {CONFIG["MAX_CODE_SIZE"]} bytes)'
            }), 400

        # Create temp directory
        temp_dir, run_id = create_temp_dir()
        app.logger.info(f"Running code in {temp_dir}")

        try:
            # Compile
            compile_success, compile_result = compile_code(code, temp_dir)

            if not compile_success:
                return jsonify({
                    'success': False,
                    'compile_error': compile_result
                })

            # Run
            run_success, stdout, stderr, exit_code = run_executable(compile_result, temp_dir)

            return jsonify({
                'success': run_success or exit_code == 0,
                'stdout': stdout,
                'stderr': stderr,
                'exit_code': exit_code
            })

        finally:
            # Always cleanup
            cleanup_temp_dir(temp_dir)

    except Exception as e:
        app.logger.exception(f"Error processing request: {e}")
        return jsonify({
            'success': False,
            'error': 'Internal server error'
        }), 500


@app.route('/api/health', methods=['GET'])
def api_health():
    """Health check endpoint."""
    # Check if compiler is available
    compiler_ok = os.path.exists(CONFIG['SXC_PATH']) or shutil.which('sxc') is not None

    return jsonify({
        'status': 'healthy' if compiler_ok else 'degraded',
        'compiler': 'available' if compiler_ok else 'not found',
        'version': '0.10.0'
    })


@app.route('/api/version', methods=['GET'])
def api_version():
    """Return Simplex version information."""
    try:
        result = subprocess.run(
            [CONFIG['SXC_PATH'], 'version'],
            capture_output=True,
            text=True,
            timeout=5
        )
        version = result.stdout.strip() if result.returncode == 0 else 'unknown'
    except Exception:
        version = 'unknown'

    return jsonify({
        'simplex_version': version,
        'playground_version': '1.0.0'
    })


def find_sxc():
    """Try to find sxc in common locations."""
    # Get the directory of this script file (handle both module and exec cases)
    try:
        script_dir = os.path.dirname(os.path.abspath(__file__))
    except NameError:
        script_dir = os.getcwd()

    locations = [
        CONFIG['SXC_PATH'],
        os.path.join(script_dir, '..', 'sxc'),  # Local sxc in parent (simplex repo)
        '/usr/local/bin/sxc',
        '/usr/bin/sxc',
        os.path.expanduser('~/simplex/sxc'),
    ]

    for loc in locations:
        if os.path.exists(loc) and os.access(loc, os.X_OK):
            return loc

    # Try PATH
    which_result = shutil.which('sxc')
    if which_result:
        return which_result

    return None


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Simplex Playground Server')
    parser.add_argument('--host', default='0.0.0.0', help='Host to bind to')
    parser.add_argument('--port', type=int, default=8080, help='Port to listen on')
    parser.add_argument('--debug', action='store_true', help='Enable debug mode')
    parser.add_argument('--sxc', help='Path to sxc compiler')
    args = parser.parse_args()

    # Update config from args
    if args.sxc:
        CONFIG['SXC_PATH'] = args.sxc
    else:
        # Try to find sxc
        found_sxc = find_sxc()
        if found_sxc:
            CONFIG['SXC_PATH'] = found_sxc
            print(f"Found sxc at: {found_sxc}")
        else:
            print("Warning: sxc not found. Set SXC_PATH environment variable or use --sxc flag.")

    print(f"Starting Simplex Playground Server on {args.host}:{args.port}")
    print(f"Using compiler: {CONFIG['SXC_PATH']}")

    app.run(host=args.host, port=args.port, debug=args.debug)
