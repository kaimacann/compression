/**
 * config/prettier.config.cjs
 */
/**
 * Shared formatting style for this repository.
 */
const config = {
	arrowParens: 'avoid',
	jsxSingleQuote: true,
	printWidth: 200,
	proseWrap: 'never',
	singleQuote: true,
	useTabs: true,
	overrides: [{ files: ['**/*.md', '**/*.mdx'], options: { parser: 'mdx', objectWrap: 'collapse' } }],
};
module.exports = config;
