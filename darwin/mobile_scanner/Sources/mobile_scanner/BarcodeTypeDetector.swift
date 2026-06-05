import Foundation

/// Extension to detect barcode type from raw string value using heuristics.
/// This matches the behavior of MLKit's valueType property.
extension String {
    /// Detects the barcode type from the raw string value using heuristic pattern matching.
    /// Returns the raw integer value matching the BarcodeType enum (0-12).
    ///
    /// Detection order matters - more specific patterns are checked first.
    ///
    /// If any error occurs during detection, returns 0 (BarcodeType.unknown).
    func detectBarcodeType() -> Int {
        do {
            let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
            
            // Return unknown if empty
            if trimmed.isEmpty {
                return 0 // BarcodeType.unknown
            }
            
            let uppercased = trimmed.uppercased()
            
            // Check for ContactInfo (VCARD) - most specific structured format
            if uppercased.hasPrefix("BEGIN:VCARD") {
                return 1 // BarcodeType.contactInfo
            }
        
            // Check for CalendarEvent (VCALENDAR) - iCalendar format
            if uppercased.hasPrefix("BEGIN:VCALENDAR") {
                return 11 // BarcodeType.calendarEvent
            }
            
            // Check for WiFi
            if uppercased.hasPrefix("WIFI:") {
                return 9 // BarcodeType.wifi
            }
            
            // Check for Email
            if uppercased.hasPrefix("MAILTO:") {
                return 2 // BarcodeType.email
            }
            
            // Check for Phone
            if uppercased.hasPrefix("TEL:") {
                return 4 // BarcodeType.phone
            }
            
            // Check for SMS
            if uppercased.hasPrefix("SMS:") {
                return 6 // BarcodeType.sms
            }
            
            // Check for Geo
            if uppercased.hasPrefix("GEO:") {
                return 10 // BarcodeType.geo
            }
            
            // Check for URL bookmark (MEBKM format)
            if uppercased.hasPrefix("MEBKM:") {
                return 8 // BarcodeType.url
            }
            
            // Check for HTTP/HTTPS URLs
            if uppercased.hasPrefix("HTTP://") || uppercased.hasPrefix("HTTPS://") {
                return 8 // BarcodeType.url
            }
            
            // Check for ISBN
            if isISBN() {
                return 3 // BarcodeType.isbn
            }

            // Check for ISSN with 2-digit supplement
            if isISSNWithSupplement() {
                return 5 // BarcodeType.product
            }
            
            // Check for Product codes (EAN/UPC)
            if isProductCode() {
                return 5 // BarcodeType.product
            }
            
            // Default to text for unrecognized patterns
            return 7 // BarcodeType.text
        } catch {
            return 0 // BarcodeType.unknown
        }
    }

    /// Parses Wi-Fi QR payloads into the same field names used by Android.
    func parseWiFi() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.uppercased().hasPrefix("WIFI:") else {
            return nil
        }

        let payload = String(trimmed.dropFirst(5))
        let fields = payload.parseSemicolonFields()
        let encryptionType: Int
        switch fields["T"]?.uppercased() {
        case "WEP":
            encryptionType = 3
        case "WPA", "WPA2", "WPA3", "WPA/WPA2":
            encryptionType = 2
        case "NOPASS", "NONE", "":
            encryptionType = 1
        default:
            encryptionType = 0
        }

        return [
            "encryptionType": encryptionType,
            "password": fields["P"],
            "ssid": fields["S"],
        ]
    }

    /// Parses iCalendar event QR payloads into the same field names used by Android.
    func parseCalendarEvent() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        let uppercased = trimmed.uppercased()
        guard uppercased.hasPrefix("BEGIN:VCALENDAR") || uppercased.hasPrefix("BEGIN:VEVENT") else {
            return nil
        }

        var fields: [String: String] = [:]
        var inEvent = uppercased.hasPrefix("BEGIN:VEVENT")

        for line in trimmed.unfoldedCalendarLines() {
            guard let separator = line.firstIndex(of: ":"),
                  separator != line.startIndex else {
                continue
            }

            let name = line[..<separator].split(separator: ";", maxSplits: 1).first?.uppercased() ?? ""
            let value = String(line[line.index(after: separator)...]).unescapedCalendarText()

            if name == "BEGIN", value.uppercased() == "VEVENT" {
                inEvent = true
            } else if name == "END", value.uppercased() == "VEVENT" {
                break
            } else if inEvent, fields[name] == nil {
                fields[name] = value
            }
        }

        guard !fields.isEmpty else {
            return nil
        }

        return [
            "description": fields["DESCRIPTION"],
            "end": fields["DTEND"]?.formattedCalendarDate(),
            "location": fields["LOCATION"],
            "organizer": fields["ORGANIZER"]?.removingMailtoPrefix(),
            "start": fields["DTSTART"]?.formattedCalendarDate(),
            "status": fields["STATUS"],
            "summary": fields["SUMMARY"],
        ]
    }

    /// Parses vCard QR payloads into the same field names used by Android.
    func parseContactInfo() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.uppercased().hasPrefix("BEGIN:VCARD") else {
            return nil
        }

        var formattedName: String?
        var firstName: String?
        var middleName: String?
        var lastName: String?
        var prefix: String?
        var suffix: String?
        var organization: String?
        var title: String?
        var addresses: [[String: Any?]] = []
        var emails: [[String: Any?]] = []
        var phones: [[String: Any?]] = []
        var urls: [String] = []

        for line in trimmed.unfoldedCalendarLines() {
            guard let separator = line.firstIndex(of: ":"),
                  separator != line.startIndex else {
                continue
            }

            let property = String(line[..<separator])
            let parts = property.split(separator: ";").map { String($0).uppercased() }
            let name = parts.first ?? ""
            let params = Array(parts.dropFirst())
            let value = String(line[line.index(after: separator)...]).unescapedCalendarText()

            switch name {
            case "FN":
                formattedName = value
            case "N":
                let values = value.split(separator: ";", omittingEmptySubsequences: false).map(String.init)
                lastName = values[safe: 0]?.nilIfEmpty
                firstName = values[safe: 1]?.nilIfEmpty
                middleName = values[safe: 2]?.nilIfEmpty
                prefix = values[safe: 3]?.nilIfEmpty
                suffix = values[safe: 4]?.nilIfEmpty
            case "ORG":
                organization = value.split(separator: ";", omittingEmptySubsequences: false).first.map(String.init)?.nilIfEmpty
            case "TITLE":
                title = value
            case "TEL":
                phones.append(["number": value, "type": params.phoneType])
            case "EMAIL":
                emails.append(["address": value, "body": nil, "subject": nil, "type": params.emailType])
            case "URL":
                urls.append(value)
            case "ADR":
                let lines = value
                    .split(separator: ";", omittingEmptySubsequences: false)
                    .dropFirst(2)
                    .map(String.init)
                    .filter { !$0.isEmpty }
                addresses.append(["addressLines": lines, "type": params.addressType])
            default:
                continue
            }
        }

        guard formattedName != nil ||
              firstName != nil ||
              lastName != nil ||
              organization != nil ||
              title != nil ||
              !addresses.isEmpty ||
              !emails.isEmpty ||
              !phones.isEmpty ||
              !urls.isEmpty else {
            return nil
        }

        return [
            "addresses": addresses,
            "emails": emails,
            "name": [
                "first": firstName,
                "formattedName": formattedName,
                "last": lastName,
                "middle": middleName,
                "prefix": prefix,
                "pronunciation": nil,
                "suffix": suffix,
            ],
            "organization": organization,
            "phones": phones,
            "title": title,
            "urls": urls,
        ]
    }

    func parseEmail() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        let uppercased = trimmed.uppercased()

        if uppercased.hasPrefix("MATMSG:") {
            let fields = String(trimmed.dropFirst(7)).parseSemicolonFields()
            return [
                "address": fields["TO"],
                "body": fields["BODY"],
                "subject": fields["SUB"],
                "type": 0,
            ]
        }

        guard uppercased.hasPrefix("MAILTO:") else {
            return nil
        }

        let body = String(trimmed.dropFirst(7))
        let address = body.split(separator: "?", maxSplits: 1, omittingEmptySubsequences: false).first.map(String.init) ?? ""
        let query = body.contains("?") ? String(body.split(separator: "?", maxSplits: 1, omittingEmptySubsequences: false).last ?? "") : ""
        let fields = query.parseQueryFields()
        return [
            "address": address.percentDecoded,
            "body": fields["body"],
            "subject": fields["subject"],
            "type": 0,
        ]
    }

    func parsePhone() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.uppercased().hasPrefix("TEL:") else {
            return nil
        }

        return ["number": String(trimmed.dropFirst(4)), "type": 0]
    }

    func parseSMS() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        let uppercased = trimmed.uppercased()
        guard uppercased.hasPrefix("SMS:") || uppercased.hasPrefix("SMSTO:") else {
            return nil
        }

        let body = String(trimmed.dropFirst(uppercased.hasPrefix("SMSTO:") ? 6 : 4))
        let phone = body.split(separator: ":", maxSplits: 1, omittingEmptySubsequences: false)
            .first
            .map(String.init)?
            .split(separator: "?", maxSplits: 1, omittingEmptySubsequences: false)
            .first
            .map(String.init) ?? ""
        let message: String?
        if body.contains(":") {
            message = body.split(separator: ":", maxSplits: 1, omittingEmptySubsequences: false).last.map(String.init)
        } else if body.contains("?") {
            let query = body.split(separator: "?", maxSplits: 1, omittingEmptySubsequences: false).last.map(String.init) ?? ""
            message = query.parseQueryFields()["body"]
        } else {
            message = nil
        }

        return ["message": message, "phoneNumber": phone]
    }

    func parseGeoPoint() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed.uppercased().hasPrefix("GEO:") else {
            return nil
        }

        let coordinates = String(trimmed.dropFirst(4))
            .split(separator: "?", maxSplits: 1, omittingEmptySubsequences: false)
            .first
            .map(String.init)?
            .split(separator: ",", omittingEmptySubsequences: false) ?? []
        guard coordinates.count >= 2,
              let latitude = Double(coordinates[0]),
              let longitude = Double(coordinates[1]) else {
            return nil
        }

        return ["latitude": latitude, "longitude": longitude]
    }

    func parseURL() -> [String: Any?]? {
        let trimmed = self.trimmingCharacters(in: .whitespacesAndNewlines)
        let uppercased = trimmed.uppercased()

        if uppercased.hasPrefix("MEBKM:") {
            let fields = String(trimmed.dropFirst(6)).parseSemicolonFields()
            guard let url = fields["URL"] else {
                return nil
            }

            return ["title": fields["TITLE"], "url": url]
        }

        guard uppercased.hasPrefix("HTTP://") || uppercased.hasPrefix("HTTPS://") else {
            return nil
        }

        return ["title": nil, "url": trimmed]
    }

    func parseDriverLicense() -> [String: Any?]? {
        let normalized = self
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
        let uppercased = normalized.uppercased()
        guard uppercased.hasPrefix("@ANSI ") || uppercased.contains("\nANSI ") else {
            return nil
        }

        var fields: [String: String] = [:]
        for line in normalized.split(separator: "\n", omittingEmptySubsequences: false).map(String.init) {
            fields.merge(line.parseDriverLicenseFields()) { _, new in new }
        }

        guard !fields.isEmpty else {
            return nil
        }

        return [
            "addressCity": fields["DAI"],
            "addressState": fields["DAJ"],
            "addressStreet": fields["DAG"],
            "addressZip": fields["DAK"],
            "birthDate": fields["DBB"],
            "documentType": "DL",
            "expiryDate": fields["DBA"],
            "firstName": fields["DAC"] ?? fields["DCT"]?.split(separator: ",", maxSplits: 1).first.map(String.init),
            "gender": fields["DBC"],
            "issueDate": fields["DBD"],
            "issuingCountry": fields["DCG"],
            "lastName": fields["DCS"],
            "licenseNumber": fields["DAQ"],
            "middleName": fields["DAD"] ?? fields["DCT"]?.split(separator: ",", maxSplits: 1).last.map(String.init),
        ]
    }
    
    /// Checks if the string matches ISBN-10 or ISBN-13 format.
    /// ISBN-10: 10 digits (with optional hyphens)
    /// ISBN-13: 13 digits starting with 978 or 979 (with optional hyphens)
    /// ISBN-13+5: ISBN-13 plus a 5-digit EAN add-on supplement
    private func isISBN() -> Bool {
        // Remove common ISBN labels and separators for validation.
        let digitsOnly = self
            .replacingOccurrences(
                of: #"ISBN(?:[- ]?1[03])?"#,
                with: "",
                options: [.regularExpression, .caseInsensitive]
            )
            .filter { $0.isLetter || $0.isNumber }
        
        // Check for ISBN-13 (13 digits, starting with 978 or 979)
        if digitsOnly.count == 13,
           let _ = Int(digitsOnly),
           digitsOnly.hasPrefix("978") || digitsOnly.hasPrefix("979") {
            return true
        }

        // Check for ISBN-13 with 5-digit supplement
        if digitsOnly.count == 18,
           let _ = Int(digitsOnly),
           digitsOnly.hasPrefix("978") || digitsOnly.hasPrefix("979") {
            return true
        }
        
        // Check for ISBN-10 (10 digits)
        if digitsOnly.count == 10,
           let _ = Int(digitsOnly) {
            return true
        }
        
        return false
    }

    /// Checks if the string matches ISSN with a 2-digit supplement.
    /// Encoded ISSN barcodes are EAN-13 values starting with 977 plus EAN-2.
    private func isISSNWithSupplement() -> Bool {
        let normalized = self.replacingOccurrences(
            of: "[^0-9A-Za-z]",
            with: "",
            options: .regularExpression
        ).replacingOccurrences(of: "ISSN", with: "", options: .caseInsensitive)

        if normalized.count == 15,
           normalized.hasPrefix("977"),
           normalized.allSatisfy({ $0.isNumber }) {
            return true
        }

        guard self.uppercased().hasPrefix("ISSN"),
              normalized.count == 10 else {
            return false
        }

        let issn = String(normalized.prefix(8))
        let supplement = String(normalized.suffix(2))
        return issn.isValidISSN() && supplement.allSatisfy { $0.isNumber }
    }
    
    /// Checks if the string matches product code format (EAN/UPC).
    /// EAN-8: 8 digits
    /// UPC-A: 12 digits
    /// EAN-13: 13 digits
    private func isProductCode() -> Bool {
        // Remove any non-digit characters
        let digitsOnly = self.replacingOccurrences(of: "[^0-9]", with: "", options: .regularExpression)
        
        // Check for EAN-8 (8 digits), UPC-A (12 digits), or EAN-13 (13 digits)
        let length = digitsOnly.count
        if length == 8 || length == 12 || length == 13 {
            // Verify all characters are digits
            return digitsOnly.allSatisfy { $0.isNumber }
        }
        
        return false
    }

    private func parseSemicolonFields() -> [String: String] {
        var fields: [String: String] = [:]
        for field in splitEscapedFields() {
            guard let separator = field.firstIndex(of: ":"),
                  separator != field.startIndex else {
                continue
            }

            let key = field[..<separator].uppercased()
            let value = field[field.index(after: separator)...].unescapedBarcodeField()
            fields[key] = value
        }
        return fields
    }

    private func parseQueryFields() -> [String: String] {
        guard !isEmpty else {
            return [:]
        }

        var fields: [String: String] = [:]
        for pair in split(separator: "&") {
            let parts = pair.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
            guard parts.count == 2 else {
                continue
            }

            fields[String(parts[0]).percentDecoded] = String(parts[1]).percentDecoded
        }

        return fields
    }

    private func unfoldedCalendarLines() -> [String] {
        let lines = self
            .replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\r", with: "\n")
            .split(separator: "\n", omittingEmptySubsequences: false)
            .map(String.init)

        var unfolded: [String] = []
        for line in lines {
            if (line.hasPrefix(" ") || line.hasPrefix("\t")), !unfolded.isEmpty {
                unfolded[unfolded.count - 1] += String(line.dropFirst())
            } else {
                unfolded.append(line)
            }
        }

        return unfolded
    }

    private func splitEscapedFields() -> [String] {
        var fields: [String] = []
        var field = ""
        var escaped = false

        for character in self {
            if escaped {
                field.append("\\")
                field.append(character)
                escaped = false
            } else if character == "\\" {
                escaped = true
            } else if character == ";" {
                fields.append(field)
                field = ""
            } else {
                field.append(character)
            }
        }

        if escaped {
            field.append("\\")
        }

        fields.append(field)
        return fields
    }

    private func unescapedBarcodeField() -> String {
        return self
            .replacingOccurrences(of: "\\;", with: ";")
            .replacingOccurrences(of: "\\:", with: ":")
            .replacingOccurrences(of: "\\,", with: ",")
            .replacingOccurrences(of: "\\\\", with: "\\")
    }

    private func unescapedCalendarText() -> String {
        return self
            .replacingOccurrences(of: "\\n", with: "\n", options: .caseInsensitive)
            .replacingOccurrences(of: "\\,", with: ",")
            .replacingOccurrences(of: "\\;", with: ";")
            .replacingOccurrences(of: "\\\\", with: "\\")
    }

    private func formattedCalendarDate() -> String {
        if range(of: #"^\d{8}$"#, options: .regularExpression) != nil {
            let year = prefix(4)
            let month = dropFirst(4).prefix(2)
            let day = suffix(2)
            return "\(year)-\(month)-\(day)"
        }

        let pattern = #"^(\d{4})(\d{2})(\d{2})T(\d{2})(\d{2})(\d{2})(Z?)$"#
        guard let range = range(of: pattern, options: .regularExpression) else {
            return self
        }

        let compact = String(self[range])
        let year = compact.prefix(4)
        let month = compact.dropFirst(4).prefix(2)
        let day = compact.dropFirst(6).prefix(2)
        let hour = compact.dropFirst(9).prefix(2)
        let minute = compact.dropFirst(11).prefix(2)
        let second = compact.dropFirst(13).prefix(2)
        let zone = compact.hasSuffix("Z") ? "Z" : ""
        return "\(year)-\(month)-\(day)T\(hour):\(minute):\(second)\(zone)"
    }

    private func removingMailtoPrefix() -> String {
        guard lowercased().hasPrefix("mailto:") else {
            return self
        }

        return String(dropFirst("mailto:".count))
    }

    private var percentDecoded: String {
        return removingPercentEncoding ?? self
    }

    private var nilIfEmpty: String? {
        return isEmpty ? nil : self
    }

    private var driverLicenseFields: [String] {
        return [
            "DAA", "DAB", "DAC", "DAD", "DAE", "DAF", "DAG", "DAH", "DAI",
            "DAJ", "DAK", "DAL", "DAM", "DAN", "DAO", "DAP", "DAQ", "DAR",
            "DAS", "DAT", "DAU", "DAV", "DAW", "DAX", "DAY", "DAZ", "DBA",
            "DBB", "DBC", "DBD", "DBE", "DBF", "DBG", "DBH", "DBI", "DBJ",
            "DBK", "DBL", "DBM", "DBN", "DBO", "DBP", "DBQ", "DBR", "DBS",
            "DCA", "DCB", "DCD", "DCE", "DCF", "DCG", "DCH", "DCI", "DCJ",
            "DCK", "DCL", "DCM", "DCN", "DCO", "DCP", "DCQ", "DCR", "DCS",
            "DCT", "DCU",
        ]
    }

    private func parseDriverLicenseFields() -> [String: String] {
        let positions = driverLicenseFields
            .flatMap { field -> [(String, String.Index)] in
                ranges(of: field).map { (field, $0.lowerBound) }
            }
            .sorted { $0.1 < $1.1 }

        guard !positions.isEmpty else {
            return [:]
        }

        var fields: [String: String] = [:]
        for (index, entry) in positions.enumerated() {
            let key = entry.0
            let start = self.index(entry.1, offsetBy: key.count)
            let end = positions[safe: index + 1]?.1 ?? endIndex
            let value = String(self[start..<end]).trimmingCharacters(in: .whitespacesAndNewlines)
            if !value.isEmpty {
                fields[key] = value
            }
        }

        return fields
    }

    private func ranges(of searchString: String) -> [Range<String.Index>] {
        var ranges: [Range<String.Index>] = []
        var searchStart = startIndex
        while searchStart < endIndex,
              let range = self[searchStart...].range(of: searchString) {
            ranges.append(range)
            searchStart = range.upperBound
        }

        return ranges
    }

    private func isValidISSN() -> Bool {
        guard count == 8 else {
            return false
        }

        let characters = Array(uppercased())
        var sum = 0

        for index in 0..<7 {
            guard let digit = characters[index].wholeNumberValue else {
                return false
            }
            sum += digit * (8 - index)
        }

        let checkCharacter = characters[7]
        let checkDigit: Int
        if checkCharacter == "X" {
            checkDigit = 10
        } else if let digit = checkCharacter.wholeNumberValue {
            checkDigit = digit
        } else {
            return false
        }

        return (sum + checkDigit) % 11 == 0
    }
}

private extension Collection {
    subscript(safe index: Index) -> Element? {
        return indices.contains(index) ? self[index] : nil
    }
}

private extension Array where Element == String {
    var phoneType: Int {
        if hasType("CELL") {
            return 4
        }
        if hasType("FAX") {
            return 3
        }
        if hasType("HOME") {
            return 2
        }
        if hasType("WORK") {
            return 1
        }
        return 0
    }

    var emailType: Int {
        if hasType("HOME") {
            return 2
        }
        if hasType("WORK") {
            return 1
        }
        return 0
    }

    var addressType: Int {
        if hasType("HOME") {
            return 2
        }
        if hasType("WORK") {
            return 1
        }
        return 0
    }

    func hasType(_ type: String) -> Bool {
        return contains { param in
            param == type ||
            param == "TYPE=\(type)" ||
            (param.hasPrefix("TYPE=") && param.dropFirst(5).split(separator: ",").contains(Substring(type)))
        }
    }
}
