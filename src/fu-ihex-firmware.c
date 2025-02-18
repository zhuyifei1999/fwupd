/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"FuFirmware"

#include "config.h"

#include <string.h>

#include "fu-common.h"
#include "fu-firmware-common.h"
#include "fu-ihex-firmware.h"

struct _FuIhexFirmware {
	FuFirmware		 parent_instance;
};

G_DEFINE_TYPE (FuIhexFirmware, fu_ihex_firmware, FU_TYPE_FIRMWARE)

#define	DFU_INHX32_RECORD_TYPE_DATA		0x00
#define	DFU_INHX32_RECORD_TYPE_EOF		0x01
#define	DFU_INHX32_RECORD_TYPE_EXTENDED_SEGMENT	0x02
#define	DFU_INHX32_RECORD_TYPE_START_SEGMENT	0x03
#define	DFU_INHX32_RECORD_TYPE_EXTENDED_LINEAR	0x04
#define	DFU_INHX32_RECORD_TYPE_START_LINEAR	0x05
#define	DFU_INHX32_RECORD_TYPE_SIGNATURE	0xfd

static const gchar *
fu_ihex_firmware_record_type_to_string (guint8 record_type)
{
	if (record_type == DFU_INHX32_RECORD_TYPE_DATA)
		return "DATA";
	if (record_type == DFU_INHX32_RECORD_TYPE_EOF)
		return "EOF";
	if (record_type == DFU_INHX32_RECORD_TYPE_EXTENDED_SEGMENT)
		return "EXTENDED_SEGMENT";
	if (record_type == DFU_INHX32_RECORD_TYPE_START_SEGMENT)
		return "START_SEGMENT";
	if (record_type == DFU_INHX32_RECORD_TYPE_EXTENDED_LINEAR)
		return "EXTENDED_LINEAR";
	if (record_type == DFU_INHX32_RECORD_TYPE_START_LINEAR)
		return "ADDR32";
	if (record_type == DFU_INHX32_RECORD_TYPE_SIGNATURE)
		return "SIGNATURE";
	return NULL;
}

static gboolean
fu_ihex_firmware_parse (FuFirmware *firmware,
			GBytes *fw,
			guint64 addr_start,
			guint64 addr_end,
			FwupdInstallFlags flags,
			GError **error)
{
	const gchar *data;
	gboolean got_eof = FALSE;
	gsize sz = 0;
	guint32 abs_addr = 0x0;
	guint32 addr_last = 0x0;
	guint32 img_addr = G_MAXUINT32;
	guint32 seg_addr = 0x0;
	g_auto(GStrv) lines = NULL;
	g_autoptr(FuFirmwareImage) img = fu_firmware_image_new (NULL);
	g_autoptr(GBytes) img_bytes = NULL;
	g_autoptr(GByteArray) buf = g_byte_array_new ();
	g_autoptr(GByteArray) buf_signature = g_byte_array_new ();

	g_return_val_if_fail (fw != NULL, FALSE);

	/* parse records */
	data = g_bytes_get_data (fw, &sz);
	lines = fu_common_strnsplit (data, sz, "\n", -1);
	for (guint ln = 0; lines[ln] != NULL; ln++) {
		const gchar *line = lines[ln];
		gsize linesz;
		guint32 addr;
		guint8 byte_cnt;
		guint8 record_type;
		guint line_end;

		/* ignore comments */
		if (g_str_has_prefix (line, ";"))
			continue;

		/* ignore blank lines */
		g_strdelimit (lines[ln], "\r\x1a", '\0');
		linesz = strlen (line);
		if (linesz == 0)
			continue;

		/* check starting token */
		if (line[0] != ':') {
			g_set_error (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "invalid starting token on line %u: %s",
				     ln + 1, line);
			return FALSE;
		}

		/* check there's enough data for the smallest possible record */
		if (linesz < 11) {
			g_set_error (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "line %u is incomplete, length %u",
				     ln + 1, (guint) linesz);
			return FALSE;
		}

		/* length, 16-bit address, type */
		byte_cnt = fu_firmware_strparse_uint8 (line + 1);
		addr = fu_firmware_strparse_uint16 (line + 3);
		record_type = fu_firmware_strparse_uint8 (line + 7);
		g_debug ("%s:", fu_ihex_firmware_record_type_to_string (record_type));
		g_debug ("  addr_start:\t0x%04x", addr);
		g_debug ("  length:\t0x%02x", byte_cnt);
		addr += seg_addr;
		addr += abs_addr;
		g_debug ("  addr:\t0x%08x", addr);

		/* position of checksum */
		line_end = 9 + byte_cnt * 2;
		if (line_end > (guint) linesz) {
			g_set_error (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "line %u malformed, length: %u",
				     ln + 1, line_end);
			return FALSE;
		}

		/* verify checksum */
		if ((flags & FWUPD_INSTALL_FLAG_FORCE) == 0) {
			guint8 checksum = 0;
			for (guint i = 1; i < line_end + 2; i += 2) {
				guint8 data_tmp = fu_firmware_strparse_uint8 (line + i);
				checksum += data_tmp;
			}
			if (checksum != 0)  {
				g_set_error (error,
					     FWUPD_ERROR,
					     FWUPD_ERROR_INVALID_FILE,
					     "line %u has invalid checksum (0x%02x)",
					     ln + 1, checksum);
				return FALSE;
			}
		}

		/* process different record types */
		switch (record_type) {
		case DFU_INHX32_RECORD_TYPE_DATA:
			/* base address for element */
			if (img_addr == G_MAXUINT32)
				img_addr = addr;

			/* does not make sense */
			if (addr < addr_last) {
				g_set_error (error,
					     FWUPD_ERROR,
					     FWUPD_ERROR_INVALID_FILE,
					     "invalid address 0x%x, last was 0x%x on line %u",
					     (guint) addr,
					     (guint) addr_last,
					     ln + 1);
				return FALSE;
			}

			/* parse bytes from line */
			g_debug ("writing data 0x%08x", (guint32) addr);
			for (guint i = 9; i < line_end; i += 2) {
				/* any holes in the hex record */
				guint32 len_hole = addr - addr_last;
				guint8 data_tmp;
				if (addr_last > 0 && len_hole > 0x100000) {
					g_set_error (error,
						     FWUPD_ERROR,
						     FWUPD_ERROR_INVALID_FILE,
						     "hole of 0x%x bytes too large to fill on line %u",
						     (guint) len_hole,
						     ln + 1);
					return FALSE;
				}
				if (addr_last > 0x0 && len_hole > 1) {
					g_debug ("filling address 0x%08x to 0x%08x on line %u",
						 addr_last + 1, addr_last + len_hole - 1, ln + 1);
					for (guint j = 1; j < len_hole; j++) {
						/* although 0xff might be clearer,
						 * we can't write 0xffff to pic14 */
						fu_byte_array_append_uint8 (buf, 0x00);
					}
				}
				/* write into buf */
				data_tmp = fu_firmware_strparse_uint8 (line + i);
				fu_byte_array_append_uint8 (buf, (gchar) data_tmp);
				addr_last = addr++;
			}
			break;
		case DFU_INHX32_RECORD_TYPE_EOF:
			if (got_eof) {
				g_set_error_literal (error,
						     FWUPD_ERROR,
						     FWUPD_ERROR_INVALID_FILE,
						     "duplicate EOF, perhaps "
						     "corrupt file");
				return FALSE;
			}
			got_eof = TRUE;
			break;
		case DFU_INHX32_RECORD_TYPE_EXTENDED_LINEAR:
			abs_addr = fu_firmware_strparse_uint16 (line + 9) << 16;
			g_debug ("  abs_addr:\t0x%02x on line %u", abs_addr, ln + 1);
			break;
		case DFU_INHX32_RECORD_TYPE_START_LINEAR:
			abs_addr = fu_firmware_strparse_uint32 (line + 9);
			g_debug ("  abs_addr:\t0x%08x on line %u", abs_addr, ln + 1);
			break;
		case DFU_INHX32_RECORD_TYPE_EXTENDED_SEGMENT:
			/* segment base address, so ~1Mb addressable */
			seg_addr = fu_firmware_strparse_uint16 (line + 9) * 16;
			g_debug ("  seg_addr:\t0x%08x on line %u", seg_addr, ln + 1);
			break;
		case DFU_INHX32_RECORD_TYPE_START_SEGMENT:
			/* initial content of the CS:IP registers */
			seg_addr = fu_firmware_strparse_uint32 (line + 9);
			g_debug ("  seg_addr:\t0x%02x on line %u", seg_addr, ln + 1);
			break;
		case DFU_INHX32_RECORD_TYPE_SIGNATURE:
			for (guint i = 9; i < line_end; i += 2) {
				guint8 tmp_c = fu_firmware_strparse_uint8 (line + i);
				fu_byte_array_append_uint8 (buf_signature, tmp_c);
			}
			break;
		default:
			/* vendors sneak in nonstandard sections past the EOF */
			if (got_eof)
				break;
			g_set_error (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "invalid ihex record type %i on line %u",
				     record_type, ln + 1);
			return FALSE;
		}
	}

	/* no EOF */
	if (!got_eof) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_INVALID_FILE,
				     "no EOF, perhaps truncated file");
		return FALSE;
	}

	/* add single image */
	img_bytes = g_bytes_new (buf->data, buf->len);
	fu_firmware_image_set_bytes (img, img_bytes);
	if (img_addr != G_MAXUINT32)
		fu_firmware_image_set_addr (img, img_addr);
	fu_firmware_add_image (firmware, img);

	/* add optional signature */
	if (buf_signature->len > 0) {
		g_autoptr(GBytes) data_sig = g_bytes_new (buf_signature->data, buf_signature->len);
		g_autoptr(FuFirmwareImage) img_sig = fu_firmware_image_new (data_sig);
		fu_firmware_image_set_id (img_sig, FU_FIRMWARE_IMAGE_ID_SIGNATURE);
		fu_firmware_add_image (firmware, img_sig);
	}
	return TRUE;
}

static void
fu_ihex_firmware_emit_chunk (GString *str,
			     guint16 address,
			     guint8 record_type,
			     const guint8 *data,
			     gsize sz)
{
	guint8 checksum = 0x00;
	g_string_append_printf (str, ":%02X%04X%02X",
				(guint) sz,
				(guint) address,
				(guint) record_type);
	for (gsize j = 0; j < sz; j++)
		g_string_append_printf (str, "%02X", data[j]);
	checksum = (guint8) sz;
	checksum += (guint8) ((address & 0xff00) >> 8);
	checksum += (guint8) (address & 0xff);
	checksum += record_type;
	for (gsize j = 0; j < sz; j++)
		checksum += data[j];
	g_string_append_printf (str, "%02X\n", (guint) (((~checksum) + 0x01) & 0xff));
}

static gboolean
dfu_firmware_to_ihex_image (FuFirmwareImage *img, GString *str, GError **error)
{
	const guint8 *data;
	const guint chunk_size = 16;
	gsize len;
	guint32 address_offset_last = 0x0;
	guint8 record_type = DFU_INHX32_RECORD_TYPE_DATA;
	g_autoptr(GBytes) bytes = NULL;

	/* get data */
	bytes = fu_firmware_image_write (img, error);
	if (bytes == NULL)
		return FALSE;

	/* special case */
	if (g_strcmp0 (fu_firmware_image_get_id (img),
		       FU_FIRMWARE_IMAGE_ID_SIGNATURE) == 0)
		record_type = DFU_INHX32_RECORD_TYPE_SIGNATURE;

	/* get number of chunks */
	data = g_bytes_get_data (bytes, &len);
	for (gsize i = 0; i < len; i += chunk_size) {
		guint32 address_tmp = fu_firmware_image_get_addr (img) + i;
		guint32 address_offset = (address_tmp >> 16) & 0xffff;
		gsize chunk_len = MIN (len - i, 16);

		/* need to offset */
		if (address_offset != address_offset_last) {
			guint8 buf[2];
			fu_common_write_uint16 (buf, address_offset, G_BIG_ENDIAN);
			fu_ihex_firmware_emit_chunk (str, 0x0,
						     DFU_INHX32_RECORD_TYPE_EXTENDED_LINEAR,
						     buf, 2);
			address_offset_last = address_offset;
		}
		address_tmp &= 0xffff;
		fu_ihex_firmware_emit_chunk (str, address_tmp,
					      record_type, data + i, chunk_len);
	}
	return TRUE;
}

static GBytes *
fu_ihex_firmware_write (FuFirmware *firmware, GError **error)
{
	g_autoptr(GPtrArray) imgs = NULL;
	g_autoptr(GString) str = NULL;

	/* write all the element data */
	str = g_string_new ("");
	imgs = fu_firmware_get_images (firmware);
	for (guint i = 0; i < imgs->len; i++) {
		FuFirmwareImage *img = g_ptr_array_index (imgs, i);
		if (!dfu_firmware_to_ihex_image (img, str, error))
			return NULL;
	}

	/* add EOF */
	fu_ihex_firmware_emit_chunk (str, 0x0, DFU_INHX32_RECORD_TYPE_EOF, NULL, 0);
	return g_bytes_new (str->str, str->len);
}

static void
fu_ihex_firmware_init (FuIhexFirmware *self)
{
}

static void
fu_ihex_firmware_class_init (FuIhexFirmwareClass *klass)
{
	FuFirmwareClass *klass_firmware = FU_FIRMWARE_CLASS (klass);
	klass_firmware->parse = fu_ihex_firmware_parse;
	klass_firmware->write = fu_ihex_firmware_write;
}

FuFirmware *
fu_ihex_firmware_new (void)
{
	return FU_FIRMWARE (g_object_new (FU_TYPE_IHEX_FIRMWARE, NULL));
}
