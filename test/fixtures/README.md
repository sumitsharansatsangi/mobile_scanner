mecard.png is zxing-cpp v2.3.0 test/samples/qrcode-5/15.png, distributed under
Apache-2.0 (https://github.com/zxing-cpp/zxing-cpp/blob/v2.3.0/LICENSE).
mecard.pgm is the same image converted to grayscale PGM for native tests,
so the C ABI regression needs no additional image library.
These fixtures contain a public sample MECARD contact from that test suite.

rmqr.png, rmqr_inverted.png and rmqr_large.png are the rMQR fixtures R7x43-H,
R7x43-H_inverted and R17x139 from the same upstream release's rmqrcode-1 suite.
rmqr.pgm is the first fixture converted to grayscale. License: Apache-2.0.
rmqr_rotated.png is the small rMQR fixture rotated 90 degrees without resampling.


formats/manifest.json lists a real fixture for every concrete format in the
pinned native ZXing-C++ 2.3.0 header, with original sample paths and expected
text. PNG files were converted to grayscale from that public upstream corpus
under the same Apache-2.0 repository license. Additional source fixtures:
- formats/qrModel1.png: qrcode-2/qr-model-1.png
- formats/gs1Code128.png: code128-1/1.png
- formats/eanAddOn.png: ean13-extension-1/1.png

qr_unicode.png was generated locally with ZXing-C++'s QR writer for the text
"Scanner ✓ 中文 😀". formats/twentyQr.png tiles this QR on a 5×4 grid at 2× scale.
wasm_readable_formats.json records the 41 readable names in the pinned
zxing-wasm v3.1.3 barcodeFormat.ts table; meta selectors and unreadable standalone
EAN2/EAN5 are excluded. It supports exhaustive mapping checks without a network
request during tests.
