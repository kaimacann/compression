/**
 * scripts/terra-quality.mjs
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// API
import { fileURLToPath } from "node:url";
import { runQualityFromCfg } from "@terra-symposium/standards-quality/cli";
await runQualityFromCfg(
  fileURLToPath(new URL("../config/quality.json", import.meta.url)),
  process.argv.slice(2),
);
