---
title: Compression API proposal
description: "Technical design for a separate compression API supporting Terra Symposium assets."
---

> **Status:** Proposal for in-class review. Authored here; not part of the official Canvas specification.

## 1. Objective

| Objective | Details |
| --- | --- |
| Repository | Build a separate compression API for Terra Symposium documentation assets and content-storage images. |
| Review and assessment | Provide a hosted review UI and demonstrate C sorting through asset listings. |
| Tutor review | Get decisions on scope, dependencies, hosting, storage, and security before implementation. |

## 2. Problem statement

Terra needs a stable upload boundary for documentation and images. The API must validate uploads and metadata; safely compress supported assets; recognise colour data, backgrounds, and strokes; convert approved strokes to SVG; store originals and processed outputs; provide versioned, authenticated contracts; and sort asset records deterministically. The first release is a reviewable prototype, not a production media platform.

## 3. Business cases

### 3.1 Documentation assets

| Benefit | Details |
| --- | --- |
| Consistent uploads | One contract validates type, size, checksum, and metadata. |
| Stable identity | Asset IDs are independent of storage paths. |
| Separation | Compression is isolated from Terra's repository. |

### 3.2 Images in content storage

| Benefit | Details |
| --- | --- |
| Lower costs | Reduce storage and delivery costs for simple visual representations. |
| Traceable processing | Preserve originals and make transformations explicit. |
| Stroke handling | Convert recognised strokes to SVG; retain originals when classification is uncertain. |
| Safe conversion | Never apply lossy conversion silently. |

### 3.3 Review and assessment value

| Value | Details |
| --- | --- |
| Review | Hosted UI exposes the pipeline to tutors and reviewers. |
| Assessment | Demonstrates a real-world data-manipulation problem, supported compression/decompression, modular C, sorting (and optionally searching), testing, debugging, and design justification. |

### 3.4 Repository separation

| Benefit | Details |
| --- | --- |
| Independent evolution | Terra integrates through a versioned API contract; implementation and storage can evolve independently. |
| Isolated testing | API tests run without the Terra website; Terra imports no compression implementation directly. |

## 4. Scope

### 4.1 In scope

| Capability | Included work |
| --- | --- |
| Repository and API | Separate `terra-compression-api`; versioned upload/retrieval API for documentation and images. |
| Validation | Configurable file/request limits; MIME/type detection; checksum and metadata validation. |
| Processing | Lossless-first processing; explicit classification of colour data, backgrounds, and simple strokes; approved stroke-to-SVG conversion. |
| Storage and security | Original and processed object references; authenticated, encrypted contract envelope where required. |
| Review and sorting | C asset-record sorting; simple hosted review UI. |
| Verification | Contract, unit, integration, and round-trip tests. |

### 4.2 Out of scope

| Excluded | Boundary |
| --- | --- |
| Platform replacement | Replacing Terra content storage; general-purpose CDN, DAM, or image editor. |
| Unsupported media | Video, audio, animation, OCR, or generative image analysis; silent lossy conversion. |
| Production services | Public multi-tenant access or production SLA; accounts or billing. |
| Additional controls | Malware scanning or enterprise retention policy unless hosting requires them. |
| Deployment/security | Custom cryptography; permanent public deployment from this repository. |

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
    ├── c/sort/               standard-library-only C sorting worker
    ├── tests/                contract, integration, and fixture tests
    ├── infra/                local/hosted configuration; no secrets
    └── README.md

### 5.1 Runtime components

| Component | Responsibility | Initial implementation |
| --- | --- | --- |
| API service | HTTP, validation, orchestration, errors | Separate service in `apps/api` |
| Contract package | Versioned schemas and validation | JSON Schema or equivalent; dependency decision pending |
| Asset processor | Lossless compression and output metadata | Deterministic library-backed module |
| Image classifier | Supported representation classification | Explicit rules and fixtures |
| SVG converter | Approved stroke data to SVG | Sanitised deterministic renderer |
| Storage adapter | Original/output objects and metadata | Local filesystem first; object store only if approved |
| Security package | Contract encryption and authentication | Vetted AEAD dependency; no custom cryptography |
| C sorting worker | Sort asset records by agreed key | Standalone C executable; approved standard libraries |
| Review UI | Upload, inspect, sort, retrieve | Minimal server-rendered or static client |

### 5.2 Data flow

    Terra or review UI
            │ HTTPS + contract envelope
            ▼
    API gateway / upload handler
            ├── validate request and content
            ├── extract checksum and metadata
            ├── select policy: unchanged, lossless compression,
            │   colour/background representation, or stroke → SVG
            ├── store through adapter
            ├── sort listings through C worker
            └── return encrypted contract

### 5.3 Processing sequence

1. Receive upload with contract version and request ID.
2. Validate declared/detected type, size, and content limits.
3. Compute checksum and immutable asset ID.
4. Store original or stage it for processing.
5. Select deterministic processing policy; validate output and compute its checksum.
6. Store output metadata and logical object references.
7. Return status, output metadata, and warnings in the response contract.

Failure rules: never silently overwrite the original; return structured errors with request ID; preserve input when classification confidence is insufficient; make retries idempotent by request ID and checksum.

## 6. API surface

### 6.1 Endpoints

| Method | Path | Purpose |
| --- | --- | --- |
| POST | `/v1/assets` | Upload and process an asset |
| GET | `/v1/assets/{assetId}` | Read status and metadata |
| GET | `/v1/assets/{assetId}/content` | Retrieve an approved representation |
| GET | `/v1/assets` | List assets with C-backed sorting |
| GET | `/healthz` | Hosted-service liveness check |

Request fields: `contractVersion`, `requestId`, `kind` (`document` or `image`), `filename`, `contentType`, `processingMode`, and content stream or approved object reference.

Response fields: `contractVersion`, `requestId`, `assetId`, `status`, source/output checksums and sizes, classification and processing decision, logical storage references, warnings, and error codes.

## 7. Encrypted contracts

### 7.1 Contract design

| Requirement | Details |
| --- | --- |
| Versioning and authentication | Keep schema and implementation versions independent; authenticate every request and response. |
| Encryption | Encrypt sensitive payload fields with authenticated encryption; include key ID, nonce, ciphertext, and authentication tag. |
| Key handling | Keep keys out of Git and example responses; support rotation without changing the public contract shape. |

Example envelope:

    {
      "contractVersion": "v1",
      "requestId": "request-id",
      "keyId": "review-key-2026-01",
      "algorithm": "AEAD-approved-by-tutor",
      "nonce": "base64...",
      "ciphertext": "base64...",
      "tag": "base64..."
    }

Security boundary: use a vetted cryptography library if allowed, TLS in transit, and environment-injected secrets or an approved secret manager. Do not claim production security from custom encryption. Document the threat model and key lifecycle in the report.

## 8. Image processing policy

### 8.1 Classification rules

| Rule | Details |
| --- | --- |
| Explicit classification | Begin with rules, not an opaque classifier. Use controlled fixtures for flat colour/palette data, repeated/uniform backgrounds, simple strokes, and unsupported/ambiguous images. |
| Uncertain results | Record the reason in metadata; classify as unknown when no rule is decisive. |

### 8.2 Representation decisions

| Classification | Default action | Fallback |
| --- | --- | --- |
| Colour data | Compact lossless representation | Preserve original |
| Background | Lossless background representation | Preserve original |
| Stroke | Convert sanitised geometry to SVG | Preserve original |
| Unknown | No transformation | Preserve original |

Constraints: no silent lossy conversion; sanitised, deterministic SVG; record original and derived checksums. Tutor must define “colour data”, “background”, and “stroke”.

## 9. C sorting integration

### 9.1 Worker contract

| Requirement | Details |
| --- | --- |
| Build and libraries | Compile `c/sort` with a repository Makefile; use only libraries allowed for assessed C unless approved. |
| Protocol | Avoid JSON dependency. Use a delimiter-safe, versioned stdin/stdout line protocol. |
| Responsibility | Return sorted asset IDs and status codes; API owns the full response schema. |

### 9.2 Sorting behaviour

| Requirement | Details |
| --- | --- |
| Sort key | Sort each request by one documented key: filename, content type, byte size, upload time, or asset ID. |
| Defined behaviour | Define direction, tie-breaking, and missing-field behaviour; test determinism and complexity. |
| API | Expose via `GET /v1/assets?sort=filename&order=asc`. |

## 10. Review interface

Five states: **Upload** (choose document/image and mode); **Inspect** (type, size, checksum, classification, warnings); **Process** (transformation and result status); **Sort** (field/order and C-sorted records); **Retrieve** (logical references for original and derived content).

Constraints: simple UI, no complex dashboard or hidden JavaScript-only processing state; display API errors and request IDs; distinguish original from derived content; temporary review deployment only if approved.

## 11. Deployment model

### Prototype

| Requirement | Prototype |
| --- | --- |
| Runtime and storage | Run API and UI locally with approved runtime; store locally in a generated data directory. |
| Keys and data | Supply keys through local environment variables. Commit fixtures, never uploads. |

### Hosted review

| Requirement | Hosted review |
| --- | --- |
| Runtime and storage | Build API as one service or container; use approved object storage via adapter. |
| Secrets and access | Inject secrets through hosting platform; restrict access with a review token if required. |
| Limits and credentials | Configure size/rate limits and request logging; never expose storage credentials to the browser. |

## 12. Testing and acceptance

| Area | Acceptance checks |
| --- | --- |
| Contracts | Valid and incompatible versions; missing fields; invalid authentication tag. |
| Uploads | Supported types; wrong declared type; over-limit content; same-request-ID retry. |
| Processing | Deterministic output; compression/decompression round trip; SVG sanitisation; unknown-image fallback. |
| Storage | Original preservation; derived-object lookup; checksum mismatch; adapter failure. |
| C | Keys and directions; ties and missing fields; malformed line protocol; debug mode and exit codes. |
| Interface | Keyboard navigation; readable status/errors; no secret leakage; desktop/mobile layout. |

## 13. Tutor questions

| Topic | Questions for the tutor |
| --- | --- |
| Scope | Is this an acceptable real-world customer problem for Assessment Task 3? Is a local prototype enough, or is hosted UI required? |
| Scope | Should milestone one cover both asset classes or one? |
| Scope | Is stroke-to-SVG in scope, or only classification and lossless storage? |
| Scope | Must every derived asset retain its original? |
| Scope | What formats, size limits, retention, and deletion rules apply? |
| Dependencies | Does the stdio/stdlib/string/math restriction apply only to submitted C, or also API and UI? |
| Dependencies | May the API use an HTTP framework, serializer, image parser, SVG library, storage SDK, and cryptography library? |
| Dependencies | May the UI use Astro or another frontend framework? Is managed object storage allowed? |
| Dependencies | Must C sorting be group-written, or may it wrap a standard sort? |
| Dependencies | Which dependency licences and network services are acceptable? |
| Security | Does “encrypted contracts” cover repository files, API payloads, stored objects, or all three? Is authenticated encryption required? |
| Security | Are vetted external cryptography libraries allowed? Is implementing cryptography from scratch prohibited? |
| Security | What threat model and key-management detail must the report include? |
| Image semantics | What evidence distinguishes colour data, backgrounds, and strokes? |
| Image semantics | Which raster/vector formats and colour spaces are required? |
| Image semantics | Should SVG preserve exact geometry or approximate appearance? |
| Image semantics | Is an explicit unknown fallback acceptable? |
| Assessment alignment | May C be a supporting subsystem while the API handles transport and storage? |
| Assessment alignment | Is sorting enough, or is searching expected? |
| Assessment alignment | Are linked lists, queues, trees, or other advanced structures required? |
| Assessment alignment | Must compression also support decompression? |
| Assessment alignment | What debug mode and report architecture diagram are acceptable? |

## 14. Recommended decisions pending review

| Decision | Recommendation |
| --- | --- |
| Repository | Keep `terra-compression-api` separate from Terra. |
| Processing | Default to lossless processing; preserve originals and make derived outputs explicit. |
| Classification | Begin with deterministic, rule-based image classification. |
| Cryptography | Use vetted AEAD if allowed; never implement cryptography from scratch. |
| Storage | Start locally; add object-store adapter only if approved. |
| Review UI | Limit to upload, inspect, sort, retrieve. |
| C sorting | Keep standalone, tested, and behind a narrow protocol. |
| Documentation | Record every dependency, licence, assumption, and fallback. |

## 15. Review exit criteria

Review is complete when the team has agreed on:

| Decision required | Agreed when review ends |
| --- | --- |
| Problem and scope | Problem statement and in/out-of-scope boundary. |
| Delivery | Dependencies and hosting. |
| Contract | Contract version and error model. |
| Image processing | Image fixture set. |
| Security | Encryption and key management. |
| C sorting | C sorting interface. |
| Milestones | Prototype, test, review, and report milestones. |
