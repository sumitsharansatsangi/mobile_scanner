import 'package:mobile_scanner/src/enums/barcode_format.dart';

/// Parses common barcode payloads without depending on a platform decoder.
/// Malformed payloads remain text rather than manufacturing structured values.
Map<String, Object?> parseBarcodePayload(String? value, BarcodeFormat format) {
  try {
    return _parsePayload(value, format);
  } on FormatException {
    return const {'type': 7};
  }
}

Map<String, Object?> _parsePayload(String? value, BarcodeFormat format) {
  if (value == null || value.isEmpty) return const {};
  final upper = value.toUpperCase();
  final uri = Uri.tryParse(value);
  // Uri normalizes stray percent signs instead of rejecting them. Validate
  // encoded URI payloads before interpreting their structured fields.
  if (uri != null &&
      {
        'mailto',
        'tel',
        'sms',
        'smsto',
        'geo',
        'http',
        'https',
        'ftp',
      }.contains(uri.scheme.toLowerCase()) &&
      RegExp('%(?![0-9a-fA-F]{2})').hasMatch(value)) {
    return const {'type': 7};
  }
  if (upper.startsWith('MATMSG:')) {
    final fields = _fields(value.substring(7));
    if (fields['TO']?.first case final address?) {
      return {
        'type': 2,
        'email': {
          'address': address,
          'subject': fields['SUB']?.first,
          'body': fields['BODY']?.first,
        },
      };
    }
  }
  if (upper.startsWith('@ANSI ') || upper.contains('\nANSI ')) {
    final fields = <String, String>{};
    final pattern = RegExp(
      '(DAQ|DCS|DAC|DAD|DCT|DBC|DBB|DBA|DBD|DAG|DAI|DAJ|DAK|DCG)',
    );
    for (final line in value.split(RegExp(r'[\r\n]+'))) {
      final matches = pattern.allMatches(line).toList();
      for (var i = 0; i < matches.length; i++) {
        final match = matches[i];
        fields[match.group(0)!] = line
            .substring(
              match.end,
              i + 1 < matches.length ? matches[i + 1].start : line.length,
            )
            .trim();
      }
    }
    if (fields.isNotEmpty) {
      return {
        'type': 12,
        'driverLicense': {
          'licenseNumber': fields['DAQ'],
          'lastName': fields['DCS'],
          'firstName': fields['DAC'] ?? fields['DCT']?.split(',').first,
          'middleName': fields['DAD'],
          'gender': fields['DBC'],
          'birthDate': fields['DBB'],
          'expiryDate': fields['DBA'],
          'issueDate': fields['DBD'],
          'addressStreet': fields['DAG'],
          'addressCity': fields['DAI'],
          'addressState': fields['DAJ'],
          'addressZip': fields['DAK'],
          'issuingCountry': fields['DCG'],
          'documentType': 'DL',
        },
      };
    }
  }
  if (upper.startsWith('WIFI:')) {
    final fields = _fields(value.substring(5));
    if (fields['S']?.first case final ssid?) {
      return {
        'type': 9,
        'wifi': {
          'ssid': ssid,
          'password': fields['P']?.first,
          'encryptionType': switch (fields['T']?.first.toUpperCase()) {
            'NOPASS' || '' => 1,
            'WPA' || 'WPA2' || 'WPA3' || 'SAE' => 2,
            'WEP' => 3,
            _ => 0,
          },
        },
      };
    }
  } else if (upper.startsWith('MECARD:') || upper.startsWith('BEGIN:VCARD')) {
    final mecard = upper.startsWith('MECARD:');
    final fields = mecard ? _fields(value.substring(7)) : _cardFields(value);
    if (fields.keys.every((key) => {'BEGIN', 'END', 'VERSION'}.contains(key))) {
      return const {'type': 7};
    }
    final name = fields['N']?.first.split(mecard ? ',' : ';');
    return {
      'type': 1,
      'contactInfo': {
        'name': {
          'formattedName': fields['FN']?.first ?? fields['N']?.first,
          'last': name?.first,
          'first': name != null && name.length > 1 ? name[1] : null,
          'middle': !mecard && name != null && name.length > 2 ? name[2] : null,
        },
        'organization': fields['ORG']?.first,
        'title': fields['TITLE']?.first,
        'phones': [
          for (final p in fields['TEL'] ?? <String>[]) {'number': p},
        ],
        'emails': [
          for (final e in fields['EMAIL'] ?? <String>[]) {'address': e},
        ],
        'addresses': [
          for (final a in fields['ADR'] ?? <String>[])
            {'addressLines': a.split(';').where((s) => s.isNotEmpty).toList()},
        ],
        'urls': fields['URL'] ?? <String>[],
      },
    };
  } else if (upper.startsWith('BEGIN:VEVENT') ||
      upper.contains('\nBEGIN:VEVENT')) {
    final fields = _cardFields(value);
    return {
      'type': 11,
      'calendarEvent': {
        'summary': fields['SUMMARY']?.first,
        'description': fields['DESCRIPTION']?.first,
        'location': fields['LOCATION']?.first,
        'organizer': fields['ORGANIZER']?.first,
        'status': fields['STATUS']?.first,
        'start': fields['DTSTART']?.first,
        'end': fields['DTEND']?.first,
      },
    };
  } else if (uri != null) {
    switch (uri.scheme.toLowerCase()) {
      case 'mailto':
        return {
          'type': 2,
          'email': {
            'address': Uri.decodeComponent(uri.path),
            'subject': uri.queryParameters['subject'],
            'body': uri.queryParameters['body'],
          },
        };
      case 'tel':
        return {
          'type': 4,
          'phone': {'number': Uri.decodeComponent(uri.path)},
        };
      case 'sms':
      case 'smsto':
        final parts = value.substring(value.indexOf(':') + 1).split(':');
        return {
          'type': 6,
          'sms': {
            'phoneNumber': Uri.decodeComponent(parts.first.split('?').first),
            'message':
                uri.queryParameters['body'] ??
                (parts.length > 1 ? parts.sublist(1).join(':') : null),
          },
        };
      case 'geo':
        final coordinates = uri.path.split(',');
        final lat = double.tryParse(coordinates.first);
        final lon = coordinates.length > 1
            ? double.tryParse(coordinates[1])
            : null;
        if (lat != null &&
            lon != null &&
            lat.isFinite &&
            lon.isFinite &&
            lat.abs() <= 90 &&
            lon.abs() <= 180) {
          return {
            'type': 10,
            'geoPoint': {'latitude': lat, 'longitude': lon},
          };
        }
      case 'http':
      case 'https':
      case 'ftp':
        if (uri.host.isNotEmpty) {
          return {
            'type': 8,
            'url': {'url': value},
          };
        }
    }
  }
  if (upper.startsWith('MEBKM:')) {
    final fields = _fields(value.substring(6));
    if (fields['URL']?.first case final url?) {
      return {
        'type': 8,
        'url': {'url': url, 'title': fields['TITLE']?.first},
      };
    }
  }
  if (format == BarcodeFormat.ean13 &&
      RegExp(r'^97[89]\d{10}$').hasMatch(value)) {
    return const {'type': 3};
  }
  if ({
    BarcodeFormat.ean13,
    BarcodeFormat.ean8,
    BarcodeFormat.upcA,
    BarcodeFormat.upcE,
  }.contains(format)) {
    return const {'type': 5};
  }
  return const {'type': 7};
}

Map<String, List<String>> _fields(String value) {
  // Split on unescaped delimiters, then unescape once. This preserves escaped
  // semicolons, colons and backslashes inside WiFi credentials and contacts.
  final fields = <String, List<String>>{};
  for (final match in RegExp(r'(?:[^;\\]|\\.)+').allMatches(value)) {
    final field = match.group(0)!;
    final separator = field.indexOf(':');
    if (separator < 0) continue;
    final key = field.substring(0, separator).toUpperCase();
    final text = field
        .substring(separator + 1)
        .replaceAllMapped(
          RegExp(r'\\(.)'),
          (m) => m.group(1)!,
        );
    fields.putIfAbsent(key, () => []).add(text);
  }
  return fields;
}

Map<String, List<String>> _cardFields(String value) {
  final fields = <String, List<String>>{};
  final unfolded = value.replaceAll(RegExp(r'\r?\n[ \t]'), '');
  for (final line in unfolded.split(RegExp(r'\r?\n'))) {
    final colon = line.indexOf(':');
    if (colon < 0) continue;
    final key = line.substring(0, colon).split(';').first.toUpperCase();
    final text = line
        .substring(colon + 1)
        .replaceAll(r'\n', '\n')
        .replaceAll(r'\N', '\n')
        .replaceAll(r'\,', ',')
        .replaceAll(r'\;', ';')
        .replaceAll(r'\\', r'\');
    fields.putIfAbsent(key, () => []).add(text);
  }
  return fields;
}
