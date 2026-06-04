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
            "calendarEvent" to null,
            "contactInfo" to null,
            "corners" to cornerList,
            "displayValue" to text,
            "driverLicense" to null,
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
            parseUrl(trimmed, upper)?.let {
                return ParsedPayload(TYPE_URL, url = it)
            }

            return when {
                upper.startsWith("BEGIN:VCARD") -> ParsedPayload(TYPE_CONTACT)
                upper.startsWith("BEGIN:VCALENDAR") -> ParsedPayload(TYPE_CALENDAR)
                upper.startsWith("@ANSI ") -> ParsedPayload(TYPE_DRIVER_LICENSE)
                isIsbn(trimmed) -> ParsedPayload(TYPE_ISBN)
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
            for (field in value.split(';')) {
                val separator = field.indexOf(':')
                if (separator <= 0) continue
                val key = field.substring(0, separator).uppercase()
                fields[key] = unescape(field.substring(separator + 1))
            }
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

        private fun isProductFormat(format: Int): Boolean {
            return format == FORMAT_EAN_13 ||
                format == FORMAT_EAN_8 ||
                format == FORMAT_UPC_A ||
                format == FORMAT_UPC_E
        }

        private fun isIsbn(value: String): Boolean {
            val normalized = value
                .replace("ISBN", "", ignoreCase = true)
                .replace("-", "")
                .replace(" ", "")
            return (normalized.length == 10 && normalized.all { it.isDigit() }) ||
                (normalized.length == 13 &&
                    (normalized.startsWith("978") || normalized.startsWith("979")) &&
                    normalized.all { it.isDigit() })
        }
    }
}
