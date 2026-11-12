/**
 * scripts/run-native-tool.test.mjs
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// API
import assert from "node:assert/strict";
import test, { after } from "node:test";
import { chmod, mkdtemp, rm, writeFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

const runnerPath = fileURLToPath(
  new URL("./run-native-tool.mjs", import.meta.url),
);
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
  const dir = await mkdtemp(join(tmpdir(), "compression-native-tool-"));
  fixtureDirs.push(dir);
  if (includeMake) {
    const makePath = join(dir, "make");
    await writeFile(
      makePath,
      '#!/bin/sh\nprintf \'%s\\n\' "$@"\nexit "${NATIVE_TOOL_EXIT_CODE:-0}"\n',
      { mode: 0o755 },
    );
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
function runWrapper(dir, args, exitCode = "0") {
  return spawnSync(process.execPath, [runnerPath, ...args], {
    encoding: "utf8",
    env: { ...process.env, PATH: dir, NATIVE_TOOL_EXIT_CODE: exitCode },
  });
}

/**
 * Remove temporary PATH fixtures created by the wrapper tests.
 *
 */
async function removeFixtures() {
  for (const dir of fixtureDirs) {
    await rm(dir, { recursive: true, force: true });
  }
}

/**
 * Verify arguments with spaces arrive at the native command unchanged.
 *
 * @return Completes after the argument output is verified.
 */
async function forwardsArguments() {
  const dir = await createToolDir();
  const result = runWrapper(dir, ["make", "first value", "--mode", "safe"]);
  assert.equal(result.status, 0);
  assert.equal(result.stdout, "first value\n--mode\nsafe\n");
}

/**
 * Verify the wrapper preserves the exit code returned by the native command.
 *
 * @return Completes after the exit status is verified.
 */
async function propagatesExitStatus() {
  const dir = await createToolDir();
  const result = runWrapper(dir, ["make", "--quality-check"], "23");
  assert.equal(result.status, 23);
}

/**
 * Verify an unavailable system command produces actionable setup output.
 *
 * @return Completes after the missing-tool diagnostic is verified.
 */
async function reportsMissingTool() {
  const dir = await createToolDir(false);
  const result = runWrapper(dir, ["make", "--version"]);
  assert.equal(result.status, 1);
  assert.equal(
    result.stderr,
    "The system tool 'make' is required; see the README for setup.\n",
  );
}
after(removeFixtures);
test(
  "forwards native-tool arguments without changing their boundaries",
  forwardsArguments,
);
test("returns the native tool's nonzero exit status", propagatesExitStatus);
test("reports a missing native tool with setup guidance", reportsMissingTool);
