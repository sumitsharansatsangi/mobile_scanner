package dev.steenbakker.mobile_scanner.engine

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull

internal class ZxingBarcodeDataTest {
    @Test
    fun data_parsesWifiPayload() {
        val barcode = barcode("WIFI:T:WPA;S:Office WiFi;P:secret123;;")

        val wifi = assertNotNull(barcode.data["wifi"] as Map<*, *>?)

        assertEquals(2, wifi["encryptionType"])
        assertEquals("Office WiFi", wifi["ssid"])
        assertEquals("secret123", wifi["password"])
    }

    @Test
    fun data_parsesEscapedWifiSeparators() {
        val barcode = barcode("WIFI:T:WPA;S:Office\\;Guest\\:5G;P:sec\\;ret\\:123\\\\end;;")

        val wifi = assertNotNull(barcode.data["wifi"] as Map<*, *>?)

        assertEquals(2, wifi["encryptionType"])
        assertEquals("Office;Guest:5G", wifi["ssid"])
        assertEquals("sec;ret:123\\end", wifi["password"])
    }

    @Test
    fun data_detectsIsbn13WithFiveDigitSupplement() {
        val barcode = barcode("ISBN-13: 9780306406157 90000")

        assertEquals(3, barcode.data["type"])
    }

    @Test
    fun data_detectsIssnWithTwoDigitSupplement() {
        val barcode = barcode("ISSN 2434-561X 12")

        assertEquals(5, barcode.data["type"])
    }

    @Test
    fun data_detectsEncodedIssnWithTwoDigitSupplement() {
        val barcode = barcode("9772434561003 12")

        assertEquals(5, barcode.data["type"])
    }

    @Test
    fun data_parsesCalendarEventPayload() {
        val barcode = barcode(
            """
            BEGIN:VCALENDAR
            VERSION:2.0
            BEGIN:VEVENT
            SUMMARY:Team Sync
            DESCRIPTION:Discuss roadmap\, blockers\; and next steps
            DTSTART:20260605T143000Z
            DTEND:20260605T150000Z
            LOCATION:Room 42
            ORGANIZER:mailto:lead@example.com
            STATUS:CONFIRMED
            END:VEVENT
            END:VCALENDAR
            """.trimIndent(),
        )

        val event = assertNotNull(barcode.data["calendarEvent"] as Map<*, *>?)

        assertEquals(11, barcode.data["type"])
        assertEquals("Team Sync", event["summary"])
        assertEquals("Discuss roadmap, blockers; and next steps", event["description"])
        assertEquals("2026-06-05T14:30:00Z", event["start"])
        assertEquals("2026-06-05T15:00:00Z", event["end"])
        assertEquals("Room 42", event["location"])
        assertEquals("lead@example.com", event["organizer"])
        assertEquals("CONFIRMED", event["status"])
    }

    @Test
    fun data_parsesDirectVeventPayloadWithFoldedLines() {
        val barcode = barcode(
            """
            BEGIN:VEVENT
            SUMMARY:Long team
             meeting
            DTSTART;VALUE=DATE:20260605
            END:VEVENT
            """.trimIndent(),
        )

        val event = assertNotNull(barcode.data["calendarEvent"] as Map<*, *>?)

        assertEquals(11, barcode.data["type"])
        assertEquals("Long teammeeting", event["summary"])
        assertEquals("2026-06-05", event["start"])
    }

    @Test
    fun data_parsesCalendarEventPayloadAfterLeadingTitle() {
        val barcode = barcode(
            """
            My Event
            BEGIN:VEVENT
            SUMMARY:My Event
            DESCRIPTION:Description
            LOCATION:Location
            DTSTART:20260605T081711
            END:VEVENT
            """.trimIndent(),
        )

        val event = assertNotNull(barcode.data["calendarEvent"] as Map<*, *>?)

        assertEquals(11, barcode.data["type"])
        assertEquals("My Event", event["summary"])
        assertEquals("Description", event["description"])
        assertEquals("Location", event["location"])
        assertEquals("2026-06-05T08:17:11", event["start"])
    }

    @Test
    fun data_parsesVcardContactInfoPayload() {
        val barcode = barcode(
            """
            BEGIN:VCARD
            VERSION:3.0
            N:Doe;Jane;Q.;Dr.;PhD
            FN:Dr. Jane Q. Doe
            ORG:Example Corp;Research
            TITLE:Director
            TEL;TYPE=CELL:+15551234567
            TEL;TYPE=WORK:+15557654321
            EMAIL;TYPE=WORK:jane@example.com
            ADR;TYPE=WORK:;;123 Main St;Springfield;IL;62704;USA
            URL:https://example.com/jane
            END:VCARD
            """.trimIndent(),
        )

        val contactInfo = assertNotNull(barcode.data["contactInfo"] as Map<*, *>?)
        val name = assertNotNull(contactInfo["name"] as Map<*, *>?)
        val phones = contactInfo["phones"] as List<*>
        val emails = contactInfo["emails"] as List<*>
        val addresses = contactInfo["addresses"] as List<*>
        val urls = contactInfo["urls"] as List<*>

        assertEquals(1, barcode.data["type"])
        assertEquals("Dr. Jane Q. Doe", name["formattedName"])
        assertEquals("Jane", name["first"])
        assertEquals("Q.", name["middle"])
        assertEquals("Doe", name["last"])
        assertEquals("Dr.", name["prefix"])
        assertEquals("PhD", name["suffix"])
        assertEquals("Example Corp", contactInfo["organization"])
        assertEquals("Director", contactInfo["title"])
        assertEquals("+15551234567", (phones[0] as Map<*, *>)["number"])
        assertEquals(4, (phones[0] as Map<*, *>)["type"])
        assertEquals("+15557654321", (phones[1] as Map<*, *>)["number"])
        assertEquals(1, (phones[1] as Map<*, *>)["type"])
        assertEquals("jane@example.com", (emails.single() as Map<*, *>)["address"])
        assertEquals(1, (emails.single() as Map<*, *>)["type"])
        assertEquals(
            listOf("123 Main St", "Springfield", "IL", "62704", "USA"),
            (addresses.single() as Map<*, *>)["addressLines"],
        )
        assertEquals(1, (addresses.single() as Map<*, *>)["type"])
        assertEquals(listOf("https://example.com/jane"), urls)
    }

    @Test
    fun data_parsesAamvaDriverLicensePayload() {
        val barcode = barcode(
            """
            @
            ANSI 636000090002DL00410288ZA03290015DLDAQD1234567
            DCSDOE
            DACJANE
            DADQUINN
            DBB06051990
            DBA06052030
            DBD06052024
            DBC2
            DAG123 MAIN ST
            DAISPRINGFIELD
            DAJIL
            DAK627040000
            DCGUSA
            """.trimIndent(),
        )

        val driverLicense = assertNotNull(barcode.data["driverLicense"] as Map<*, *>?)

        assertEquals(12, barcode.data["type"])
        assertEquals("DOE", driverLicense["lastName"])
        assertEquals("JANE", driverLicense["firstName"])
        assertEquals("QUINN", driverLicense["middleName"])
        assertEquals("D1234567", driverLicense["licenseNumber"])
        assertEquals("06051990", driverLicense["birthDate"])
        assertEquals("06052030", driverLicense["expiryDate"])
        assertEquals("06052024", driverLicense["issueDate"])
        assertEquals("2", driverLicense["gender"])
        assertEquals("123 MAIN ST", driverLicense["addressStreet"])
        assertEquals("SPRINGFIELD", driverLicense["addressCity"])
        assertEquals("IL", driverLicense["addressState"])
        assertEquals("627040000", driverLicense["addressZip"])
        assertEquals("USA", driverLicense["issuingCountry"])
        assertEquals("DL", driverLicense["documentType"])
    }

    private fun barcode(text: String): ZxingBarcode {
        return ZxingBarcode(
            format = 256,
            text = text,
            rawBytes = text.encodeToByteArray(),
            corners = floatArrayOf(0f, 0f, 10f, 0f, 10f, 10f, 0f, 10f),
        )
    }
}
