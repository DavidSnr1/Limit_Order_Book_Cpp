# Limit Order Book

Ein Limit-Order-Book in C++ mit Preis-Zeit-Priorität-Matching — die Kernkomponente, mit der Börsen (und HFT-Firmen) Kauf- und Verkaufsaufträge zusammenführen.

## Warum das interessant ist

In jedem elektronischen Handelssystem (NASDAQ, NYSE, Krypto-Börsen) trifft eine Order nicht sofort auf einen Handelspartner — sie wartet in einer Warteschlange, sortiert nach Preis und bei gleichem Preis nach Ankunftszeit ("Preis-Zeit-Priorität"). Das Order Book ist die Datenstruktur, die diese Warteschlangen verwaltet und bei jeder neuen Order prüft, ob ein Trade zustande kommt. Latenz zählt hier in Nanosekunden — deshalb ist die Wahl der Datenstrukturen entscheidend, nicht nur die Korrektheit der Logik.

## Projektstruktur

- `types.h` — Basistypen (`OrderId`, `Price`, `Volume` als `uint64_t`; `Side`, `MessageType`)
- `order.h` — das `Order`-Struct (inkl. `next_order`/`prev_order` für die Intrusive List)
- `trade.h` — das `Trade`-Struct (Ergebnis eines Matches)
- `memory_pool.h` / `memory_pool.cpp` — Pre-allocated Pool mit Free-List statt Heap-Allokation pro Order
- `order_book.h` / `order_book.cpp` — die Engine: `add_order`, `cancel_order`, `display`, Matching mit Preis-Zeit-Priorität; Preis-Levels sind Intrusive Linked Lists, deren Knoten aus dem `MemoryPool` kommen
- `feed_simulator.h` / `feed_simulator.cpp` — generiert zufällige Test-Orders
- `main.cpp` — verbindet Simulator und Order Book zu einer lauffähigen Demo
- `tests/test_matching.cpp` — Testfälle für Insert/Match/Cancel (doctest)
- `tests/test_memory_pool.cpp` — Testfälle für den Memory Pool (Allokation, Wiederverwendung, Wachstum, Guard gegen `blockCount == 0`)

## Bauen & Ausführen

Voraussetzung: `g++` (C++17) und `make` — z.B. über [MSYS2](https://www.msys2.org/) unter Windows.

```
make          # baut app.exe und test.exe
./app.exe     # 10 simulierte Orders, Buch-Snapshot nach jeder Order
./test.exe    # Testsuite (doctest)
make clean    # aufräumen
```

## Beispiel-Output

`app.exe` druckt nach jeder eingefügten Order die besten 5 Preisstufen je Seite. Kommt es dabei zu einem Match, wird der Trade live dazwischen ausgegeben:

```
--- BID ---
102 |  [ 103 ]
--- ASK ---
103 |  [ 101 ]
105 |  [ 102 ]

Trade @ Quantity 102 @ Price: 102
--- BID ---
102 |  [ 1 ]
--- ASK ---
103 |  [ 101 ]
105 |  [ 102 ]
```

Jede Buch-Zeile: `Preis | [Restvolumen Order 1] [Restvolumen Order 2] ...`, sortiert nach Preis-Zeit-Priorität (bester Bid/Ask zuerst). Der Trade-Preis ist der Preis der Order, die zuerst im Buch lag (Maker/Resting-Preis) — die neu ankommende Order bekommt ggf. eine Preisverbesserung. Alle Trades landen zusätzlich in `trade_log` (für spätere P&L-Auswertung).

## Aktueller Stand

MVP (Teil 1) ist fertig. Schritt 4/5 der "Receive Side Masterclass" (siehe [TODO.md](TODO.md)) sind jetzt ebenfalls abgeschlossen: `std::list<Order>` ist komplett aus dem Projekt raus, ersetzt durch einen `MemoryPool` + eine Intrusive Linked List (`next_order`/`prev_order` liegen direkt im `Order`-Struct — jeder Preis-Level in `bids`/`asks` ist nur noch ein `OrderQueue{head, tail}`, keine separate Heap-Allokation pro Listenknoten mehr).

Fertig:
- `MemoryPool` (Allokation, Wiederverwendung, Wachstum, Guard gegen `blockCount == 0`) — eigenständig getestet
- `add_order` — Pool-Allokation + Placement-New + Verlinken ans Ende der Queue
- `cancel_order` — Order per `order_map` finden, aus der Kette aushängen (4 Fälle: einziges Element / Kopf / Ende / Mitte), Destruktor + `deallocate`
- `match()` — arbeitet auf `head`/manuellem Aushängen statt `front()`/`pop_front()`
- `display()`, `order_volume()` — auf Zeiger-Traversierung umgestellt

Baut sauber (`make all`), keine Compiler-Warnungen, alle Tests grün.

Als Nächstes — Rest der "Receive Side Masterclass":

- Echter NASDAQ-ITCH-Parser statt Zufalls-Simulator
- Benchmarking: naive `std::list`- vs. Pool-Version, p50/p99-Latenzen
- Lock-Free SPSC Queue zur Trennung von Parser- und Matching-Thread
