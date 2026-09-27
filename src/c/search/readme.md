## Search -- Haadi

```javascript
/**
 * @param
 * - Hits -- number of hits per page
 * - Sort -- sort type, e.g. alphabetical by name
 * - Filter -- facets of hits, e.g. files under 5Mb, name includes string
 * @example 
 * `RETURN 20 SORT name,size FILTER size>50,includes(name,'test') `
 * @todo consider whether it should be broken down more so querying is more intuitive (e.g. three prompts).
 */
function list 
```

```javascript
function hits
```

```javascript
function sort 
```

```javascript
function filter
```

```javascript
/**
 * @param array files -- file path(s) of files to view
 * @returns decrypted files
 */
function view
```
