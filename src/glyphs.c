#include <unicode/utf8.h>

#include "statbar.h"

char mail_glyph[5];
char volume_glyph[5];
char battery_glyph[5];
char battery_mid_glyph[5];
char plug_glyph[5];
char battery_low_glyph[5];
char unknown_glyph[5];

void
init_glyphs(void)
{
	UChar32 mail_cp = 0xeb1c;
	UChar32 volume_cp = 0xf028;
	UChar32 battery_cp = 0xf241;
	UChar32 batt_mid_cp = 0xf242;
	UChar32 plug_cp = 0xf492;
	UChar32 batt_low_cp = 0xf243;
	UChar32 unknown_cp = 0xeb32;
	int32_t i = 0;

	U8_APPEND_UNSAFE(mail_glyph, i, mail_cp);
	i = 0;
	U8_APPEND_UNSAFE(volume_glyph, i, volume_cp);
	i = 0;
	U8_APPEND_UNSAFE(battery_glyph, i, battery_cp);
	i = 0;
	U8_APPEND_UNSAFE(battery_mid_glyph, i, batt_mid_cp);
	i = 0;
	U8_APPEND_UNSAFE(plug_glyph, i, plug_cp);
	i = 0;
	U8_APPEND_UNSAFE(battery_low_glyph, i, batt_low_cp);
	i = 0;
	U8_APPEND_UNSAFE(unknown_glyph, i, unknown_cp);
}
