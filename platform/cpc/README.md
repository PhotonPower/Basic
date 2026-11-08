# platform/cpc

Plattformschicht für den Amstrad CPC (Z80): Ladeadresse `&0100`, eigener Stapel bis `&A600`, Felder
und Strings im freien Speicher dazwischen (`plat_heap.s`), `plat_putc` über `TXT OUTPUT` (`&BB5A`,
CR vor LF), Tastatur über `KM WAIT CHAR`/`KM READ CHAR` mit Echo, `TIMER` über `KL TIME PLEASE`.
Geplant: Grafik über `GRA`, Dateien über AMSDOS (eine Eingabe- und eine Ausgabedatei). Siehe
[docs/targets.md](../../docs/targets.md#cpc-z80).
