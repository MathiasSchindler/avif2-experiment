# Security Policy

## Project Status and Scope

This repository is an experimental project for decoding AV2 still images
stored in HEIF files. It includes C code that reads HEIF files and AV2
bitstreams, turns the compressed data into images, and writes the decoded
output. It also includes reference and development tools. Input files may be
broken or deliberately crafted to expose bugs in the decoder.

Much of this project has been developed with help from large language models.
The project makes no promise that the code is correct, reliable, secure,
ready for production, or suitable for any particular purpose.

At this point, this software should not be used in production to process files
from people or sources you do not trust. Tests show that selected files decode
to exactly the same pixels as AVM. The project also uses strict compiler
settings, explicit buffer limits, checks that reject unsupported features, and
no standard C library. These are useful engineering properties, but they are
not a security review or proof of memory safety. A malformed file may still
cause wrong output, excessive resource use, a crash, or memory corruption.

## Decoder and Container Risks

Image files are compact but complex. They may also be deliberately designed to
trigger decoder bugs. A mistake in OBU framing, AV2 syntax parsing, HEIF/BMFF
box handling, buffer-size calculations, block geometry, prediction,
transforms, filtering, or output layout may become a security problem simply
by opening a file.

Relevant failure modes include:

- reading or writing outside a buffer, or numbers overflowing their limits
- incorrect use of memory supplied by the calling program, including buffers
  that are uninitialized, out of date, or overlapping
- excessive use of CPU time, stack space, working memory, or output space
- hangs or unexpectedly slow decoding
- accepting malformed, ambiguous, or unsupported data
- silent pixel corruption or disagreement between validation and decoding
- HEIF metadata that does not match the coded image or output layout

The program using the decoder provides its limits, working memory, and output
buffers. This reduces hidden memory allocation and makes memory ownership
clear, but it does not isolate the decoder from the rest of the program. When
testing files you do not trust, use a separate restricted environment with its
own memory and CPU limits.

The `spatial-still-8bit` release gate exercises strict workspace accounting,
one-byte-short rejection, maximum admitted geometry, malformed tile groups,
and deterministic mutation/truncation of every retained feature-family seed.
Compact seeds are truncated at every byte boundary; larger seeds use bounded
distributed truncations and mutations. These checks establish the documented
resource and failure contracts for tested tuples, not a denial-of-service or
memory-safety proof.

## Reference Tools and Artifacts

The hosted AVM checkout, Python development tools, generated test files, and
experimental viewers are separate from the freestanding decoder core. They
have their own dependencies and may read files or start other programs. They
have not received a production security review.

Recorded hashes and output comparisons help reproduce results for specific
files. They do not prove that a download is authentic, that the reference
decoder is correct, or that a file or program is safe. Please also report build
or release problems that replace, omit, or incorrectly describe generated
files.

## Reporting Vulnerabilities

Security reports and abuse-risk concerns are welcome via
[mathiasschindler@github.com](mailto:mathiasschindler@github.com).

Useful reports include:

- memory-safety bugs caused by AV2, OBU, or HEIF input
- errors involving numbers, buffer limits, memory lifetime, overlapping
  buffers, or working-memory checks
- excessive CPU, memory, or stack use, and hangs
- differences between parsing and decoding that bypass checks or limits
- malformed input that produces output instead of being rejected
- build, test-file, origin-tracking, or release issues that misrepresent files

When practical, please include the smallest file or set of steps that shows
the problem, the commit you tested, your platform and compiler, what happened,
and any sanitizer or debugger output. Do not include secrets or another
person's private data.

## Versions, Releases, and Advisories

This project does not currently publish formally supported releases. The
latest commit on the main branch is the most recent version, but it is not a
product version with promised security support.

The maintainer does not currently promise CVE numbers, formal security
advisories, fixes for older versions, or support for a set period of time.
Security-related changes may be documented in commits and repository history.

## Disclosure Expectations

Because this is a research project and not production software, reports do not
need to be kept confidential or unpublished for a set period. Reporters are
encouraged to notify the maintainer so a problem can be reproduced, understood,
fixed, or reduced. They are not asked to limit independent discussion or
publication of their findings.

Please do not use vulnerability information to attack systems, mislead users,
or cause harm to third parties.

## Response Expectations

This is an independent research project, so response times may vary. The
maintainer will make a reasonable effort to review clear reports, reproduce
issues where possible, and publish fixes or ways to reduce the risk when
appropriate.