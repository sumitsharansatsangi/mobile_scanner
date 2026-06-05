package dev.steenbakker.mobile_scanner.engine

import java.net.URLDecoder
import java.nio.charset.StandardCharsets

/**
 * Maps a [ZxingBarcode] to the same map shape that ML Kit barcodes are sent to
 * Flutter with (see `Barcode.data` in MobileScannerUtilities.kt), so the Dart
 * `Barcode.fromNative` parser handles both engines identically.
 *
 * ZXing-C++ returns symbology, text, bytes, and corners. For common QR payload
 * conventions we parse the text into the same structured fields ML Kit exposes,
 * so making ZXing the primary engine does not lose URL/WiFi/SMS/etc. metadata.
 */
val ZxingBarcode.data: Map<String, Any?>
    get() {
        val parsed = ParsedPayload.from(text, format)
        val cornerList = listOf(
            mapOf("x" to corners[0].toDouble(), "y" to corners[1].toDouble()),
            mapOf("x" to corners[2].toDouble(), "y" to corners[3].toDouble()),
            mapOf("x" to corners[4].toDouble(), "y" to corners[5].toDouble()),
            mapOf("x" to corners[6].toDouble(), "y" to corners[7].toDouble()),
        )

        // Derive a bounding size from the corner points.
        val xs = listOf(corners[0], corners[2], corners[4], corners[6])
        val ys = listOf(corners[1], corners[3], corners[5], corners[7])
        val sizeMap = mapOf(
            "width" to (xs.max() - xs.min()).toDouble(),
            "height" to (ys.max() - ys.min()).toDouble(),
        )

        return mapOf(
            "calendarEvent" to parsed.calendarEvent,
            "contactInfo" to parsed.contactInfo,
            "corners" to cornerList,
            "displayValue" to text,
            "driverLicense" to parsed.driverLicense,
            "email" to parsed.email,
            "format" to format,
            "geoPoint" to parsed.geoPoint,
            "phone" to parsed.phone,
            "rawBytes" to rawBytes,
            "rawValue" to text,
            "size" to sizeMap,
            "sms" to parsed.sms,
            "type" to parsed.type,
            "url" to parsed.url,
            "wifi" to parsed.wifi,
        )
    }

private data class ParsedPayload(
    val type: Int,
    val calendarEvent: Map<String, Any?>? = null,
    val contactInfo: Map<String, Any?>? = null,
    val driverLicense: Map<String, Any?>? = null,
    val email: Map<String, Any?>? = null,
    val geoPoint: Map<String, Any?>? = null,
    val phone: Map<String, Any?>? = null,
    val sms: Map<String, Any?>? = null,
    val url: Map<String, Any?>? = null,
    val wifi: Map<String, Any?>? = null,
) {
    companion object {
        private const val TYPE_UNKNOWN = 0
        private const val TYPE_CONTACT = 1
        private const val TYPE_EMAIL = 2
        private const val TYPE_ISBN = 3
        private const val TYPE_PHONE = 4
        private const val TYPE_PRODUCT = 5
        private const val TYPE_SMS = 6
        private const val TYPE_TEXT = 7
        private const val TYPE_URL = 8
        private const val TYPE_WIFI = 9
        private const val TYPE_GEO = 10
        private const val TYPE_CALENDAR = 11
        private const val TYPE_DRIVER_LICENSE = 12

        private const val FORMAT_EAN_13 = 32
        private const val FORMAT_EAN_8 = 64
        private const val FORMAT_UPC_A = 512
        private const val FORMAT_UPC_E = 1024
        private const val EMAIL_UNKNOWN = 0
        private const val PHONE_UNKNOWN = 0
        private const val WIFI_OPEN = 1
        private const val WIFI_WPA = 2
        private const val WIFI_WEP = 3
        private val DRIVER_LICENSE_FIELDS = setOf(
            "DAA", "DAB", "DAC", "DAD", "DAE", "DAF", "DAG", "DAH", "DAI",
            "DAJ", "DAK", "DAL", "DAM", "DAN", "DAO", "DAP", "DAQ", "DAR",
            "DAS", "DAT", "DAU", "DAV", "DAW", "DAX", "DAY", "DAZ", "DBA",
            "DBB", "DBC", "DBD", "DBE", "DBF", "DBG", "DBH", "DBI", "DBJ",
            "DBK", "DBL", "DBM", "DBN", "DBO", "DBP", "DBQ", "DBR", "DBS",
            "DCA", "DCB", "DCD", "DCE", "DCF", "DCG", "DCH", "DCI", "DCJ",
            "DCK", "DCL", "DCM", "DCN", "DCO", "DCP", "DCQ", "DCR", "DCS",
            "DCT", "DCU",
        )

        fun from(text: String?, format: Int): ParsedPayload {
            if (text.isNullOrBlank()) return ParsedPayload(TYPE_UNKNOWN)

            val trimmed = text.trim()
            val upper = trimmed.uppercase()

            parseWifi(trimmed)?.let {
                return ParsedPayload(TYPE_WIFI, wifi = it)
            }
            parseEmail(trimmed, upper)?.let {
                return ParsedPayload(TYPE_EMAIL, email = it)
            }
            parsePhone(trimmed, upper)?.let {
                return ParsedPayload(TYPE_PHONE, phone = it)
            }
            parseSms(trimmed, upper)?.let {
                return ParsedPayload(TYPE_SMS, sms = it)
            }
            parseGeo(trimmed, upper)?.let {
                return ParsedPayload(TYPE_GEO, geoPoint = it)
            }
            parseContactInfo(trimmed, upper)?.let {
                return ParsedPayload(TYPE_CONTACT, contactInfo = it)
            }
            parseCalendarEvent(trimmed, upper)?.let {
                return ParsedPayload(TYPE_CALENDAR, calendarEvent = it)
            }
            parseDriverLicense(trimmed, upper)?.let {
                return ParsedPayload(TYPE_DRIVER_LICENSE, driverLicense = it)
            }
            parseUrl(trimmed, upper)?.let {
                return ParsedPayload(TYPE_URL, url = it)
            }

            return when {
                isIsbn(trimmed) -> ParsedPayload(TYPE_ISBN)
                isIssnWithSupplement(trimmed) -> ParsedPayload(TYPE_PRODUCT)
                isProductFormat(format) -> ParsedPayload(TYPE_PRODUCT)
                else -> ParsedPayload(TYPE_TEXT)
            }
        }

        private fun parseWifi(text: String): Map<String, Any?>? {
            if (!text.startsWith("WIFI:", ignoreCase = true)) return null
            val fields = parseSemicolonFields(text.substringAfter(":", ""))
            val encryptionType = when (fields["T"]?.uppercase()) {
                "WEP" -> WIFI_WEP
                "WPA", "WPA2", "WPA3", "WPA/WPA2" -> WIFI_WPA
                "NOPASS", "NONE", "" -> WIFI_OPEN
                else -> TYPE_UNKNOWN
            }
            return mapOf(
                "encryptionType" to encryptionType,
                "password" to fields["P"],
                "ssid" to fields["S"],
            )
        }

        private fun parseEmail(text: String, upper: String): Map<String, Any?>? {
            if (!upper.startsWith("MAILTO:") && !upper.startsWith("MATMSG:")) return null
            if (upper.startsWith("MATMSG:")) {
                val fields = parseSemicolonFields(text.substringAfter(":", ""))
                return mapOf(
                    "address" to fields["TO"],
                    "body" to fields["BODY"],
                    "subject" to fields["SUB"],
                    "type" to EMAIL_UNKNOWN,
                )
            }

            val body = text.substringAfter(":", "")
            val address = body.substringBefore("?")
            val query = parseQuery(body.substringAfter("?", ""))
            return mapOf(
                "address" to decode(address),
                "body" to query["body"],
                "subject" to query["subject"],
                "type" to EMAIL_UNKNOWN,
            )
        }

        private fun parsePhone(text: String, upper: String): Map<String, Any?>? {
            if (!upper.startsWith("TEL:")) return null
            return mapOf(
                "number" to text.substringAfter(":", ""),
                "type" to PHONE_UNKNOWN,
            )
        }

        private fun parseSms(text: String, upper: String): Map<String, Any?>? {
            if (!upper.startsWith("SMS:") && !upper.startsWith("SMSTO:")) {
                return null
            }
            val body = text.substringAfter(":", "")
            val phone = body.substringBefore(":").substringBefore("?")
            val message = when {
                body.contains(":") -> body.substringAfter(":")
                body.contains("?") -> parseQuery(body.substringAfter("?", ""))["body"]
                else -> null
            }
            return mapOf("message" to message, "phoneNumber" to phone)
        }

        private fun parseGeo(text: String, upper: String): Map<String, Any?>? {
            if (!upper.startsWith("GEO:")) return null
            val coords = text.substringAfter(":", "").substringBefore("?").split(",")
            if (coords.size < 2) return null
            val latitude = coords[0].toDoubleOrNull() ?: return null
            val longitude = coords[1].toDoubleOrNull() ?: return null
            return mapOf("latitude" to latitude, "longitude" to longitude)
        }

        private fun parseContactInfo(text: String, upper: String): Map<String, Any?>? {
            if (!upper.startsWith("BEGIN:VCARD")) return null

            var formattedName: String? = null
            var firstName: String? = null
            var middleName: String? = null
            var lastName: String? = null
            var prefix: String? = null
            var suffix: String? = null
            var organization: String? = null
            var title: String? = null
            val addresses = mutableListOf<Map<String, Any?>>()
            val emails = mutableListOf<Map<String, Any?>>()
            val phones = mutableListOf<Map<String, Any?>>()
            val urls = mutableListOf<String>()

            for (line in unfoldCalendarLines(text)) {
                val separator = line.indexOf(':')
                if (separator <= 0) continue

                val property = line.substring(0, separator)
                val name = property.substringBefore(';').uppercase()
                val params = property
                    .split(';')
                    .drop(1)
                    .map { it.uppercase() }
                val value = unescapeCalendarText(line.substring(separator + 1))

                when (name) {
                    "FN" -> formattedName = value
                    "N" -> {
                        val parts = value.split(';')
                        lastName = parts.getOrNull(0)?.ifBlank { null }
                        firstName = parts.getOrNull(1)?.ifBlank { null }
                        middleName = parts.getOrNull(2)?.ifBlank { null }
                        prefix = parts.getOrNull(3)?.ifBlank { null }
                        suffix = parts.getOrNull(4)?.ifBlank { null }
                    }
                    "ORG" -> organization = value.split(';').firstOrNull()?.ifBlank { null }
                    "TITLE" -> title = value
                    "TEL" -> phones.add(
                        mapOf("number" to value, "type" to phoneTypeFromParams(params)),
                    )
                    "EMAIL" -> emails.add(
                        mapOf(
                            "address" to value,
                            "body" to null,
                            "subject" to null,
                            "type" to emailTypeFromParams(params),
                        ),
                    )
                    "URL" -> urls.add(value)
                    "ADR" -> {
                        val lines = value
                            .split(';')
                            .drop(2)
                            .filter { it.isNotBlank() }
                        addresses.add(
                            mapOf(
                                "addressLines" to lines,
                                "type" to addressTypeFromParams(params),
                            ),
                        )
                    }
                }
            }

            if (formattedName == null &&
                firstName == null &&
                lastName == null &&
                organization == null &&
                title == null &&
                addresses.isEmpty() &&
                emails.isEmpty() &&
                phones.isEmpty() &&
                urls.isEmpty()
            ) {
                return null
            }

            return mapOf(
                "addresses" to addresses,
                "emails" to emails,
                "name" to mapOf(
                    "first" to firstName,
                    "formattedName" to formattedName,
                    "last" to lastName,
                    "middle" to middleName,
                    "prefix" to prefix,
                    "pronunciation" to null,
                    "suffix" to suffix,
                ),
                "organization" to organization,
                "phones" to phones,
                "title" to title,
                "urls" to urls,
            )
        }

        private fun parseCalendarEvent(text: String, upper: String): Map<String, Any?>? {
            if (!upper.contains("BEGIN:VCALENDAR") && !upper.contains("BEGIN:VEVENT")) {
                return null
            }

            val fields = mutableMapOf<String, String>()
            var inEvent = upper.startsWith("BEGIN:VEVENT")

            for (line in unfoldCalendarLines(text)) {
                val separator = line.indexOf(':')
                if (separator <= 0) continue

                val name = line
                    .substring(0, separator)
                    .substringBefore(';')
                    .uppercase()
                val value = unescapeCalendarText(line.substring(separator + 1))

                when (name) {
                    "BEGIN" -> if (value.equals("VEVENT", ignoreCase = true)) inEvent = true
                    "END" -> if (value.equals("VEVENT", ignoreCase = true)) break
                    else -> if (inEvent && name !in fields) fields[name] = value
                }
            }

            if (fields.isEmpty()) return null

            return mapOf(
                "description" to fields["DESCRIPTION"],
                "end" to formatCalendarDate(fields["DTEND"]),
                "location" to fields["LOCATION"],
                "organizer" to fields["ORGANIZER"]?.removeMailtoPrefix(),
                "start" to formatCalendarDate(fields["DTSTART"]),
                "status" to fields["STATUS"],
                "summary" to fields["SUMMARY"],
            )
        }

        private fun parseDriverLicense(text: String, upper: String): Map<String, Any?>? {
            if (!upper.startsWith("@ANSI ") && !upper.contains("\nANSI ")) {
                return null
            }

            val fields = mutableMapOf<String, String>()
            for (line in text.replace("\r\n", "\n").replace('\r', '\n').split('\n')) {
                fields.putAll(parseDriverLicenseFields(line))
            }

            if (fields.isEmpty()) return null

            return mapOf(
                "addressCity" to fields["DAI"],
                "addressState" to fields["DAJ"],
                "addressStreet" to fields["DAG"],
                "addressZip" to fields["DAK"],
                "birthDate" to fields["DBB"],
                "documentType" to "DL",
                "expiryDate" to fields["DBA"],
                "firstName" to (fields["DAC"] ?: fields["DCT"]?.substringBefore(',')),
                "gender" to fields["DBC"],
                "issueDate" to fields["DBD"],
                "issuingCountry" to fields["DCG"],
                "lastName" to fields["DCS"],
                "licenseNumber" to fields["DAQ"],
                "middleName" to (fields["DAD"] ?: fields["DCT"]?.substringAfter(',', "")),
            )
        }

        private fun parseDriverLicenseFields(line: String): Map<String, String> {
            val positions = DRIVER_LICENSE_FIELDS
                .flatMap { field ->
                    Regex(Regex.escape(field)).findAll(line).map { field to it.range.first }
                }
                .sortedBy { it.second }

            if (positions.isEmpty()) return emptyMap()

            val fields = mutableMapOf<String, String>()
            for ((index, entry) in positions.withIndex()) {
                val key = entry.first
                val start = entry.second + key.length
                val end = positions.getOrNull(index + 1)?.second ?: line.length
                val value = line.substring(start, end).trim()
                if (value.isNotEmpty()) {
                    fields[key] = value
                }
            }

            return fields
        }

        private fun parseUrl(text: String, upper: String): Map<String, Any?>? {
            if (upper.startsWith("MEBKM:")) {
                val fields = parseSemicolonFields(text.substringAfter(":", ""))
                fields["URL"]?.let {
                    return mapOf("title" to fields["TITLE"], "url" to it)
                }
            }
            if (upper.startsWith("HTTP://") || upper.startsWith("HTTPS://")) {
                return mapOf("title" to null, "url" to text)
            }
            return null
        }

        private fun parseSemicolonFields(value: String): Map<String, String> {
            val fields = mutableMapOf<String, String>()
            for (field in splitEscapedFields(value)) {
                val separator = field.indexOf(':')
                if (separator <= 0) continue
                val key = field.substring(0, separator).uppercase()
                fields[key] = unescape(field.substring(separator + 1))
            }
            return fields
        }

        private fun unfoldCalendarLines(value: String): List<String> {
            val lines = value.replace("\r\n", "\n").replace('\r', '\n').split('\n')
            val unfolded = mutableListOf<String>()

            for (line in lines) {
                if ((line.startsWith(" ") || line.startsWith("\t")) && unfolded.isNotEmpty()) {
                    unfolded[unfolded.lastIndex] += line.drop(1)
                } else {
                    unfolded.add(line)
                }
            }

            return unfolded
        }

        private fun splitEscapedFields(value: String): List<String> {
            val fields = mutableListOf<String>()
            val field = StringBuilder()
            var escaped = false

            for (character in value) {
                when {
                    escaped -> {
                        field.append('\\')
                        field.append(character)
                        escaped = false
                    }
                    character == '\\' -> escaped = true
                    character == ';' -> {
                        fields.add(field.toString())
                        field.clear()
                    }
                    else -> field.append(character)
                }
            }

            if (escaped) {
                field.append('\\')
            }

            fields.add(field.toString())
            return fields
        }

        private fun parseQuery(query: String): Map<String, String> {
            if (query.isEmpty()) return emptyMap()
            return query.split('&').mapNotNull { pair ->
                val separator = pair.indexOf('=')
                if (separator <= 0) return@mapNotNull null
                decode(pair.substring(0, separator)) to decode(pair.substring(separator + 1))
            }.toMap()
        }

        private fun decode(value: String): String {
            return try {
                URLDecoder.decode(value, StandardCharsets.UTF_8.name())
            } catch (_: IllegalArgumentException) {
                value
            }
        }

        private fun unescape(value: String): String {
            return value
                .replace("\\;", ";")
                .replace("\\:", ":")
                .replace("\\,", ",")
                .replace("\\\\", "\\")
        }

        private fun unescapeCalendarText(value: String): String {
            return value
                .replace("\\n", "\n", ignoreCase = true)
                .replace("\\,", ",")
                .replace("\\;", ";")
                .replace("\\\\", "\\")
        }

        private fun formatCalendarDate(value: String?): String? {
            if (value.isNullOrBlank()) return null
            val normalized = value.trim()

            if (Regex("""^\d{8}$""").matches(normalized)) {
                return "${normalized.substring(0, 4)}-${normalized.substring(4, 6)}-${normalized.substring(6, 8)}"
            }

            val dateTime = Regex("""^(\d{8})T(\d{6})(Z?)$""").matchEntire(normalized)
            if (dateTime != null) {
                val date = dateTime.groupValues[1]
                val time = dateTime.groupValues[2]
                val zone = dateTime.groupValues[3]
                return "${date.substring(0, 4)}-${date.substring(4, 6)}-${date.substring(6, 8)}T" +
                    "${time.substring(0, 2)}:${time.substring(2, 4)}:${time.substring(4, 6)}$zone"
            }

            return normalized
        }

        private fun String.removeMailtoPrefix(): String {
            return if (startsWith("mailto:", ignoreCase = true)) {
                drop("mailto:".length)
            } else {
                this
            }
        }

        private fun phoneTypeFromParams(params: List<String>): Int {
            return when {
                params.hasType("CELL") -> 4
                params.hasType("FAX") -> 3
                params.hasType("HOME") -> 2
                params.hasType("WORK") -> 1
                else -> PHONE_UNKNOWN
            }
        }

        private fun emailTypeFromParams(params: List<String>): Int {
            return when {
                params.hasType("HOME") -> 2
                params.hasType("WORK") -> 1
                else -> EMAIL_UNKNOWN
            }
        }

        private fun addressTypeFromParams(params: List<String>): Int {
            return when {
                params.hasType("HOME") -> 2
                params.hasType("WORK") -> 1
                else -> TYPE_UNKNOWN
            }
        }

        private fun List<String>.hasType(type: String): Boolean {
            return any { param ->
                param == type ||
                    param == "TYPE=$type" ||
                    param.startsWith("TYPE=") && param.substringAfter("TYPE=").split(',').contains(type)
            }
        }

        private fun isProductFormat(format: Int): Boolean {
            return format == FORMAT_EAN_13 ||
                format == FORMAT_EAN_8 ||
                format == FORMAT_UPC_A ||
                format == FORMAT_UPC_E
        }

        private fun isIsbn(value: String): Boolean {
            val normalized = value
                .replace(Regex("(?i)ISBN(?:[- ]?1[03])?"), "")
                .filter { it.isLetterOrDigit() }
            return (normalized.length == 10 && normalized.all { it.isDigit() }) ||
                (normalized.length == 13 &&
                    (normalized.startsWith("978") || normalized.startsWith("979")) &&
                    normalized.all { it.isDigit() }) ||
                (normalized.length == 18 &&
                    (normalized.startsWith("978") || normalized.startsWith("979")) &&
                    normalized.all { it.isDigit() })
        }

        private fun isIssnWithSupplement(value: String): Boolean {
            val normalized = value
                .replace("ISSN", "", ignoreCase = true)
                .filter { it.isLetterOrDigit() }

            if (normalized.length == 15 &&
                normalized.startsWith("977") &&
                normalized.all { it.isDigit() }
            ) {
                return true
            }

            if (!value.startsWith("ISSN", ignoreCase = true) || normalized.length != 10) {
                return false
            }

            val issn = normalized.take(8)
            val supplement = normalized.takeLast(2)
            return isValidIssn(issn) && supplement.all { it.isDigit() }
        }

        private fun isValidIssn(value: String): Boolean {
            if (value.length != 8) return false
            val check = when (val character = value.last().uppercaseChar()) {
                'X' -> 10
                in '0'..'9' -> character.digitToInt()
                else -> return false
            }

            var sum = check
            for (index in 0 until 7) {
                val digit = value[index].digitToIntOrNull() ?: return false
                sum += digit * (8 - index)
            }

            return sum % 11 == 0
        }
    }
}
