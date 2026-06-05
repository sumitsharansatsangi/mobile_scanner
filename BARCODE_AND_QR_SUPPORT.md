# Barcode and QR Code Support

This document explains which barcode formats this project supports, which ones
it does not support, where each format is normally used, and the main pros and
cons of each format.

The project exposes barcode symbologies through `BarcodeFormat` and decoded
content categories through `BarcodeType`.

## Engine Summary

`mobile_scanner` uses multiple scanner engines:

| Platform | Primary engine | Fallback engine | Notes |
| --- | --- | --- | --- |
| Android | ZXing-C++ | ML Kit | ZXing-C++ covers extra formats such as GS1 DataBar, MaxiCode, DotCode, Code 11, MSI/Plessey, and Pharmacode. |
| iOS | Apple Vision, or ZXing-C++ when enabled | Vision fallback when ZXing is enabled | MaxiCode, DotCode, Code 11, MSI/Plessey, and Pharmacode require the optional `MOBILE_SCANNER_ZXING` build path. |
| macOS | Apple Vision, or ZXing-C++ when enabled | Vision fallback when ZXing is enabled | Same format notes as iOS. |
| Web | ZXing-js | None | DotCode, Code 11, MSI/Plessey, and Pharmacode are not supported on web. |
| Linux / Windows | ZXing-C++ | None | Image decoding is supported; live camera preview is not available in this project. |

## Supported Symbologies

| Format | Supported | Type | Common purpose | Why it is used | Pros | Cons |
| --- | --- | --- | --- | --- | --- | --- |
| QR Code | Yes | 2D | URLs, payments, tickets, app deep links, contact cards, WiFi setup, general data sharing. | QR is the most widely recognized 2D code and is easy for phones to scan. | High adoption, good error correction, stores more data than 1D barcodes, works well with mobile cameras. | Larger visual footprint than some 2D codes, can be abused for phishing links, dense QR codes become harder to scan. |
| Data Matrix | Yes | 2D | Small product labels, electronics, medical devices, industrial marking. | It stores useful data in a very small area and works well for direct part marking. | Compact, strong error correction, good for tiny surfaces. | Less familiar to consumers than QR, not ideal for public URL sharing. |
| PDF417 | Yes | 2D stacked | Driver licenses, boarding passes, shipping labels, government documents. | It stores larger structured payloads in a printable rectangular barcode. | High capacity, robust for documents, common in ID workflows. | Wide shape needs more label space, not as consumer-friendly as QR. |
| Aztec | Yes | 2D | Transport tickets, boarding passes, mobile tickets. | It scans well without a quiet zone and is good for compact ticket layouts. | Compact, no quiet zone required, good error correction. | Less common than QR, fewer consumer tools recognize it. |
| MaxiCode | Yes where ZXing-C++ is active | 2D | Parcel and logistics sorting, especially shipping labels. | Designed for high-speed package sorting systems. | Fast logistics scanning, fixed-size symbol, good for conveyor environments. | Specialized use, not supported by ML Kit or Apple Vision; relies on ZXing path. |
| DotCode | Yes where ZXing-C++ is active, except web | 2D dot pattern | High-speed printing, tobacco/pharma packaging, industrial coding. | Useful where dot-matrix or high-speed production printing is used. | Works with dot-based printing, compact, industrial-friendly. | Not supported on web ZXing-js, not supported by ML Kit or Apple Vision, less common for general apps. |
| Code 128 | Yes | 1D | Shipping, inventory, asset tracking, serial numbers. | It encodes dense alphanumeric data and is common in logistics. | High density for 1D, supports full ASCII, widely supported. | Needs a straight scan line, stores less data than 2D codes. |
| GS1-128 / UCC/EAN-128 | Yes where ZXing-C++ is active | 1D GS1 Code 128 | GS1 application identifiers for cartons, logistics, healthcare, and retail supply-chain labels. | Encodes GS1 data using Code 128 with FNC1 in the first position. | Distinguishes GS1-128 from plain Code 128 on native ZXing platforms. | Physically a Code 128 symbol; ML Kit, Apple Vision, and web may report it as plain Code 128. |
| Code 39 | Yes | 1D | Industrial labels, badges, inventory, older systems. | Simple and widely implemented. | Easy to print, broad legacy support. | Low density, larger labels, limited character set unless extended variants are used. |
| Code 93 | Yes | 1D | Inventory, logistics, compact labels. | Similar to Code 39 but denser and more reliable. | Better density than Code 39, checksum support. | Less common than Code 128. |
| Code 11 | Yes where the native ZXing-C++ bridge is active | 1D | Telecommunications equipment labels and older industrial systems. | Compact numeric/dash encoding for legacy telecom workflows. | Dense for its small character set; checksum-protected. | Not supported by ML Kit, Apple Vision, or ZXing-js web; native fallback uses sampled scan lines and requires valid checksum characters. |
| MSI / MSI Plessey | Yes where the native ZXing-C++ bridge is active | 1D | Inventory, warehouse shelves, stock locations, and older retail systems. | Numeric-only legacy format with common checksum variants. | Variable length, simple legacy workflows, Mod-10/Mod-11 checksum support. | Not supported by ML Kit, Apple Vision, or ZXing-js web; native fallback requires a valid Mod-10, Mod-10/10, Mod-11, or Mod-11/10 checksum. |
| Pharmacode One-Track | Yes where the native ZXing-C++ bridge is active, when explicitly requested | 1D | Pharmaceutical packaging verification. | Encodes one internal control number in narrow/wide bars. | Compact, designed for packaging-control workflows. | No checksum; detector is explicit-only to avoid false positives; not supported by ML Kit, Apple Vision, or ZXing-js web. |
| Pharmacode Two-Track | Yes where the native ZXing-C++ bridge is active, when explicitly requested | Two-track 1D | Pharmaceutical packaging verification where shorter symbols are needed. | Encodes one internal control number using upper/lower/full bars. | Higher capacity than one-track in a short symbol. | No checksum; detector is explicit-only to avoid false positives; not supported by ML Kit, Apple Vision, or ZXing-js web. |
| Codabar | Yes | 1D | Libraries, blood banks, legacy medical/logistics systems. | Simple format used by older domain-specific systems. | Easy to print, works with older workflows. | Limited character set, legacy-oriented. |
| EAN-13 | Yes | 1D | Retail product barcodes worldwide. | Standard product identifier for retail goods. | Universal retail support, compact, reliable. | Numeric only, fixed length, product identity only. |
| EAN-8 | Yes | 1D | Small retail products. | Shorter version for packages too small for EAN-13. | Compact retail code. | Numeric only, limited namespace. |
| UPC-A | Yes | 1D | Retail products, mainly North America. | Standard retail barcode for consumer goods. | Very widely supported in retail. | Numeric only, fixed length, product identity only. |
| UPC-E | Yes | 1D | Small retail products, mainly North America. | Compressed UPC-A for small packaging. | Smaller than UPC-A. | Numeric only, limited to compressible UPC values. |
| UPC/EAN extension | Yes where ZXing-C++ or ZXing-js is active | 1D add-on | Magazines, books, coupons, and retail items that use EAN-2 or EAN-5 supplements. | Adds issue or price metadata beside an EAN/UPC symbol. | Supports common 2-digit and 5-digit retail add-ons. | Not a standalone product code; native ZXing reports the main EAN/UPC format and appends the add-on value to the decoded text; not supported by ML Kit or Apple Vision. |
| ITF / ITF-14 | Yes | 1D | Cartons, cases, warehouse packaging. | Reliable for printing on corrugated boxes and outer packaging. | Good for logistics cartons, tolerant of rough printing. | Numeric only, often needs bearer bars, not for rich data. |
| Interleaved 2 of 5 | Yes, normalized to ITF | 1D | Warehousing, industrial numeric codes. | Legacy numeric-only format for dense 1D labels. | Dense numeric encoding. | Numeric only; this project maps ITF variants to a single ZXing ITF format. |
| Interleaved 2 of 5 with checksum | Yes, normalized to ITF | 1D | Same as ITF, with checksum expectation in some systems. | Used when the workflow expects a checksum-protected ITF value. | Adds validation in systems that enforce checksum. | ZXing path reports it as the shared ITF format, so the variant is not distinguished in decoded output. |
| GS1 DataBar / RSS-14 | Yes | 1D | Retail coupons, fresh food, healthcare, small item marking. | Encodes GS1 data in smaller spaces than classic EAN/UPC. | Compact GS1 support, useful for variable measure items. | Less familiar than EAN/UPC; not supported by Android ML Kit fallback. |
| GS1 DataBar Expanded / RSS Expanded | Yes | 1D | Coupons, variable weight products, expiry/lot data. | Can encode richer GS1 application identifier data. | More data than basic DataBar, GS1-compatible. | Larger than basic DataBar, not supported by Android ML Kit fallback. |

## Not Supported Symbologies

These formats are not exposed by this project because the integrated engines do
not provide dependable support for them.

Some C++20 helper code for these formats lives in
`src/ms_fallback_barcodes.{h,cpp}`. Those helpers decode or validate
already-normalized symbol streams. Code 11, MSI/Plessey, and Pharmacode are the
exceptions: their helpers are wired into native sampled-line detectors.

| Format | Supported | Why not supported | Typical purpose |
| --- | --- | --- | --- |
| POSTNET | Not exposed | Native helper decodes normalized full/half bars; postal image detection is not wired into the scanner. | USPS postal routing. |
| PLANET | Not exposed | Native helper decodes normalized full/half bars; postal image detection is not wired into the scanner. | USPS postal tracking, now legacy. |
| Intelligent Mail Barcode / IMb | No | Postal barcode formats are not supported by the integrated engines. | USPS mail tracking and routing. |
| Australia Post / Royal Mail / other postal variants | No | Postal barcode formats are not supported by the integrated engines. | Postal sorting and routing. |

## Supported QR / Barcode Content Types

`BarcodeFormat` tells you the physical barcode symbology, such as `qrCode` or
`code128`. `BarcodeType` tells you what the decoded value appears to contain,
such as a URL, WiFi credentials, or contact details.

These content types are supported by the Dart model:

| Content type | Supported | Often encoded in | Purpose | Pros | Cons |
| --- | --- | --- | --- | --- | --- |
| Text | Yes | QR Code, Data Matrix, PDF417, Aztec, 1D formats | Plain text, IDs, internal values. | Flexible and simple. | App must decide how to interpret it. |
| URL / bookmark | Yes | QR Code | Open websites, deep links, campaign links, docs. | Very convenient for mobile users. | Security risk if users cannot inspect destination; links can expire. |
| WiFi | Yes | QR Code | Share network SSID, password, and encryption type. | Fast network onboarding, avoids typing passwords. | Exposes credentials to anyone who can scan the code. |
| Email | Yes | QR Code | Pre-fill recipient, subject, and body. | Reduces typing and input mistakes. | Depends on mail app support; long body text can make QR dense. |
| Phone | Yes | QR Code | Dial a phone number. | Simple call-to-action. | Can be abused for unwanted calls; limited data. |
| SMS | Yes | QR Code | Pre-fill SMS number and message. | Useful for opt-in, support, or short workflows. | Depends on device SMS support; not ideal for long messages. |
| Contact info | Yes | QR Code | vCard-style person or business contact sharing. | Easy address-book import. | Large contact cards create dense QR codes. |
| Calendar event | Yes | QR Code | Add meetings, appointments, ticket times. | Good for event onboarding. | Time zone and recurrence details can be tricky. |
| Geo coordinates | Yes | QR Code | Share map locations. | Simple location sharing. | Coordinates alone lack context; precision/privacy concerns. |
| ISBN | Yes | EAN-13, QR Code text | Identify books. | Standard book identifier. | Mostly useful only with lookup systems. |
| Product | Yes | EAN, UPC, GS1 DataBar | Retail product identification. | Retail-standard and fast to scan. | Usually only an identifier; needs a product database. |
| Driver license | Yes | PDF417, sometimes QR depending on region | Identity document data. | High-capacity structured ID data. | Sensitive personal data; parsing varies by jurisdiction. |
| Unknown | Yes | Any | Fallback when the scanner cannot classify content. | Preserves raw value for custom parsing. | No structured helper fields are available. |

## Why QR Code Is Commonly Used

QR Code is the default choice for many mobile apps because it balances capacity,
scan reliability, and public familiarity. It supports numeric, alphanumeric,
byte, and Kanji-oriented data modes, plus error correction. In practical app
workflows, QR Code is usually chosen when humans will scan with a phone camera.

Use QR Code when:

- Users need to scan from a phone.
- The value is a URL, app link, WiFi setup, contact card, ticket, or payment reference.
- You need more data than a 1D barcode can comfortably hold.
- The printed code may be partially damaged and error correction matters.

Avoid QR Code when:

- The workflow is retail POS product identification, where EAN/UPC is the standard.
- The workflow is carton logistics, where ITF-14 or Code 128 may be expected.
- The printed area is extremely small and Data Matrix is a better fit.
- The environment already requires a specialized format such as PDF417 for IDs or MaxiCode for parcel sorting.

## Practical Recommendations

| Use case | Recommended format |
| --- | --- |
| Public website link or app deep link | QR Code |
| WiFi sharing | QR Code |
| Retail product barcode | EAN-13, EAN-8, UPC-A, UPC-E |
| Shipping label or inventory ID | Code 128 |
| Outer carton / case barcode | ITF-14 |
| Driver license or large document payload | PDF417 |
| Tiny industrial product marking | Data Matrix |
| Transit or mobile ticket | Aztec or QR Code |
| Parcel sorting with MaxiCode workflows | MaxiCode |
| GS1 coupon or expanded product metadata | GS1 DataBar Expanded |

## Important Limitations

- Platform support is not identical across every engine. DotCode is not
  supported on web; Code 11, MSI/Plessey, and Pharmacode are also unavailable
  on web.
- MaxiCode, DotCode, GS1-128 identification, Code 11, MSI/Plessey,
  Pharmacode, and UPC/EAN extension support rely on the native bridge; on Apple
  platforms that path is optional and must be enabled at build time.
- Android ML Kit does not support GS1 DataBar, GS1 DataBar Expanded, MaxiCode,
  DotCode, Code 11, MSI/Plessey, or Pharmacode; the project handles those
  through native decoding.
- Pharmacode has no checksum, so it is decoded only when the caller explicitly
  requests `BarcodeFormat.pharmaCode` or `BarcodeFormat.pharmaCodeTwoTrack`.
- UPC/EAN add-ons can be required by requesting
  `BarcodeFormat.upcEanExtension`. On native ZXing platforms, the scan result
  format remains the main `ean13`, `ean8`, `upcA`, or `upcE` format and the
  decoded text includes the add-on after a space.
- `BarcodeType` classification can be `unknown` even when the barcode format is
  decoded correctly. In that case, use `rawValue` or `rawDecodedBytes` and parse
  it yourself.
- Some formats encode only identifiers. For example, EAN/UPC usually gives a
  product number, not the product name; you need an external database to resolve
  it.
