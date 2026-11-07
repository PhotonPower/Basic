# platform/pc

C++-Laufzeitschicht des Ziels `pc` (`rt_pc.hpp`, `rt_pc.cpp`): alles, was ein GFA-BASIC-Programm
über `runtime/ref` hinaus zur Laufzeit braucht – Konvertierungen mit GFA-Fehlern, die eingebauten
Funktionen, `PRINT` mit Spalten und `USING`, Fehlertexte, `rt_error`.

Der Referenz-Interpreter (`src/ir/interp.cpp`) und die vom C++-Backend erzeugten Programme benutzen
dieselbe Schicht; damit stimmen sie in diesen Teilen überein.
