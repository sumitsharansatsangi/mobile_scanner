import 'dart:ui';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:mobile_scanner/src/web/web_barcode_utils.dart';

void main() {
  test('corner ordering starts at top-left and preserves the caller list', () {
    final points = [
      const Offset(10, 10),
      Offset.zero,
      const Offset(0, 10),
      const Offset(10, 0),
    ];
    final original = List<Offset>.of(points);
    expect(orderBarcodeCornersClockwise(points), [
      Offset.zero,
      const Offset(10, 0),
      const Offset(10, 10),
      const Offset(0, 10),
    ]);
    expect(points, original);
  });

  test('mirroring retains structured payloads and restores corner winding', () {
    const barcode = Barcode(
      corners: [Offset(10, 20), Offset(30, 20), Offset(30, 40), Offset(10, 40)],
      format: BarcodeFormat.qrCode,
      rawValue: 'MECARD:N:Example;;',
      type: BarcodeType.contactInfo,
      contactInfo: ContactInfo(organization: 'Example'),
      calendarEvent: CalendarEvent(summary: 'Meeting'),
      driverLicense: DriverLicense(firstName: 'Example'),
      email: Email(address: 'scanner@example.com'),
      geoPoint: GeoPoint(latitude: 1, longitude: 2),
      phone: Phone(number: '1234'),
      sms: SMS(phoneNumber: '1234', message: 'Hello'),
      url: UrlBookmark(url: 'https://example.com'),
      wifi: WiFi(ssid: 'Example'),
    );

    final mirrored = mirrorBarcodeX(barcode, 100);

    expect(mirrored.corners, const [
      Offset(70, 20),
      Offset(90, 20),
      Offset(90, 40),
      Offset(70, 40),
    ]);
    expect(mirrored.contactInfo, same(barcode.contactInfo));
    expect(mirrored.calendarEvent, same(barcode.calendarEvent));
    expect(mirrored.driverLicense, same(barcode.driverLicense));
    expect(mirrored.email, same(barcode.email));
    expect(mirrored.geoPoint, same(barcode.geoPoint));
    expect(mirrored.phone, same(barcode.phone));
    expect(mirrored.sms, same(barcode.sms));
    expect(mirrored.url, same(barcode.url));
    expect(mirrored.wifi, same(barcode.wifi));
    expect(mirrored.rawValue, barcode.rawValue);
  });

  group('computeBoundingBoxSize', () {
    test('returns the bounding box size of four corners', () {
      const corners = [
        Offset(10, 20),
        Offset(110, 20),
        Offset(110, 70),
        Offset(10, 70),
      ];

      expect(computeBoundingBoxSize(corners), const Size(100, 50));
    });

    test('handles rotated (non-axis-aligned) corners', () {
      const corners = [
        Offset(50, 0),
        Offset(100, 50),
        Offset(50, 100),
        Offset(0, 50),
      ];

      expect(computeBoundingBoxSize(corners), const Size(100, 100));
    });

    test('returns Size.zero when there are not exactly four corners', () {
      expect(computeBoundingBoxSize(const []), Size.zero);
      expect(computeBoundingBoxSize(const [Offset.zero]), Size.zero);
      expect(
        computeBoundingBoxSize(const [
          Offset.zero,
          Offset(1, 1),
          Offset(2, 2),
          Offset(3, 3),
          Offset(4, 4),
        ]),
        Size.zero,
      );
    });
  });
}
