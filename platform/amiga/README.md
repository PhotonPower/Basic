# platform/amiga

Plattformschicht für den Amiga (MC68000): ein AmigaDOS-Programm (Hunk), das `dos.library` öffnet
und über `Output()`/`Write()` ausgibt (gepuffert, geleert bei jedem Zeilenende und am Ende). Den
String-Heap und den Array-Bereich holt `plat_alloc` mit `AllocMem`; `plat_exit` gibt alles frei,
schließt die Bibliothek und kehrt mit dem Rückgabewert (0, bei einem Laufzeitfehler 10) zu AmigaDOS
zurück, auch aus beliebiger Tiefe. Später kommen Fenster, Grafik und Ereignisse über
Intuition/Graphics dazu. Siehe [docs/targets.md](../../docs/targets.md#amiga-mc68000).

| Modul | Aufgabe |
|---|---|
| `plat_start` | Einstieg: Register sichern, `dos.library` öffnen, `Output()`, `main` aufrufen |
| `plat_putc`, `plat_flush` | gepufferte Ausgabe über `Write()` |
| `plat_alloc`, `plat_free_all` | Speicher über `AllocMem`/`FreeMem` |
| `plat_exit` | Ende aus jeder Tiefe: leeren, freigeben, `CloseLibrary`, zurück zu AmigaDOS |
| `plat_file` | Dateien über `dos.library` (`Open`, `Read`, `Write`, `Seek`, `Close`, `DeleteFile`, `Lock`), Fehler aus `IoErr()` |
| `plat_error` | nicht abgefangener Laufzeitfehler: Meldung, Rückgabewert 10 |
