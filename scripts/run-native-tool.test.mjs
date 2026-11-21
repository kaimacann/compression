/**
 * scripts/run-native-tool.test.mjs
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// API
import assert from 'node:assert/strict';
import test, { after } from 'node:test';
import { chmod, mkdtemp, rm, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

const fixtureDirs = [];

/*//////////////////////////////////////////////////
HELPERS
//////////////////////////////////////////////////*/

/**
 * Prepare a temporary PATH entry with a controlled Make command.
 *
 * @param includeMake Whether to create the fake command in this fixture.
 *
 * @return The temporary directory path.
 */
async function createToolDir(includeMake = true) {
	const dir = await mkdtemp(join(tmpdir(), 'compression-native-tool-'));
	fixtureDirs.push(dir);
	if (includeMake) {
		const makePath = join(dir, 'make');
		await writeFile(makePath, '#!/bin/sh\nprintf \'%s\\n\' "$@"\nexit "${NATIVE_TOOL_EXIT_CODE:-0}"\n', { mode: 0o755 });
		await chmod(makePath, 0o755);
	}
	return dir;
}

/**
 * Run the wrapper with only the supplied directory available on PATH.
 *
 * @param dir Temporary PATH directory for the child process.
 * @param args Tool name and arguments passed to the wrapper.
 * @param exitCode Exit status requested from the fake native command.
 *
 * @return The completed wrapper process result.
 */
function runWrapper(dir, args, exitCode = '0') {
	return spawnSync(process.execPath, [fileURLToPath(new URL('./run-native-tool.mjs', import.meta.url)), ...args], {
		encoding: 'utf8',
		env: { ...process.env, PATH: dir, NATIVE_TOOL_EXIT_CODE: exitCode },
	});
}

after(async () => {
	for (const dir of fixtureDirs) {
		await rm(dir, { recursive: true, force: true });
	}
});

test('forwards native-tool arguments without changing their boundaries', async () => {
	const result = runWrapper(await createToolDir(), ['make', 'first value', '--mode', 'safe']);
	assert.equal(result.status, 0);
	assert.equal(result.stdout, 'first value\n--mode\nsafe\n');
});

test("returns the native tool's nonzero exit status", async () => {
	assert.equal(runWrapper(await createToolDir(), ['make', '--quality-check'], '23').status, 23);
});

test('reports a missing native tool with setup guidance', async () => {
	const result = runWrapper(await createToolDir(false), ['make', '--version']);
	assert.equal(result.status, 1);
	assert.equal(result.stderr, "The system tool 'make' is required; see the README for setup.\n");
});
