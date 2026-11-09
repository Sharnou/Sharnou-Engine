# Sharnou File Format Governance

Status: Approved initial format set
Format policy version: 1.0
Canonical path: `Docs/FileFormats.md`

This document is the canonical governance contract for native Sharnou source and project files and the approved interoperable asset formats. Its contents must remain byte-for-byte identical in these repositories:

- `Sharnou/Sharnou-Compiler`
- `Sharnou/Sharnou-IDE`
- `Sharnou/Sharnou-Engine`
- `Sharnou/Honour-War`

## 1. Approved native formats

| Extension | Purpose | Encoding / structure |
|---|---|---|
| `.sharnou` | Sharnou source code | UTF-8 without BOM; Sharnou language syntax |
| `.sharnouproj` | Project and IDE configuration | Strict JSON |
| `.sharnoumeta` | Metadata and resource properties | Strict JSON |
| `.sharnouasset` | Asset descriptors and manifests | Strict JSON |

No additional native extension is approved by this policy. Adding an extension requires explicit approval before it is accepted by a compiler, IDE, Engine, asset pipeline, or repository policy.

## 2. Approved interoperable asset formats

| Extension | Representation | Intended role |
|---|---|---|
| `.gltf` | Text JSON | Interoperable 3D scene description |
| `.glb` | Binary | Binary glTF scene container |
| `.ktx2` | Binary | GPU texture container |
| `.avif` | Binary | Image and UI visual asset |

The formats retain their own specifications and semantics. A tool must not reinterpret one format as another or silently convert assets during validation.

## 3. Text encoding and line endings

All four native Sharnou text formats (`.sharnou`, `.sharnouproj`, `.sharnoumeta`, and `.sharnouasset`) must:

1. Be valid UTF-8 without a byte-order mark (BOM).
2. Use LF (`0A`) line endings only.
3. End with exactly one final LF newline.
4. Contain no CR (`0D`) bytes.

The governance document itself follows the same UTF-8-no-BOM, LF-only, one-final-newline rule.

`.gltf` is text JSON and must be decoded as UTF-8 according to the glTF format requirements. The binary formats `.glb`, `.ktx2`, and `.avif` must be treated as binary; newline conversion or text decoding must never be applied to them.

## 4. Strict JSON requirements

`.sharnouproj`, `.sharnoumeta`, and `.sharnouasset` must be parsed as strict JSON. A parser must reject:

- Invalid UTF-8, a UTF-8 BOM, comments, and trailing commas.
- Duplicate object member names.
- `NaN`, positive or negative infinity, and other non-JSON numeric values.
- Invalid string escapes or malformed Unicode.
- A root value other than a JSON object.
- Missing, malformed, or unsupported `formatVersion` values.
- Unknown required fields or values that violate the applicable schema.

The initial supported `formatVersion` is the string `"1.0"`. The major version is the integer before the first dot. A consumer must fail closed on an unsupported major version and must not guess, downgrade, or silently reinterpret the document. An unsupported minor version must also be rejected until that minor version is explicitly supported.

A `.sharnou` source file must begin with the source-format directive `// formatVersion: 1`. This directive is a format header, not an executable statement. Source consumers must reject a missing, malformed, or unsupported major version before compiling the program.

## 5. Relative path policy

Every external path stored in a project, metadata, asset manifest, or asset reference must be relative to its owning project or manifest root.

Reject:

- Drive-qualified and rooted paths (for example, `C:\\asset.glb` or `/asset.glb`).
- UNC paths and network-share paths.
- Absolute URI schemes used as external file paths.
- Any path containing a `..` traversal segment.
- Paths that resolve outside the owning root after normalization.
- Paths whose final resolved target escapes the root through a symbolic link, junction, or equivalent filesystem redirection.

Normalize separators before validation. Validation must account for percent-encoded separators or traversal components wherever URI decoding is part of the format. Inline data URIs, when allowed by an interoperable format, are embedded payloads and are not external filesystem paths.

## 6. Binary asset integrity

Every referenced external binary asset must have a SHA-256 digest recorded in its owning `.sharnouasset` manifest. This includes binary scene containers, texture containers, image assets, and external binary buffers referenced by a text scene.

The digest must be exactly 64 hexadecimal characters. Consumers must compute SHA-256 over the actual file bytes and compare it with the declared digest before loading or using the asset. Missing digests, unreadable files, malformed digests, and mismatches are hard failures. Do not use file names, timestamps, sizes, or cached validation results as substitutes for the hash check.

A manifest entry must identify the relative `path`, declared `format`, and `sha256` for each external binary asset. The extension and declared format must agree.

## 7. Fail-closed validation

Format validation is a gate, not a repair operation. On invalid encoding, invalid JSON, unsupported versions, path-policy violations, missing assets, or hash mismatches, the consumer must report a diagnostic and reject the input.

A validator must not rewrite line endings, remove a BOM, repair JSON, normalize a traversal path into an accepted path, convert an asset, or replace a failed hash with a newly computed value. Repairs require an explicit authoring action followed by validation.

Each accepted format must have both positive tests and negative tests. Positive tests must cover valid encoding, structure, paths, versions, and integrity data. Negative tests must cover malformed encoding, invalid syntax or structure, unsupported versions, path traversal and absolute paths, missing references, and hash mismatches as applicable to that format.

## 8. Repository and generated-artifact policy

Generated executables, libraries, object files, debug symbols, and comparable build outputs (including `.exe`, `.dll`, `.pdb`, `.obj`, and `.lib`) are excluded from normal source control through repository ignore rules. Source code, documentation, test fixtures, reference data, and explicitly tracked manifests remain protected.

No Git LFS configuration is part of this initial governance change.

`.gitattributes` may declare explicit rules for the approved text and binary extensions. It must not introduce a global `* text=auto` rule and must not normalize PowerShell (`.ps1`) files.

## 9. Change control and mirrors

Only the four native extensions listed in Section 1 are approved native Sharnou formats. Any additional custom extension requires explicit approval.

This file is canonical at `Docs/FileFormats.md`. Its bytes must be identical in all four repositories listed at the start of this document. Update the four mirrors together and verify byte-for-byte equality before committing a governance change.
