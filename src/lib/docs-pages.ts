import type { DocsLocalPage } from "@terra-symposium/docs";
import assignmentImageUrl from "../../docs/assessment/assignment/images/group-project-image-encryption.svg?url";

const modules = import.meta.glob("../../docs/**/*.{md,mdx}", {
  eager: true,
}) as Record<
  string,
  { default: unknown; frontmatter?: { title?: string; description?: string } }
>;
const sources = import.meta.glob("../../docs/**/*.{md,mdx}", {
  query: "?raw",
  import: "default",
  eager: true,
}) as Record<string, string>;
const stripFrontmatter = (source: string) =>
  source.replace(/^---\s*\n[\s\S]*?\n---\s*\n/, "");
const assignmentBriefFile = "../../docs/assessment/assignment/brief.md";
const assignmentBrief = stripFrontmatter(sources[assignmentBriefFile] ?? "");
const assignmentImageSource = assignmentBrief
  .match(/^\s*!\[[^\]]*\]\([^)]+\)\s*$/m)?.[0]
  ?.trim();
const assignmentImage = assignmentImageSource?.replace(
  "./images/group-project-image-encryption.svg",
  assignmentImageUrl,
);
const assignmentText = assignmentImage
  ? assignmentBrief.replace(assignmentImageSource!, "").trimEnd()
  : assignmentBrief.trimEnd();

export const docs: DocsLocalPage[] = Object.entries(modules)
  .filter(([file]) => file !== assignmentBriefFile)
  .map(([file, module]) => {
    const slug = file
      .replace("../../docs/", "")
      .replace(/(?:^|\/)index\.mdx?$/, "")
      .replace(/\.mdx?$/, "");
    const id = slug || "index";
    const title =
      module.frontmatter?.title ??
      (id === "index"
        ? "Compression"
        : id
            .split("/")
            .at(-1)!
            .replace(/-/g, " ")
            .replace(/\b\w/g, (value) => value.toUpperCase()));
    let markdown = stripFrontmatter(String(sources[file] ?? ""));
    if (id === "assessment/assignment") {
      markdown = `${markdown.trimEnd()}\n\n## Official assignment brief (verbatim)\n\n${assignmentText}${assignmentImage ? `\n\n${assignmentImage}` : ""}\n`;
    }
    return {
      id,
      path: file.replace("../../", ""),
      pathname: slug ? `/${slug}/` : "/",
      title,
      ...(module.frontmatter?.description
        ? { description: module.frontmatter.description }
        : {}),
      markdown,
    };
  });
