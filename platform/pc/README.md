# platform/pc

C++-Laufzeitschicht des Ziels `pc` (`rt_pc.hpp`, `rt_pc.cpp`): alles, was ein GFA-BASIC-Programm
über `runtime/ref` hinaus zur Laufzeit braucht – Konvertierungen mit GFA-Fehlern, die eingebauten
Funktionen, `PRINT` mit Spalten und `USING`, Fehlertexte, `rt_error`.

Der Referenz-Interpreter (`src/ir/interp.cpp`) und die vom C++-Backend erzeugten Programme benutzen
dieselbe Schicht; damit stimmen sie in diesen Teilen überein.

`rt_prog.hpp` enthält zusätzlich, was nur erzeugte Programme brauchen: Variablen mit Umleitung für
`VAR` (D15), Rahmen zum Sichern von Parametern und `LOCAL`, Arrays mit Indexprüfung,
Fehlerbehandlung mit `RESUME`, `FOR`-Schritte, `DATA` und den Programmstart (`brt::run`).

Ein erzeugtes Programm übersetzen (C++17):

```sh
basicc --target pc programm.lst -o programm.cpp
c++ -std=c++17 -I platform/pc -I runtime/ref programm.cpp build/libbasic_rt_pc.a build/runtime/ref/libbasic_rt_ref.a
```
