# Agent skills

Skill instructions are authored in `docs/**/SKILL.md` pages marked `skill: true`. After installing dependencies, run `pnpm run skills:prepare` before using repository skills. This generates the ignored `.agents/skills/` discovery directory. Start a new agent session or refresh skill discovery after preparation. Run `pnpm run skills:check` to detect drift without rewriting files. The shared authoring guide is exported as `@terra-symposium/skills/authoring.md`. Repositories without marked pages prepare successfully with no skills.
