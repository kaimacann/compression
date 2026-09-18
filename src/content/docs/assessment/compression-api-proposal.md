---
title: Compression API proposal
description: 'Technical design for a separate compression API supporting Terra Symposium assets.'
---

> **Status:** proposal for in-class review. This document is authored here and is not part of the official Canvas specification.

## 1. Objective

- Build a separate repository for a compression API used by the Terra Symposium website.
- Support two initial asset classes:
  - documentation assets;
  - images in content storage.
- Provide a small hosted review interface.
- Demonstrate the C sorting requirement through the API’s asset listing path.
- Obtain tutor decisions on scope, dependencies, hosting, storage, and security before implementation.

## 2. Problem statement

Terra needs a stable upload boundary for documentation and image content. The API should:

- validate uploads and metadata;
- compress supported assets safely;
- recognise simple image representations such as colour data, backgrounds, and strokes;
- convert an approved stroke representation to SVG;
- store original and processed representations;
- return versioned, authenticated contracts;
- expose deterministic sorting for asset records.

The first version is a reviewable prototype, not a production media platform.

## 3. Business cases

### 3.1 Documentation assets

- One upload contract for Terra documentation assets.
- Consistent type, size, checksum, and metadata validation.
- Stable asset identifiers independent of storage-provider paths.
- Compression logic isolated from the Terra website repository.

### 3.2 Images in content storage

- Reduce storage and delivery cost for simple visual representations.
- Preserve the original upload and make every transformation explicit.
- Represent a recognised stroke as SVG rather than a raster image.
- Fall back to the original when classification is uncertain.
- Do not apply lossy conversion silently.

### 3.3 Review and assessment value

- A hosted interface makes the pipeline visible to tutors and reviewers.
- The project demonstrates:
  - a real-world data manipulation problem;
  - compression and decompression where supported;
  - modular C code;
  - sorting, and optionally searching;
  - testing, debugging, and design justification.

### 3.4 Repository separation

- Terra integrates through a versioned API contract.
- API implementation and storage can evolve independently.
- API tests run without the full Terra website.
- No compression implementation is imported directly into Terra.

## 4. Scope

### 4.1 In scope

- Separate repository: terra-compression-api.
- Versioned upload and retrieval API.
- Documentation asset and image upload paths.
- Configurable file and request limits.
- MIME/type detection, checksum, and metadata validation.
- Lossless-first processing.
- Explicit image classification for:
  - colour data;
  - backgrounds;
  - simple strokes.
- Stroke-to-SVG conversion for approved inputs.
- Original and processed object references.
- Authenticated, encrypted contract envelope where required.
- C sorting worker for asset records.
- Simple hosted review interface.
- Contract, unit, integration, and round-trip tests.

### 4.2 Out of scope

- Replacing Terra content storage wholesale.
- General-purpose CDN, DAM, or image editor.
- Video, audio, animation, OCR, or generative image analysis.
- Silent lossy conversion.
- Public multi-tenant access or production SLA.
- User account and billing systems.
- Malware scanning and enterprise retention policy unless required by hosting.
- Implementing cryptography from scratch.
- Permanent public deployment from this repository.

## 5. Proposed repository architecture

Repository layout:

    terra-compression-api/
    ├── apps/
    │   ├── api/              HTTP service and request orchestration
    │   └── review/           minimal hosted upload and inspection UI
    ├── packages/
    │   ├── contract/         schemas, versions, validation, errors
    │   ├── processing/       asset and image processing policies
    │   ├── storage/          local and approved object-store adapters
    │   └── security/         encryption envelope and key-provider interface
    ├── c/
    │   └── sort/             standard-library-only C sorting worker
    ├── tests/
    │   ├── contract/
    │   ├── integration/
    │   └── fixtures/
    ├── infra/                local/hosted configuration, no secrets
    └── README.md

### 5.1 Runtime components

| Component | Responsibility | Initial implementation |
| --- | --- | --- |
| API service | HTTP, validation, orchestration, error responses | Separate service in apps/api |
| Contract package | Versioned schemas and validation | JSON Schema or equivalent, pending dependency decision |
| Asset processor | Lossless compression and output metadata | Deterministic library-backed module |
| Image classifier | Classify supported image representations | Explicit rules and fixtures |
| SVG converter | Convert approved stroke data to SVG | Sanitised, deterministic renderer |
| Storage adapter | Store original/output objects and metadata | Local filesystem first; object store only if approved |
| Security package | Contract encryption and authentication | Vetted AEAD dependency; no custom cryptography |
| C sorting worker | Sort asset records by an agreed key | Standalone C executable using approved standard libraries |
| Review UI | Upload, inspect, sort, and retrieve | Minimal server-rendered or static client |

### 5.2 Data flow

    Terra or review UI
            │ HTTPS + contract envelope
            ▼
    API gateway / upload handler
            │
            ├── request and content validation
            ├── checksum and metadata extraction
            ├── processing policy selection
            │       ├── unchanged
            │       ├── lossless compression
            │       ├── colour/background representation
            │       └── stroke → SVG
            ├── storage adapter
            ├── C sorting worker for ordered listings
            └── encrypted response contract

### 5.3 Processing sequence

1. Receive an upload with a contract version and request ID.
2. Validate declared type, detected type, size, and content limits.
3. Compute a checksum and create an immutable asset ID.
4. Store the original or stage it for processing.
5. Select a deterministic processing policy.
6. Validate the output and compute its checksum.
7. Store output metadata and logical object references.
8. Return status, output metadata, and warnings in the response contract.

Failure rules:

- never overwrite the original silently;
- return a structured error with the request ID;
- preserve the input when classification confidence is insufficient;
- make retries idempotent using the request ID and checksum.

## 6. API surface

### 6.1 Endpoints

| Method | Path | Purpose |
| --- | --- | --- |
| POST | /v1/assets | Upload an asset and request processing |
| GET | /v1/assets/{assetId} | Read asset status and metadata |
| GET | /v1/assets/{assetId}/content | Retrieve an approved representation |
| GET | /v1/assets | List assets with C-backed sorting |
| GET | /healthz | Liveness check for the hosted service |

Initial request fields:

- contractVersion;
- requestId;
- kind: document or image;
- filename;
- contentType;
- processingMode;
- content stream or approved object reference.

Initial response fields:

- contractVersion;
- requestId;
- assetId;
- status;
- source and output checksums;
- source and output sizes;
- classification and processing decision;
- logical storage references;
- warnings and error codes.

## 7. Encrypted contracts

### 7.1 Contract design

- Keep the schema version independent from the implementation version.
- Authenticate every request and response.
- Encrypt sensitive payload fields using an authenticated-encryption scheme.
- Include a key identifier, nonce, ciphertext, and authentication tag.
- Keep keys outside Git and outside example responses.
- Support key rotation without changing the public contract shape.

Example envelope shape:

    {
      "contractVersion": "v1",
      "requestId": "request-id",
      "keyId": "review-key-2026-01",
      "algorithm": "AEAD-approved-by-tutor",
      "nonce": "base64...",
      "ciphertext": "base64...",
      "tag": "base64..."
    }

Recommended security boundary:

- use a vetted cryptography library if external dependencies are allowed;
- use TLS for transport;
- use environment-injected secrets or an approved secret manager;
- do not implement encryption algorithms for production claims;
- document the threat model and key lifecycle in the report.

## 8. Image processing policy

### 8.1 Classification rules

- Start with explicit rules, not an opaque classifier.
- Use a controlled fixture set for:
  - flat colour or palette data;
  - repeated or uniform backgrounds;
  - simple strokes;
  - unsupported or ambiguous images.
- Record the classification reason in metadata.
- Use unknown when no rule is decisive.

### 8.2 Representation decisions

| Classification | Default action | Fallback |
| --- | --- | --- |
| Colour data | Store a compact lossless representation | Preserve original |
| Background | Store a lossless background representation | Preserve original |
| Stroke | Convert sanitised geometry to SVG | Preserve original |
| Unknown | No transformation | Preserve original |

Constraints:

- no silent lossy conversion;
- SVG output must be sanitised and deterministic;
- original and derived checksums must both be recorded;
- tutor must confirm the exact meaning of “colour data”, “background”, and “stroke”.

## 9. C sorting integration

### 9.1 Worker contract

- Compile c/sort with a repository Makefile.
- Use only the libraries permitted for the assessed C implementation unless the tutor approves otherwise.
- Avoid a JSON dependency in the C worker.
- Exchange a delimiter-safe, versioned line protocol over standard input/output.
- Return sorted asset IDs and status codes; the API owns the full response schema.

### 9.2 Sorting behaviour

- Sort by one documented key per request:
  - filename;
  - content type;
  - byte size;
  - upload time;
  - asset ID.
- Define ascending/descending order.
- Define tie-breaking and missing-field behaviour.
- Test determinism and complexity.
- Expose sorting through GET /v1/assets?sort=filename&order=asc.

## 10. Review interface

The hosted interface should contain five simple states:

1. **Upload** — select a document or image and processing mode.
2. **Inspect** — show type, size, checksum, classification, and warnings.
3. **Process** — show the selected transformation and result status.
4. **Sort** — select a field and order; show C-sorted records.
5. **Retrieve** — show logical references for original and derived content.

Interface constraints:

- no complex dashboard;
- no hidden JavaScript-only processing state;
- display API errors and request IDs;
- make original versus derived content obvious;
- use a temporary review deployment only if approved.

## 11. Deployment model

### Prototype

- API and review UI run locally with the approved runtime.
- Local filesystem storage uses a generated data directory.
- Keys are supplied through local environment variables.
- Test fixtures are committed; uploads are not.

### Hosted review

- Build the API as a single service or container.
- Use approved object storage through the storage adapter.
- Inject secrets through the hosting platform.
- Restrict access with a review token if authentication is required.
- Configure size limits, rate limits, and request logging.
- Do not expose storage credentials to the browser.

## 12. Testing and acceptance

- Contract tests:
  - valid version;
  - missing fields;
  - incompatible version;
  - invalid authentication tag.
- Upload tests:
  - supported types;
  - wrong declared type;
  - over-limit content;
  - retry with the same request ID.
- Processing tests:
  - deterministic output;
  - compression/decompression round trip;
  - SVG sanitisation;
  - unknown-image fallback.
- Storage tests:
  - original preservation;
  - derived-object lookup;
  - checksum mismatch;
  - adapter failure.
- C tests:
  - sorting keys and directions;
  - ties and missing fields;
  - malformed line protocol;
  - debug mode and exit codes.
- Interface checks:
  - keyboard navigation;
  - readable status and errors;
  - no secret leakage;
  - desktop and mobile layout.

## 13. Tutor questions

### Scope

- Is this an acceptable real-world customer problem for Assessment Task 3?
- Is a local prototype sufficient, or is a hosted interface required?
- Should the first milestone cover both documents and images, or one asset class?
- Is stroke-to-SVG conversion in scope, or only classification and lossless storage?
- Must the original be retained for every derived asset?
- What formats, size limits, retention, and deletion rules are required?

### Dependencies

- Does the stdio, stdlib, string, and math restriction apply only to submitted C, or also to the API and review UI?
- May the API use an HTTP framework, serializer, image parser, SVG library, storage SDK, and cryptography library?
- May the review UI use Astro or another frontend framework?
- May the project use managed object storage?
- Must C sorting be group-written, or may it wrap a standard sorting routine?
- What dependency licenses and network services are acceptable?

### Security

- Does “encrypted contracts” mean repository files, API payloads, stored objects, or all three?
- Is authenticated encryption required?
- Are external, vetted cryptography libraries permitted?
- Is implementing cryptography from scratch explicitly prohibited?
- What threat model and key-management detail must appear in the report?

### Image semantics

- What evidence distinguishes colour data, a background, and a stroke?
- Which raster/vector formats and colour spaces are required?
- Should SVG preserve exact geometry or approximate appearance?
- Is an explicit unknown fallback acceptable?

### Assessment alignment

- Is the C worker allowed to be a supporting subsystem while the API handles transport and storage?
- Is sorting sufficient, or is searching also expected?
- Are linked lists, queues, trees, or other advanced structures required?
- Must the chosen compression support both compression and decompression?
- What constitutes an acceptable debug mode and report architecture diagram?

## 14. Recommended decisions pending review

- Keep terra-compression-api separate from Terra.
- Use lossless processing by default.
- Preserve originals and make derived output explicit.
- Use deterministic, rule-based image classification first.
- Use a vetted AEAD library if permitted; never implement cryptography from scratch.
- Start with local storage, then add an approved object-store adapter.
- Keep the hosted UI to upload, inspect, sort, and retrieve.
- Make C sorting a standalone, tested worker with a narrow protocol.
- Record every dependency, licence, assumption, and fallback.

## 15. Review exit criteria

The in-class review is complete when the team has:

- an approved problem statement;
- an agreed in/out-of-scope boundary;
- a dependency and hosting decision;
- a contract version and error model;
- an approved image fixture set;
- an encryption and key-management decision;
- a C sorting interface;
- milestones for prototype, tests, review, and report.
