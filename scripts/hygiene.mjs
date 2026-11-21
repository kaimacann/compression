/**
 * scripts/hygiene.mjs
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

import { fileURLToPath } from 'node:url';
import { runQualityFromCfg } from '@terra-symposium/hygiene/cli';

await runQualityFromCfg(fileURLToPath(new URL('../config/quality.json', import.meta.url)), process.argv.slice(2));
