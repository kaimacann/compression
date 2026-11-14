# Complete repository test plan: compression

This checklist inventories repository-owned functions, documents, and configuration as of the current working tree. It is a plan, not a claim that every item is already tested. Update it when an owned source or document changes.

## Scope and method

- Function entries identify the source path and line. Exercise the stated normal, edge, and failure scenarios; link or add assertions as each entry is completed.
- Document entries require rendering, link checking, and content review. Execute documented commands against the current repository.
- Generated output, dependencies, vendored trees, bundled course viewers, and Portfolio pinned `projects/` snapshots are covered through build, package, and integration checks rather than individual upstream functions. Keep those boundaries under review when their inputs change.
- Put produced reports, screenshots, traces, and coverage under `tests/results/`.

## Function and executable source inventory

### `config/prettier.config.cjs`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `scripts/check-doc-params.mjs`

- [ ] `collect` (line 12): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `mask` (line 25): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@26` (line 26): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `matching` (line 37): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `parameters` (line 51): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@66` (line 66): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `report` (line 107): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@118` (line 118): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@119` (line 119): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@122` (line 122): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@124` (line 124): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@125` (line 125): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@126` (line 126): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@127` (line 127): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `scripts/quality-runner.mjs`

- [ ] `callback@18` (line 18): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@45` (line 45): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `isBuildOrTest` (line 50): Run on clean input; verify deterministic output and a useful failure on invalid input. No direct test file is linked yet.
- [ ] `callback@53` (line 53): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `callback@55` (line 55): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `scripts/terra-quality.mjs`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/compression/codec.c`

- [ ] `copyBytes` (line 203): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `fillBytes` (line 223): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcAppend` (line 239): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcCrc` (line 272): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `adler` (line 291): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `be32` (line 307): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readBits` (line 325): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `makeHuff` (line 348): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeHuff` (line 386): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `readCodeLengthRun` (line 417): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readDynamicTables` (line 464): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readStoredBlock` (line 532): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `buildFixedTables` (line 554): Run on clean input; verify deterministic output and a useful failure on invalid input. No direct test file is linked yet.
- [ ] `copyInflateMatch` (line 584): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeHuffmanBlock` (line 615): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `rcInflate` (line 643): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 705): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `reverseBits` (line 723): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `fixedSymbol` (line 739): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 755): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `findDeflateMatch` (line 780): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `indexDeflatePositions` (line 827): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcDeflate` (line 850): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 874): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 890): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 896): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `reverseBits` (line 898): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 901): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `indexDeflatePositions` (line 908): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `putBits` (line 916): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `paeth` (line 940): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcSample` (line 961): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcUnfilter` (line 981): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readPngHeader` (line 1019): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readPngPalette` (line 1051): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readPngTransparency` (line 1072): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `handlePngAncillary` (line 1105): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `handlePngChunk` (line 1121): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `parsePngChunks` (line 1164): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `rcCrc` (line 1174): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodePngPixel` (line 1212): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `decodePngRow` (line 1268): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `decodePngPass` (line 1289): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `decodePngRaster` (line 1326): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `pngDecode` (line 1378): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `fillBytes` (line 1386): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodePngRaster` (line 1388): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `rcDecode` (line 1413): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.

### `src/c/compression/codec.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/compression/compression.c`

- [ ] `openInputFile` (line 63): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `createOutputFile` (line 78): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `finishOutputFile` (line 93): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `closeInputFile` (line 111): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `discardOutputFile` (line 125): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `nodeLess` (line 216): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `queueInsert` (line 233): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `takeMinimum` (line 248): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `buildHuffmanTree` (line 262): Run on clean input; verify deterministic output and a useful failure on invalid input. No direct test file is linked yet.
- [ ] `assignCodes` (line 313): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `scanInput` (line 344): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `measureArchiveSource` (line 379): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `isMediaInput` (line 399): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `calculateEncodedBits` (line 421): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `prepareArchiveEncoding` (line 448): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writeUnsignedLongLong` (line 474): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `readUnsignedLongLong` (line 491): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `writeHeader` (line 511): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `readHeader` (line 539): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `memcmp` (line 544): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `copyPayload` (line 584): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `fwrite` (line 600): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `writeHuffmanPayload` (line 624): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `sameFrequencies` (line 684): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `bitReaderGet` (line 696): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `finishBitReader` (line 717): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readHuffmanSymbol` (line 739): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `writeDecodedHuffman` (line 768): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `compressFile` (line 822): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `archiveFile` (line 834): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writeArchiveFrame` (line 844): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `copyPayload` (line 851): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `archiveFile` (line 865): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decompressFile` (line 975): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `frequencyBefore` (line 1061): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `mergeSortFrequencies` (line 1076): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `inspectFile` (line 1113): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `buildSearchPrefix` (line 1169): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `advanceSearch` (line 1193): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `searchFile` (line 1211): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.

### `src/c/compression/compression.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/compression/jpeg.c`

- [ ] `j16` (line 178): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `jbits` (line 188): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `jhuff` (line 224): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `jreceive` (line 251): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `nextMarker` (line 266): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `ex16` (line 291): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `ex32` (line 303): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `exifOrientation` (line 316): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `ex16` (line 344): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `frameGeometry` (line 363): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `frame` (line 383): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `refineNonzero` (line 471): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeInitialAc` (line 488): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `refineAcSymbol` (line 531): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeDc` (line 579): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `decodeBlock` (line 613): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `readScanComponents` (line 679): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `validScanBand` (line 727): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `recordScanProgression` (line 751): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `prepareScan` (line 779): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `resetScanInterval` (line 807): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeMcu` (line 838): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `scan` (line 876): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `clampByte` (line 912): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `buildIdctBasis` (line 927): Run on clean input; verify deterministic output and a useful failure on invalid input. No direct test file is linked yet.
- [ ] `hasAcCoefficients` (line 947): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `renderDcBlock` (line 967): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `renderAcBlock` (line 991): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `renderComponent` (line 1034): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `sampleComponent` (line 1070): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `renderPixel` (line 1118): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `render` (line 1154): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `renderPixel` (line 1178): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `orientImage` (line 1192): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readQuantizationTables` (line 1249): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readHuffmanTables` (line 1282): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `processMarker` (line 1325): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readSegment` (line 1374): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `rcJpeg` (line 1405): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `src/c/compression/media.c`

- [ ] `appendText` (line 85): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `appendUnsigned` (line 105): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writeDiagnostic` (line 134): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `writeUnsignedDiagnostic` (line 145): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `copyText` (line 168): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readWhole` (line 186): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `fileEquals` (line 227): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `uniqueFile` (line 260): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `cleanOutputs` (line 290): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `validateOutputTarget` (line 329): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `validateOutputTargets` (line 376): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `stageOutput` (line 392): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `stageOutputs` (line 419): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `saveExistingOutput` (line 435): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `reserveOutput` (line 454): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rollbackOutputs` (line 469): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `commitOutputs` (line 491): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writeOutputs` (line 519): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `imagePath` (line 536): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `detectMediaFormat` (line 586): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `validMediaRequest` (line 608): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `preparePdf` (line 627): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `prepareRaster` (line 654): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `createOutputs` (line 687): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `reportMediaResult` (line 722): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `cleanMediaWork` (line 754): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `processMedia` (line 774): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `src/c/compression/media.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/compression/pdf.c`

- [ ] `white` (line 142): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `delimiter` (line 153): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `copyPdfBytes` (line 164): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `movePdfBytes` (line 179): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `space` (line 202): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `token` (line 226): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `uintAt` (line 241): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `literalStringEnd` (line 280): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `hexStringEnd` (line 314): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `containerEnd` (line 331): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `scalarEnd` (line 360): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `valueEnd` (line 394): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `containerEnd` (line 409): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `dictValue` (line 423): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `nameIs` (line 468): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `objectValue` (line 482): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `reference` (line 522): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `resolve` (line 553): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `sizeValue` (line 576): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `space` (line 580): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `getSize` (line 596): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `getRef` (line 610): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `locateXref` (line 638): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `memcmp` (line 643): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `findTrailer` (line 682): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `prepareTrailer` (line 736): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `dictValue` (line 749): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readXrefEntry` (line 766): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readXrefSubsection` (line 802): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `indexXrefObjects` (line 835): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `validateStreamBoundary` (line 856): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `validateOneObject` (line 892): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `validateLiveObjects` (line 925): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `parsePdf` (line 944): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `collectFilterItems` (line 1000): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeFilterItem` (line 1042): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `appendAscii85Value` (line 1073): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `appendAscii85Remainder` (line 1092): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `appendAscii85Value` (line 1103): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `consumeAscii85Byte` (line 1112): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `ascii85` (line 1159): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `hexDigit` (line 1184): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `asciiHex` (line 1203): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodeFilters` (line 1248): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `selectPredictorDictionary` (line 1307): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readOptionalPredictorSize` (line 1334): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `resolvePredictorLayout` (line 1348): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `reverseTiffPredictor` (line 1404): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `reversePngPredictor` (line 1443): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `predictor` (line 1471): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readPdfDecodeRanges` (line 1509): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `expandPdfPixel` (line 1550): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `expandPdfSamples` (line 1596): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `loadPdfLookup` (line 1623): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readPdfIndexedColor` (line 1657): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `readPdfImageColor` (line 1707): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `applyPdfColorKey` (line 1731): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decodePdfJpegImage` (line 1785): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `decodePdfRawImage` (line 1804): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `validatePdfImage` (line 1848): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `decodePdfImageData` (line 1881): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `pdfImage` (line 1903): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `appendUnsigned` (line 2007): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writePdfObject` (line 2034): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `writePdfCrossReference` (line 2066): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `rcAppend` (line 2084): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `serialize` (line 2098): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `markPdfMasks` (line 2130): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `getRef` (line 2136): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `writePdfForm` (line 2150): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `sizeof` (line 2159): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `processPdfImageObject` (line 2188): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `processPdfImages` (line 2234): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `dictValue` (line 2242): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `releasePdfObjects` (line 2257): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `releasePdfImages` (line 2273): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `processPdf` (line 2295): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `src/c/compression/vector.c`

- [ ] `colorKey` (line 127): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `compareBins` (line 145): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `siftColorBins` (line 157): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `sortColorBins` (line 181): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `collectColorBins` (line 205): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `scoreColorBox` (line 261): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `chooseColorBox` (line 310): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `splitColorBox` (line 336): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `averagePalette` (line 362): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `assignPaletteLabels` (line 394): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `paletteImage` (line 431): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `splitColorBox` (line 470): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `pointAdd` (line 504): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `labelAt` (line 531): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `edge` (line 548): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `labelAt` (line 570): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `distanceSquared` (line 586): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `distinctCornerLabels` (line 610): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `pointBefore` (line 637): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rotateContourToAnchor` (line 652): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `splitPointBefore` (line 690): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `simplifySegments` (line 702): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `retainSameWinding` (line 737): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `simplify` (line 773): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `emitContour` (line 834): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `storeTraceContour` (line 882): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `traceContour` (line 900): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `traceVectorContours` (line 961): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writeVectorDocument` (line 996): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `sizeof` (line 1012): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcFreeVector` (line 1064): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `rcVectorize` (line 1080): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `src/c/encryption/encryption.c`

- [ ] `initCipher` (line 9): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `nextRandom` (line 25): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `performCipher` (line 40): Render each state; check interactions, accessibility, loading, and error behavior. No direct test file is linked yet.
- [ ] `encryptFile` (line 73): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `src/c/encryption/encryption.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/fileIO/file.c`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/fileIO/fileInput/fileInput.c`

- [ ] `fileNameFromPath` (line 21): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `lowerAscii` (line 37): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `hasExtension` (line 48): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `detectFileType` (line 69): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readInputFile` (line 95): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `clearInput` (line 163): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `selectFiles` (line 181): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `displayFiles` (line 199): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `createFile` (line 216): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.

### `src/c/fileIO/fileInput/fileInput.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/fileIO/fileOutput/fileOutput.c`

- [ ] `browseFiles` (line 17): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `writeOutputFile` (line 26): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.

### `src/c/fileIO/fileOutput/fileOutput.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/login/login.c`

- [ ] `createAccount` (line 9): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `loadCredentials` (line 74): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `authenticateUser` (line 108): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `strcmp` (line 140): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `loginFunction` (line 153): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

### `src/c/login/login.h`

- [ ] File behavior: execute or compile this source, including expected output and failure handling. No direct test file is linked yet.

### `src/c/main/main.c`

- [ ] `writePathDiagnostic` (line 47): Test successful mutation, duplicate/conflict, validation, and failed-write behavior. No direct test file is linked yet.
- [ ] `compressAndEncrypt` (line 60): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `decryptAndDecompress` (line 81): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `usage` (line 104): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `parseColors` (line 124): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `parseTolerance` (line 142): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `processCommand` (line 160): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `runCommand` (line 201): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readPath` (line 238): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `validateInput` (line 267): Test valid, malformed, empty, and boundary input; assert exact output or error. No direct test file is linked yet.
- [ ] `defaultOutputPath` (line 311): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `vectorizeMedia` (line 353): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `runInteractiveAction` (line 365): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `readMenuChoice` (line 426): Test found, missing, unauthorized, and upstream-failure results. No direct test file is linked yet.
- [ ] `runMenu` (line 462): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `main` (line 501): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.
- [ ] `loginFunction` (line 504): Test expected input/output, empty and boundary cases, and a relevant failure path. No direct test file is linked yet.

## Document inventory

- [ ] [`.changeset/repository-structure.md`](../.changeset/repository-structure.md): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`CHANGELOG.md`](../CHANGELOG.md): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`README.md`](../README.md): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/assignment/brief.mdx`](../docs/assessment/assignment/brief.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/assignment/code-submission.mdx`](../docs/assessment/assignment/code-submission.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/assignment/index.mdx`](../docs/assessment/assignment/index.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/assignment/report-submission.mdx`](../docs/assessment/assignment/report-submission.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/assignment/report-writing-scaffold.mdx`](../docs/assessment/assignment/report-writing-scaffold.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/index.mdx`](../docs/assessment/index.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/project-specifications/business.mdx`](../docs/assessment/project-specifications/business.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/assessment/project-specifications/technical.mdx`](../docs/assessment/project-specifications/technical.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/compression/guide/file-handling.mdx`](../docs/compression/guide/file-handling.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/compression/guide/flowchart-export.mdx`](../docs/compression/guide/flowchart-export.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/compression/guide/reference.mdx`](../docs/compression/guide/reference.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/compression/index.mdx`](../docs/compression/index.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/development/tests/after/image-stream.pdf`](../docs/development/tests/after/image-stream.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`docs/development/tests/after/multi-image.pdf`](../docs/development/tests/after/multi-image.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`docs/development/tests/after/rebuild.pdf`](../docs/development/tests/after/rebuild.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`docs/development/tests/before/image-stream.pdf`](../docs/development/tests/before/image-stream.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`docs/development/tests/before/multi-image.pdf`](../docs/development/tests/before/multi-image.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`docs/development/tests/before/rebuild.pdf`](../docs/development/tests/before/rebuild.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`docs/development/tests/gallery.html`](../docs/development/tests/gallery.html): Render the page, check links, keyboard access, and browser behavior.
- [ ] [`docs/development/tests/index.mdx`](../docs/development/tests/index.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`docs/index.mdx`](../docs/index.mdx): Render, check links and examples, then review each factual claim and instruction against the current implementation.
- [ ] [`samples/image-stream.pdf`](../samples/image-stream.pdf): Render every page, inspect layout and text, and verify referenced procedures and links.
- [ ] [`samples/repeated.txt`](../samples/repeated.txt): Check encoding, content integrity, links, and the owning reader or renderer.
- [ ] [`samples/sample.txt`](../samples/sample.txt): Check encoding, content integrity, links, and the owning reader or renderer.

## Configuration and other owned files

- [ ] `.changeset/config.json`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `.github/workflows/repository-check.yml`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `.gitignore`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `.husky/pre-commit`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `LICENSE`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `Makefile`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `config/.clang-format`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `config/Doxyfile`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `config/package.json`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `config/pnpm-lock.yaml`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `config/pnpm-workspace.yaml`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `config/quality.json`: validate syntax or schema, exercise its owning tool, and check references to this file.
- [ ] `docs/assessment/assignment/images/group-project-image-encryption.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/assessment/images/group-project-image-encryption.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/bald-lossless.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/bald-vector.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/duck-vector.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/image-stream-svg/image-stream-image-001.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/jpeg-vector.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/jpeg-vector.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-multi-svg/multi-image-image-001.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-multi-svg/multi-image-image-002.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-multi-svg/multi-image-image-003.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-render/multi-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-render/multi-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-render/page-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/pdf-render/page-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-1.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-16.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-2.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-4.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-8-trns.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-8.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-alpha-16.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-alpha-8.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/gray-alpha-adam7.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/palette-1-alpha.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/palette-2-alpha.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/palette-4-alpha.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/palette-8-alpha.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/palette-adam7.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/rgb-16.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/rgb-8.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/rgba-16.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/rgba-8.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-cases/rgba-adam7.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-4.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-8-trns.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-alpha-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-alpha-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/gray-alpha-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/palette-1-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/palette-2-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/palette-4-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/palette-8-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/palette-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/rgb-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/rgb-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/rgba-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/rgba-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/png-preview/rgba-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/after/rebuild-svg/rebuild-image-001.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/bald-source.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/duck-source.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/jpeg-source.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/pdf-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/pdf-render/multi-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/pdf-render/multi-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/pdf-render/page-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/pdf-render/page-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-4.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-8-trns.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-alpha-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-alpha-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/gray-alpha-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/palette-1-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/palette-2-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/palette-4-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/palette-8-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/palette-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/rgb-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/rgb-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/rgba-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/rgba-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-cases/rgba-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-1.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-2.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-4.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-8-trns.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-alpha-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-alpha-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/gray-alpha-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/palette-1-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/palette-2-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/palette-4-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/palette-8-alpha.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/palette-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/rgb-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/rgb-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/rgba-16.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/rgba-8.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/png-preview/rgba-adam7.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `docs/development/tests/before/test.jpeg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `public/favicon.svg`: render or load this asset and inspect the visual or byte-level result.
- [ ] `samples/bald.png`: render or load this asset and inspect the visual or byte-level result.
- [ ] `samples/duck.png`: render or load this asset and inspect the visual or byte-level result.

Inventory: 319 function-like declarations, 27 documents, 117 other owned files, and 23 executable source files.
