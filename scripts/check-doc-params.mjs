import { readFile, readdir } from "node:fs/promises";
import { relative, resolve } from "node:path";

const root = resolve(import.meta.dirname, "..");
const sources = [];

/** Collect C translation units whose function contracts belong to this repository.
 * @param {string} directory Directory to inspect recursively.
 * @returns {Promise<void>} Resolves after every nested C source and header path is recorded.
 * @author haadi
 */
async function collect(directory) {
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const path = resolve(directory, entry.name);
    if (entry.isDirectory()) await collect(path);
    else if (/\.[ch]$/.test(entry.name)) sources.push(path);
  }
}

/** Replace comments and literals with spaces while retaining source offsets and lines.
 * @param {string} source C source text to scan.
 * @returns {string} Same-length text with non-code characters blanked out.
 * @author haadi
 */
function mask(source) {
  return source.replace(
    /\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g,
    (part) => part.replace(/[^\n]/g, " "),
  );
}

/** Find the matching closing delimiter while respecting nested delimiters.
 * @param {string} text Source text containing the opening delimiter.
 * @param {number} start Offset of the opening delimiter.
 * @param {string} open Opening delimiter to balance.
 * @param {string} close Closing delimiter to balance.
 * @returns {number} Closing delimiter offset, or -1 when the pair is incomplete.
 * @author haadi
 */
function matching(text, start, open = "(", close = ")") {
  let depth = 0;
  for (let i = start; i < text.length; i++) {
    if (text[i] === open) depth++;
    if (text[i] === close && --depth === 0) return i;
  }
  return -1;
}

/** Extract parameter identifiers from a C parameter list, including function pointers.
 * @param {string} text Text between a function declarator's parentheses.
 * @returns {string[]} Parameter names in signature order; a void list is empty.
 * @author haadi
 */
function parameters(text) {
  if (!text.trim() || text.trim() === "void") return [];
  const parts = [];
  let start = 0,
    round = 0,
    square = 0;
  for (let i = 0; i <= text.length; i++) {
    const c = text[i];
    if (c === "(") round++;
    if (c === ")") round--;
    if (c === "[") square++;
    if (c === "]") square--;
    if ((c === "," && !round && !square) || i === text.length) {
      const part = text.slice(start, i).trim();
      const pointer = part.match(/\(\s*\*\s*(\w+)/);
      const ids = [
        ...part.replace(/\[[^\]]*\]/g, "").matchAll(/[A-Za-z_]\w*/g),
      ].map((x) => x[0]);
      const name = pointer?.[1] ?? ids.at(-1);
      if (name && name !== "void" && part !== "...") parts.push(name);
      start = i + 1;
    }
  }
  return parts;
}

await collect(resolve(root, "src/c"));
let warnings = 0;
let functions = 0;
const checkedByFile = new Map();
for (const path of sources.sort()) {
  const source = await readFile(path, "utf8");
  const code = mask(source);
  let depth = 0,
    start = 0,
    lineStart = 0;
  for (let i = 0; i < code.length;) {
    if (depth === 0 && /[A-Za-z_]/.test(code[i])) {
      const id = code.slice(i).match(/^[A-Za-z_]\w*/)[0];
      const afterName = i + id.length;
      let open = afterName;
      while (/\s/.test(code[open] ?? "")) open++;
      if (
        code[open] === "(" &&
        !["if", "for", "while", "switch", "sizeof", "return"].includes(id)
      ) {
        const end = matching(code, open);
        let after = end + 1;
        while (/\s/.test(code[after] ?? "")) after++;
        const prefix = code.slice(start, i).trim();
        if (
          end > 0 &&
          (code[after] === "{" || code[after] === ";") &&
          /^[\w\s*]+$/.test(prefix) &&
          /[A-Za-z_]\w*/.test(prefix)
        ) {
          {
            const docs = [
              ...source.slice(0, i).matchAll(/\/\*\*([\s\S]*?)\*\//g),
            ];
            const last = docs.at(-1);
            const between = last
              ? code.slice(last.index + last[0].length, i).trim()
              : "";
            const doc = last && /^[\w\s*]+$/.test(between) ? last[1] : "";
            const args = parameters(code.slice(open + 1, end));
            const line = source.slice(0, i).split("\n").length;
            const file = relative(root, path);
            const location = `${file}:${line}: ${id}`;
            const author = file.startsWith("src/c/compression/")
              ? "kai"
              : /src\/c\/(encryption|login)\//.test(file)
                ? "marceline"
                : "haadi";
            const report = (message) => {
              console.warn(`${location}: ${message}`);
              warnings++;
            };
            functions++;
            checkedByFile.set(file, (checkedByFile.get(file) ?? 0) + 1);
            if (!doc) {
              report("missing TSDoc");
            } else {
              const summary = doc
                .split("\n")
                .map((row) => row.replace(/^\s*\*?\s?/, "").trim())
                .find((row) => row && !row.startsWith("@"));
              if (!summary) report("missing summary");
              const parameterTags = [
                ...doc.matchAll(/@param(?:\[[^\]]+\])?\s+(\w+)/g),
              ];
              const documentedOrder = parameterTags.map((match) => match[1]);
              const documented = new Set(documentedOrder);
              for (const name of args.filter((name) => !documented.has(name)))
                report(`missing @param ${name}`);
              for (const name of [...documented].filter(
                (name) => !args.includes(name),
              ))
                report(`stale @param ${name}`);
              if (
                args.length &&
                args.some((name, index) => documentedOrder[index] !== name)
              )
                report(`@param order should be ${args.join(", ")}`);
              for (const name of args.filter((name) => documented.has(name))) {
                if (
                  !new RegExp(
                    `@param(?:\\[[^\\]]+\\])?\\s+${name}\\s+\\S`,
                  ).test(doc)
                )
                  report(`missing description for @param ${name}`);
              }
              const resultType = prefix
                .replace(/\b(static|extern|inline|_Noreturn)\b/g, "")
                .trim();
              const nonVoid = !/^void$/.test(resultType);
              if (nonVoid && !/@returns?\s+\S/.test(doc))
                report("missing @return description");
              if (!new RegExp(`@author\\s+${author}\\b`, "i").test(doc))
                report(`expected @author ${author}`);
            }
          }
        }
      }
      i = afterName;
      continue;
    }
    const c = code[i++];
    if (depth === 0 && c === "\n") {
      if (/^\s*#/.test(code.slice(lineStart, i - 1))) start = i;
      lineStart = i;
    }
    if (c === "{") {
      depth++;
      if (depth === 1) start = i;
    }
    if (c === "}") {
      depth--;
      if (depth === 0) start = i;
    }
    if (depth === 0 && c === ";") start = i;
  }
}
for (const [file, count] of checkedByFile)
  console.log(
    `${file}: ${count} function declaration(s)/definition(s) checked.`,
  );
console.log(
  warnings
    ? `Found ${warnings} documentation issue(s) across ${functions} C declarations and definitions.`
    : `All ${functions} C declarations and definitions have summaries, parameter and return contracts, and the expected author.`,
);
if (warnings) process.exitCode = 1;
