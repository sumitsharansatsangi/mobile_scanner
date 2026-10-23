import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

void main() {
  Barcode parse(String value, {BarcodeFormat format = BarcodeFormat.qrCode}) =>
      Barcode.fromNative({'rawValue': value, 'format': format.rawValue});

  test(
    'MECARD preserves repeated phones, contacts, geometry and raw bytes',
    () {
      final bytes = Uint8List.fromList([0, 128, 255]);
      final barcode = Barcode.fromNative({
        'rawValue':
            'MECARD:N:Doe,Jane;TEL:123;TEL:456;EMAIL:jane@example.com;;',
        'format': BarcodeFormat.qrCode.rawValue,
        'rawBytes': bytes,
        'corners': [
          {'x': 1.0, 'y': 2.0},
        ],
      });
      expect(barcode.type, BarcodeType.contactInfo);
      expect(barcode.contactInfo?.name?.first, 'Jane');
      expect(barcode.contactInfo?.phones.map((p) => p.number), ['123', '456']);
      expect((barcode.rawDecodedBytes! as DecodedBarcodeBytes).bytes, bytes);
      expect(barcode.corners.single.dx, 1);
    },
  );

  test('WiFi escaping does not split credentials', () {
    final wifi = parse(r'WIFI:T:WPA;S:Office\;Guest;P:a\:b\;c\\d;;').wifi!;
    expect(wifi.ssid, 'Office;Guest');
    expect(wifi.password, r'a:b;c\d');
    expect(wifi.encryptionType, EncryptionType.wpa);
  });

  test('vCard unfolds lines and accepts field parameters', () {
    final contact = parse(
      'BEGIN:VCARD\r\nN:Doe;Jane;;;\r\n'
      'FN:Jane Doe\r\nORG:Example\r\n Company\r\n'
      'TEL;TYPE=CELL:123\r\nEND:VCARD',
    ).contactInfo!;
    expect(contact.organization, 'ExampleCompany');
    expect(contact.name?.first, 'Jane');
    expect(contact.phones.single.number, '123');
  });

  test('email, phone, SMS, geo and bookmarks have structured values', () {
    final email = parse(
      'mailto:hello@example.com?subject=Hello%20world&body=Hi',
    ).email!;
    expect(email.subject, 'Hello world');
    expect(email.address, 'hello@example.com');
    expect(parse('tel:+1234').phone?.number, '+1234');
    expect(parse('SMSTO:+1234:hello:world').sms?.message, 'hello:world');
    expect(parse('sms:+1234?body=hello').sms?.message, 'hello');
    expect(parse('geo:12.5,-77.2').geoPoint?.latitude, 12.5);
    expect(parse('geo:NaN,1').type, BarcodeType.text);
    expect(parse('geo:91,1').geoPoint, isNull);
    expect(parse('https://example.com').url?.url, 'https://example.com');
    expect(
      parse('MEBKM:TITLE:Example;URL:https://example.com;;').url?.title,
      'Example',
    );
  });

  test('calendar fields and product classification', () {
    final event = parse(
      'BEGIN:VEVENT\nSUMMARY:Meeting\nDTSTART:20261023T100000Z\n'
      'DTEND:20261023T110000Z\nEND:VEVENT',
    ).calendarEvent!;
    expect(event.start, DateTime.utc(2026, 10, 23, 10));
    expect(event.summary, 'Meeting');
    expect(
      parse('9781234567890', format: BarcodeFormat.ean13).type,
      BarcodeType.isbn,
    );
    expect(
      parse('12345678', format: BarcodeFormat.ean8).type,
      BarcodeType.product,
    );
    expect(parse('hello').type, BarcodeType.text);
  });

  test('native structured information wins over heuristic parsing', () {
    final barcode = Barcode.fromNative({
      'rawValue': 'tel:123',
      'type': BarcodeType.contactInfo.rawValue,
      'contactInfo': {'organization': 'Native'},
    });
    expect(barcode.type, BarcodeType.contactInfo);
    expect(barcode.contactInfo?.organization, 'Native');
    expect(barcode.phone, isNull);
  });
  test('malformed escapes remain text and null SDK payloads are enriched', () {
    expect(parse('mailto:%GG').type, BarcodeType.text);
    expect(parse('MECARD:').type, BarcodeType.text);
    final barcode = Barcode.fromNative({
      'rawValue': 'WIFI:T:WPA;S:Guest;;',
      'type': 7,
      'wifi': null,
    });
    expect(barcode.type, BarcodeType.wifi);
    expect(barcode.wifi?.ssid, 'Guest');
    expect(
      parse('MATMSG:TO:a@example.com;SUB:Hello;BODY:Body;;').email?.subject,
      'Hello',
    );
  });
  test('AAMVA data exposes driver license fields on FFI and web', () {
    final license = parse(
      '@\nANSI 636000080002DL00410288\n'
      'DAQSAMPLE123\nDCSDOE\nDACJANE\nDADANN\n'
      'DBB01011990\nDAG123 Example Road\nDAICity\nDAJCA\nDCGUSA',
    ).driverLicense!;
    expect(license.licenseNumber, 'SAMPLE123');
    expect(license.firstName, 'JANE');
    expect(license.addressState, 'CA');
  });
}
