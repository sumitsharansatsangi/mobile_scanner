import 'dart:math' as math;
import 'dart:ui';

import 'package:mobile_scanner/src/objects/barcode.dart';

/// Returns true if the bounding box of [barcode] (in raw camera pixel
/// coordinates) is fully contained within [scanWindow].
///
/// [scanWindow] is a normalized [Rect] (values in [0, 1]) relative to the
/// camera texture. The barcode corners are normalized by dividing by
/// [videoSize] before comparing.
///
/// This check must be performed on the **raw** (pre-mirror) barcode corners
/// so that the coordinate spaces align.
///
/// Always returns true when [scanWindow] is null or [videoSize] is zero.
bool isInsideScanWindow(Barcode barcode, Rect? scanWindow, Size videoSize) {
  if (scanWindow == null) {
    return true;
  }

  final corners = barcode.corners;

  if (corners.length != 4) {
    return true;
  }

  final vw = videoSize.width;
  final vh = videoSize.height;

  if (vw <= 0 || vh <= 0) {
    return true;
  }

  final minX = corners.map((c) => c.dx).reduce(math.min) / vw;
  final maxX = corners.map((c) => c.dx).reduce(math.max) / vw;
  final minY = corners.map((c) => c.dy).reduce(math.min) / vh;
  final maxY = corners.map((c) => c.dy).reduce(math.max) / vh;
  final barcodeRect = Rect.fromLTRB(minX, minY, maxX, maxY);

  return barcodeRect.left >= scanWindow.left &&
      barcodeRect.top >= scanWindow.top &&
      barcodeRect.right <= scanWindow.right &&
      barcodeRect.bottom <= scanWindow.bottom;
}

/// Returns the size of the axis-aligned bounding box of [corners].
///
/// Returns [Size.zero] when [corners] does not contain exactly four points.
Size computeBoundingBoxSize(List<Offset> corners) {
  if (corners.length != 4) {
    return Size.zero;
  }

  final xs = corners.map((c) => c.dx);
  final ys = corners.map((c) => c.dy);

  return Size(
    xs.reduce(math.max) - xs.reduce(math.min),
    ys.reduce(math.max) - ys.reduce(math.min),
  );
}

/// Returns a copy of [barcode] with all corner x-coordinates mirrored
/// relative to [videoWidth].
Barcode mirrorBarcodeX(Barcode barcode, double videoWidth) {
  final corners = barcode.corners;

  if (corners.isEmpty) {
    return barcode;
  }

  // Mirror each x-coordinate.
  final mirrored = corners.map((c) => Offset(videoWidth - c.dx, c.dy)).toList();

  // Mirroring x reverses the clockwise winding order from
  // [TL, TR, BR, BL] to [TR_m, TL_m, BL_m, BR_m].
  // Swap TL↔TR and BL↔BR to restore [TL_m, TR_m, BR_m, BL_m].
  final reordered = mirrored.length == 4
      ? [mirrored[1], mirrored[0], mirrored[3], mirrored[2]]
      : mirrored;

  return Barcode(
    calendarEvent: barcode.calendarEvent,
    contactInfo: barcode.contactInfo,
    driverLicense: barcode.driverLicense,
    email: barcode.email,
    geoPoint: barcode.geoPoint,
    phone: barcode.phone,
    sms: barcode.sms,
    url: barcode.url,
    wifi: barcode.wifi,
    corners: reordered,
    format: barcode.format,
    displayValue: barcode.displayValue,
    rawDecodedBytes: barcode.rawDecodedBytes,
    rawValue: barcode.rawValue,
    size: barcode.size,
    type: barcode.type,
  );
}

/// Orders four corners clockwise from the top-left without mutating the input.
List<Offset> orderBarcodeCornersClockwise(List<Offset> points) {
  if (points.length != 4) return List.of(points);
  final center = points.reduce((a, b) => a + b) / 4;
  final ordered = List<Offset>.of(points)
    ..sort(
      (a, b) => math
          .atan2(a.dy - center.dy, a.dx - center.dx)
          .compareTo(math.atan2(b.dy - center.dy, b.dx - center.dx)),
    );
  var first = 0;
  for (var i = 1; i < ordered.length; i++) {
    final candidate = ordered[i];
    final current = ordered[first];
    final difference = candidate.dx + candidate.dy - current.dx - current.dy;
    if (difference < 0 || (difference == 0 && candidate.dy < current.dy)) {
      first = i;
    }
  }
  return [...ordered.skip(first), ...ordered.take(first)];
}
