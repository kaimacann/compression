/**
 * scripts/run-native-tool.mjs
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// API
import { spawnSync } from "node:child_process";

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

const [tool, ...args] = process.argv.slice(2);
const supportedTools = new Set(["doxygen", "make"]);
if (!tool || !supportedTools.has(tool)) {
  console.error(
    "Usage: node scripts/run-native-tool.mjs <make|doxygen> [arguments...]",
  );
  process.exitCode = 2;
} else {
  const result = spawnSync(tool, args, { stdio: "inherit" });
  if (result.error) {
    console.error(
      `The system tool '${tool}' is required; see the README for setup.`,
    );
    process.exitCode = 1;
  } else {
    process.exitCode = result.status ?? 1;
  }
}
