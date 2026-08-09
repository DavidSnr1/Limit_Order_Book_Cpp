# ITCH Parser: Konzept & Implementierungsplan

Dieses Dokument ist die Grundlage für Schritt 6 aus [TODO.md](TODO.md). Teil 1 erklärt, was NASDAQ ITCH ist und wie das Format funktioniert. Teil 2 ist die konkrete Baureihenfolge, angepasst an den aktuellen Stand des Projekts (`OrderBook` mit `MemoryPool` + Intrusive List, `add_order`/`cancel_order`, siehe [order_book.h](order_book.h)).

**Wichtiger Hinweis zur Genauigkeit:** die Byte-Layouts der einzelnen Nachrichtentypen in Teil 1 sind aus meinem Wissen über ITCH 5.0 rekonstruiert, nicht live gegen die offizielle NASDAQ-Spezifikation (das "NASDAQ TotalView-ITCH 5.0 Specification" PDF) geprüft. Bevor du die Feld-Offsets 1:1 in Code gießt, verifiziere sie gegen das offizielle Dokument oder eine Referenzimplementierung. Die Grundstruktur (Framing, Big-Endian, welche Message-Typen es gibt, was sie grob enthalten) ist verlässlich, die exakten Byte-für-Byte-Offsets solltest du gegenchecken.

---

## Teil 1: Wie ITCH funktioniert

### 1.1 Was ITCH überhaupt ist

NASDAQ TotalView-ITCH ist das Marktdaten-Protokoll, mit dem die Börse in Echtzeit jede Veränderung am Orderbuch nach außen meldet. Jede eingehende Order, jede Ausführung, jede Löschung wird als eigene, kompakte Binärnachricht rausgeschickt. Anders als bei einer REST-API oder einem menschenlesbaren Format ist das bewusst so kompakt und binär wie möglich gehalten, weil bei einer Börse mit Millionen Nachrichten pro Sekunde jedes Byte und jede Nanosekunde Verarbeitungszeit zählt.

Für dich ist relevant: NASDAQ veröffentlicht historische ITCH-5.0-Dateien zum Download (Sample-Daten für genau diesen Zweck, üblicherweise als `.gz`-komprimierte Dateien mit einem Datum im Namen). Eine solche Datei ist einfach eine lange Aneinanderreihung von Nachrichten, so wie sie chronologisch über den Tag rausgeschickt wurden. Du liest die Datei einmal komplett durch (offline, kein Netzwerk, kein Multicast nötig) und "spielst" die Nachrichten nacheinander ab, so als würdest du live mithören.

### 1.2 Das Framing: wie eine Datei aufgebaut ist

Jede Nachricht im File-Format hat diese Form:

```
[2 Byte Länge][1 Byte Message-Type][Payload...]
```

Die 2-Byte-Länge ist Big-Endian und gibt an, wie viele Bytes ab dem Message-Type-Byte folgen (Type-Byte selbst mitgezählt, die 2 Längen-Bytes selbst nicht). Dein Lese-Loop ist im Kern immer derselbe Ablauf:

```
solange noch Bytes in der Datei übrig sind:
    lies 2 Bytes  -> das ist N, die Länge der folgenden Nachricht
    lies N Bytes  -> das ist die komplette Nachricht (Type-Byte + Payload)
    schau dir das erste Byte an  -> das ist der Message-Type ('A', 'D', 'E', ...)
    parse den Rest je nach Type
```

Das ist der einzige Ort, an dem du wirklich "durch die Datei navigierst". Alles danach ist reines Feld-Parsing innerhalb eines bereits vollständig eingelesenen Nachrichten-Puffers.

### 1.3 Big-Endian: die Falle, die du explizit behandeln musst

#### 1.3.1 Was "Endianness" überhaupt bedeutet

Ein `uint16_t` ist eine Zahl, die 16 Bit (2 Byte) braucht. Wenn diese Zahl irgendwo gespeichert oder über eine Leitung geschickt wird, muss man sich auf eine Reihenfolge einigen, in welcher der 2 Bytes zuerst kommt: das "wichtige" (hochwertige) Byte oder das "unwichtige" (niederwertige) Byte.

Nimm die Dezimalzahl 4660. In Hexadezimal ist das `0x1234`. Als 16-Bit-Zahl besteht sie aus zwei Bytes:
- High Byte: `0x12` (steht für den Anteil `0x1200` = 4608)
- Low Byte: `0x34` (steht für den Anteil `0x34` = 52)
- Zusammen: 4608 + 52 = 4660. Passt.

**Big-Endian** heißt: das High Byte wird zuerst geschickt/gespeichert, dann das Low Byte. Also landen im Puffer die zwei Bytes in der Reihenfolge `[0x12, 0x34]` — genau in der Reihenfolge, in der ein Mensch die Hex-Zahl auch aufschreiben würde. ITCH kodiert alle Mehrbyte-Zahlen so.

**Little-Endian** ist das Gegenteil: zuerst das Low Byte, dann das High Byte, also `[0x34, 0x12]`. x86/x64-Prozessoren (dein Rechner) speichern Mehrbyte-Werte intern so.

Das Problem: wenn du die zwei ankommenden Bytes `[0x12, 0x34]` einfach 1:1 als rohen Speicher in einen `uint16_t` uminterpretierst (z.B. per Pointer-Cast oder `memcpy`), fasst dein Little-Endian-Prozessor das erste Byte im Puffer automatisch als das **niederwertige** auf — er würde `0x3412` lesen (13330), nicht `0x1234` (4660). Falscher Wert, und zwar nicht zufällig falsch, sondern systematisch falsch auf jede einzelne Zahl.

#### 1.3.2 Warum die manuelle Lösung funktioniert: Bit für Bit durchgerechnet

Die Lösung ist, die Bytes nicht als rohen Speicherblock zu interpretieren, sondern jedes Byte einzeln als **normale Zahl zwischen 0 und 255** zu lesen und sie per Bit-Arithmetik selbst an die richtige Stelle zu schieben. Das ist komplett unabhängig davon, wie dein Prozessor intern Zahlen ablegt — du rechnest ja nur mit Werten, nicht mit rohem Speicher.

Nimm wieder `p[0] = 0x12`, `p[1] = 0x34` (also 18 und 52 in Dezimal).

**Schritt 1 — `p[0] << 8` (Linksschieben um 8 Bit):**
`0x12` in Binär ist `0001 0010`. Ein Byte hat 8 Bit. Wenn du diesen Wert in einen `uint16_t` (16 Bit) packst und um 8 Stellen nach links schiebst, wandern alle Bits in die obere Hälfte der 16 Bit, die untere Hälfte wird mit Nullen aufgefüllt:

```
vorher (als 16-Bit-Wert):  0000 0000 0001 0010     (das ist 0x0012 = 18)
nach << 8:                 0001 0010 0000 0000     (das ist 0x1200 = 4608)
```

Das "Warum 8 Bit": ein Byte hat exakt 8 Bit. Um einen Wert von "der untersten Byte-Position" in "die nächsthöhere Byte-Position" zu verschieben, ist ein Shift um 8 der einzige Weg, der genau eine ganze Byte-Breite bewegt.

**Schritt 2 — `p[1]` bleibt unverändert:**
`0x34` in Binär ist `0011 0100`, als 16-Bit-Wert `0000 0000 0011 0100` (also `0x0034` = 52). Das bleibt so, weil es ja schon an der richtigen (niedrigsten) Stelle steht.

**Schritt 3 — beide mit OR verknüpfen:**
```
0001 0010 0000 0000   (0x1200, aus Schritt 1)
0000 0000 0011 0100   (0x0034, aus Schritt 2)
OR (bitweise, pro Position: 1 wenn mindestens eine Seite 1 hat)
---------------------
0001 0010 0011 0100   = 0x1234 = 4660
```

Das funktioniert sauber per OR (statt z.B. Addition), weil sich die beiden Bit-Muster nach dem Schieben **nie überlappen** — die oberen 8 Bit kommen exklusiv aus `p[0]`, die unteren 8 Bit exklusiv aus `p[1]`. OR ist hier nur die übliche, leicht lesbare Art, "kombiniere zwei nicht überlappende Bit-Bereiche" auszudrücken; Addition würde im konkreten Fall zum selben Ergebnis führen, ist aber weniger idiomatisch.

Damit ist der Code aus dem vorherigen Abschnitt genau das:
```cpp
uint16_t read_u16_be(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}
```
`p[0]` ist das High Byte (kommt zuerst bei Big-Endian) → wird 8 Bit nach links geschoben, damit es an der hochwertigen Position landet. `p[1]` ist das Low Byte → bleibt, wie es ist. OR verklebt beide zur vollständigen 16-Bit-Zahl.

#### 1.3.3 Dasselbe Prinzip für mehr Bytes

Für ein 4-Byte-Feld (`uint32_t`) mit Bytes `[p0, p1, p2, p3]` (p0 = höchstwertig, kommt zuerst) gilt exakt dieselbe Logik, nur mit vier Positionen statt zwei. Jedes Byte muss so weit nach links geschoben werden, wie viele Byte-Breiten (à 8 Bit) es von der niedrigsten Position entfernt ist:

| Byte | Abstand zur untersten Position | Shift |
|---|---|---|
| `p0` (höchstwertig) | 3 Byte | `<< 24` (3 × 8) |
| `p1` | 2 Byte | `<< 16` (2 × 8) |
| `p2` | 1 Byte | `<< 8` (1 × 8) |
| `p3` (niederwertig) | 0 Byte | kein Shift |

```cpp
uint32_t read_u32_be(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}
```

Ganz wichtig, sonst ein häufiger Bug: `uint32_t(p[0])` **vor** dem Schieben. Würdest du `p[0] << 24` direkt schreiben, wäre `p[0]` immer noch vom Typ `uint8_t` (8 Bit!) und ein Schieben um 24 Bit auf einem 8-Bit-Wert ist entweder undefiniertes Verhalten oder verwirft die Bits sofort wieder (abhängig von impliziten Konvertierungsregeln) — der Wert muss erst auf `uint32_t` erweitert werden, bevor er verschoben wird, damit überhaupt Platz für die verschobenen Bits da ist.

Für 6 und 8 Byte lohnt sich statt einzelner Shift-Ausdrücke eine Schleife, die dasselbe Prinzip iterativ macht:

```cpp
uint64_t read_u48_be(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 6; ++i) v = (v << 8) | p[i];
    return v;
}
```
Was hier pro Durchlauf passiert: `v` enthält schon alle bisher verarbeiteten Bytes. `v << 8` schiebt den bisherigen Inhalt um eine Byte-Breite nach links (macht "Platz" für ein neues Byte ganz unten), `| p[i]` setzt das neue Byte in die frei gewordene unterste Position. Nach 6 Durchläufen stehen alle 6 Bytes in der richtigen Reihenfolge in den unteren 48 Bit von `v`, die oberen 16 Bit von `v` (weil `uint64_t` 64 Bit hat, wir aber nur 48 Bit befüllen) bleiben 0. `read_u64_be` ist identisch, nur mit 8 statt 6 Durchläufen.

**Warum diese Technik unabhängig von der Endianness deines eigenen Prozessors funktioniert:** du liest nie einen rohen Speicherblock direkt als Mehrbyte-Zahl (das wäre der Fall, der von der Prozessor-Endianness abhinge). Stattdessen liest du jedes Byte einzeln als das, was es ist — eine Zahl zwischen 0 und 255 — und baust die größere Zahl explizit selbst durch Bit-Arithmetik zusammen. Diese Arithmetik (Schieben, OR) verhält sich auf jedem Prozessor identisch, weil du nicht mit Speicher-Layout arbeitest, sondern mit Werten in CPU-Registern. Genau deshalb ist dieser Ansatz portabel und der Standardweg, um Netzwerk-/Dateiformate zu parsen, ohne sich um die Zielplattform kümmern zu müssen.

### 1.4 Die Message-Typen, die für ein Orderbuch relevant sind

Es gibt in ITCH 5.0 deutlich mehr Nachrichtentypen (System-Events, Handelsstatus, Stock-Directory, Cross-Trades, ...), aber für die Rekonstruktion des Orderbuchs brauchst du im Kern diese sechs:

**`A` — Add Order (No MPID Attribution)**
Eine neue Order kommt ins Buch. Typische Feldreihenfolge:
| Feld | Größe | Bedeutung |
|---|---|---|
| Stock Locate | 2 Byte | numerische ID für das Symbol (siehe 1.6) |
| Tracking Number | 2 Byte | intern, meist ignorierbar |
| Timestamp | 6 Byte | Nanosekunden seit Mitternacht |
| Order Reference Number | 8 Byte | eindeutige ID dieser Order, entspricht deiner `OrderId` |
| Buy/Sell Indicator | 1 Byte | `'B'` oder `'S'` |
| Shares | 4 Byte | Ordergröße |
| Stock | 8 Byte | Ticker-Symbol, ASCII, mit Leerzeichen aufgefüllt |
| Price | 4 Byte | Fixed-Point, tatsächlicher Preis × 10000 |

**`F` — Add Order (With MPID Attribution)**
Wie `A`, plus ein zusätzliches 4-Byte-Feld für die Market Participant ID am Ende. Für dein Orderbuch inhaltlich identisch zu `A`, die Attribution kannst du ignorieren.

**`D` — Order Delete**
Eine Order verschwindet komplett aus dem Buch (nicht ausgeführt, sondern zurückgezogen). Enthält nur die Order Reference Number, sonst nichts weiter Relevantes.

**`X` — Order Cancel**
Ein **Teil** einer Order wird storniert (die Order bleibt bestehen, nur mit weniger Volumen). Enthält Order Reference Number + Canceled Shares. Wichtig: das ist **keine** vollständige Löschung, sondern eine Volumen-Reduktion.

**`E` — Order Executed**
Ein Teil (oder das gesamte Restvolumen) einer Order wurde ausgeführt, zum ursprünglichen Limit-Preis der Order. Enthält Order Reference Number, Executed Shares, eine Match Number. Auch das ist primär eine Volumen-Reduktion, kein Preis-Wechsel.

**`C` — Order Executed With Price**
Wie `E`, aber die Ausführung passierte zu einem anderen Preis als dem ursprünglichen Limit (relevant bei bestimmten Cross-/Auction-Situationen). Enthält zusätzlich ein Printable-Flag und den tatsächlichen Ausführungspreis.

**`U` — Order Replace**
Eine bestehende Order wird durch eine neue ersetzt (z.B. bei einer Preis-/Mengenänderung, die die Zeitpriorität zurücksetzt). Enthält die alte **und** eine neue Order Reference Number, plus neue Shares und neuer Preis. Im Kern: "lösch die alte Referenz, füg eine neue Order mit neuer ID ein".

### 1.5 Der zentrale Design-Fork: Rekonstruktion vs. Simulator-Ersatz

Das ist die wichtigste Entscheidung, bevor du irgendetwas an `OrderBook` anschließt, und sie hat direkte Konsequenzen für deinen Code:

**ITCH beschreibt einen Buchzustand, den die Börse bereits selbst fertig verwaltet hat.** Wenn eine Order gegen eine andere ausgeführt wird, weißt du das explizit aus einer `E`/`C`-Nachricht — die Börse hat längst entschieden, wer mit wem handelt, basierend auf Informationen (z.B. verborgene Iceberg-Anteile, andere Marktteilnehmer), die du in den öffentlichen ITCH-Daten gar nicht vollständig siehst.

Dein `OrderBook::add_order()` ruft aktuell automatisch `match()` auf ([order_book.cpp:39](order_book.cpp:39)). Würdest du ITCH-`A`-Nachrichten einfach durch dieses `add_order()` schicken, würde **deine eigene Matching-Engine** anfangen, Orders gegeneinander zu matchen — und zwar nach deiner eigenen Preis-Zeit-Priorität-Logik, die von der tatsächlichen Entscheidung der Börse abweichen kann. Dein rekonstruiertes Buch würde dann vom echten Orderbuch abweichen, obwohl du "echte Daten" verwendest.

Zwei ehrliche Wege:

**Option A: Echte Rekonstruktion (empfohlen, das ist der "Wow-Effekt")**
Du baust einen Buchzustand nach, der **niemals selbst matched**. `A` fügt nur ein, `D`/`X`/`E`/`C` reduzieren/entfernen nur, ohne dass deine `match()`-Logik je aufgerufen wird. Das bedeutet konkret:
- Ein neuer Einfüge-Pfad in `OrderBook`, der **kein** `match()` aufruft (z.B. `add_resting_order(Order)`, parallel zu `add_order()`).
- Eine neue Methode `reduce_volume(OrderId id, Volume qty)` für `X` und `E`/`C` — die gibt es bei dir aktuell nicht, nur volles `cancel_order()`.
- `D` bildet direkt auf dein bestehendes `cancel_order()` ab.
- `U` bildet auf `cancel_order(alte_id)` + `add_resting_order(neue_order)` ab.

**Option B: ITCH als realistischerer Simulator-Ersatz**
Du ignorierst `D`/`X`/`E`/`C`/`U` komplett und nutzt nur `A`-Nachrichten als Quelle für realistische Preis-/Volumen-/Timing-Muster, die du ganz normal durch dein bestehendes `add_order()` (mit deinem eigenen Matching) laufen lässt. Dein Buch matched dann selbst, mit echten Ankunftsmustern statt Zufallszahlen aus `feed_simulator`.

Option A ist die, die wirklich einlöst, was dein TODO mit "läuft auf echten Börsendaten" meint. Sie bedeutet aber mehr Arbeit an `OrderBook` selbst. Option B ist deutlich weniger Aufwand (im Grunde nur `feed_simulator` durch eine ITCH-Quelle ersetzen), verliert aber den Anspruch "das ist wirklich das echte Buch".

### 1.6 Symbole und Stock Locate

ITCH-Daten decken **alle** an der Börse gehandelten Symbole gleichzeitig ab, nicht nur eines. Jede Nachricht trägt eine "Stock Locate"-Nummer (2 Byte) statt des Ticker-Klartexts, das ist eine kompakte numerische Kurzform. Die Zuordnung Nummer-zu-Symbol kommt aus separaten `R`-Nachrichten (Stock Directory), die zu Beginn der Datei einmal pro Symbol gesendet werden.

Für ein Lernprojekt willst du das vermutlich nicht für alle paar Tausend Symbole gleichzeitig verarbeiten. Praktikabel: beim Parsen der `R`-Nachrichten eine `stock_locate -> Ticker-String`-Map aufbauen, dann beim Verarbeiten aller anderen Nachrichten nur die Stock-Locate-Nummer(n) durchlassen, die zu deinem gewünschten Test-Symbol gehören (z.B. nur "AAPL"), alles andere verwerfen.

---

## Teil 2: Konkrete Baureihenfolge

Die Reihenfolge ist bewusst so gewählt, dass du nach jedem Schritt etwas Lauffähiges und Testbares hast, statt erst am Ende alles gleichzeitig zusammenzustecken.

### Schritt 1: Byte-Lese-Hilfsfunktionen + Tests

Neue Datei `byte_utils.h`: die vier Funktionen aus 1.3 (`read_u16_be`, `read_u32_be`, `read_u48_be`, `read_u64_be`), genau wie dort hergeleitet. Dazu sofort ein `tests/test_byte_utils.cpp` mit doctest:

```cpp
#include "doctest.h"
#include "../byte_utils.h"

TEST_CASE("read_u16_be combines two bytes correctly") {
    uint8_t buf[] = {0x12, 0x34};
    CHECK(read_u16_be(buf) == 0x1234);
}

TEST_CASE("read_u16_be treats the first byte as most significant") {
    uint8_t low[]  = {0x00, 0x01};
    uint8_t high[] = {0x01, 0x00};
    CHECK(read_u16_be(low)  == 1);
    CHECK(read_u16_be(high) == 256);   // 0x01 << 8 = 256
}

TEST_CASE("read_u32_be handles all four bytes") {
    uint8_t buf[] = {0x00, 0x01, 0x02, 0x03};
    CHECK(read_u32_be(buf) == 0x00010203);
}

TEST_CASE("read_u48_be handles the timestamp width") {
    uint8_t buf[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    CHECK(read_u48_be(buf) == 1);
}
```

Der zweite Testfall (`low`/`high`) ist bewusst so gewählt, dass er den Fehler sofort aufdecken würde, den man am ehesten macht (Bytes vertauscht): `{0x00, 0x01}` und `{0x01, 0x00}` müssen unterschiedliche Ergebnisse liefern, und zwar genau `1` bzw. `256`, nicht umgekehrt. Das ist die Grundlage für alles Weitere. Fehler hier sind besonders tückisch, weil sie sich erst viele Schritte später als "komische Preise/Mengen" zeigen würden, nicht als klarer Absturz. Lieber jetzt isoliert testen, bevor irgendetwas anderes darauf aufbaut.

### Schritt 2: Nur das Framing, ohne Feld-Parsing

Bevor du Nachrichteninhalte parst, musst du erstmal überhaupt zuverlässig durch die Datei navigieren können. Das ist reine Datei-I/O-Mechanik in C++, unabhängig von ITCH selbst:

```cpp
#include <fstream>
#include <vector>
#include <map>
#include <iostream>

void count_message_types(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "could not open " << path << "\n";
        return;
    }

    std::map<char, int> counts;
    uint8_t length_buf[2];

    while (file.read(reinterpret_cast<char*>(length_buf), 2)) {
        uint16_t msg_len = read_u16_be(length_buf);

        std::vector<uint8_t> msg_buf(msg_len);
        file.read(reinterpret_cast<char*>(msg_buf.data()), msg_len);
        if (static_cast<uint16_t>(file.gcount()) != msg_len) {
            std::cerr << "truncated message, stopping\n";
            break;
        }

        char msg_type = static_cast<char>(msg_buf[0]);
        counts[msg_type]++;
    }

    for (const auto& [type, n] : counts) {
        std::cout << type << ": " << n << "\n";
    }
}
```

Was hier mechanisch passiert, Zeile für Zeile:
- `std::ifstream file(path, std::ios::binary)`: öffnet die Datei im **Binärmodus**. Ohne `std::ios::binary` würde die Standardbibliothek auf manchen Plattformen Zeilenendezeichen umwandeln (`\r\n` zu `\n`) — bei einem Binärformat wie ITCH würde das echte Datenbytes verfälschen, die zufällig wie ein Zeilenende aussehen.
- `file.read(reinterpret_cast<char*>(length_buf), 2)`: `std::ifstream::read` erwartet einen `char*`-Zeiger (so ist die Standardbibliothek nunmal typisiert), unsere Puffer sind aber `uint8_t*` (das ist inhaltlich dasselbe, ein Byte, nur mit dem für uns passenderen vorzeichenlosen Typ). `reinterpret_cast` sagt dem Compiler "behandle diesen Speicher als den anderen Typ, ich weiß was ich tue" — hier unproblematisch, weil `char` und `uint8_t` beide exakt 1 Byte groß sind.
- `while (file.read(...))`: `read()` gibt den Stream selbst zurück, und ein Stream lässt sich implizit in `bool` umwandeln (true = letzte Operation erfolgreich, false = z.B. Dateiende erreicht). Das ist der übliche C++-Idiom für "lies, solange es klappt".
- `uint16_t msg_len = read_u16_be(length_buf)`: hier kommt deine Funktion aus Schritt 1 zum Einsatz — die 2 rohen Bytes werden zur tatsächlichen Länge zusammengesetzt.
- `std::vector<uint8_t> msg_buf(msg_len)`: ein Puffer in der genau richtigen Größe für diese eine Nachricht, jedes Mal neu (Nachrichten sind unterschiedlich lang).
- `file.gcount()`: sagt dir, wie viele Bytes die letzte `read()`-Operation tatsächlich gelesen hat. Der Vergleich mit `msg_len` fängt den Fall ab, dass die Datei mitten in einer Nachricht endet (kaputte/abgeschnittene Datei) — sonst würdest du mit einem halb gefüllten Puffer weiterarbeiten und Datenmüll produzieren.
- `msg_buf[0]` ist der Message-Type (das erste Byte jeder Nachricht, siehe Framing in 1.2).

Am Ende gibst du nur die Zähl-Statistik aus (wie oft kam `'A'`, `'D'`, `'E'` etc. vor). Das beweist, dass dein Loop synchron mit der Datei bleibt: würdest du irgendwo die Länge falsch interpretieren, würdest du an der falschen Stelle weiterlesen, und die nächsten "Message-Type-Bytes" wären zufällige Datenbytes statt echter Buchstaben — die Statistik würde sofort chaotisch aussehen (viele seltsame Typen statt der paar erwarteten Buchstaben).

### Schritt 3: Message-Structs definieren

Ein Struct pro relevantem Typ, mit genau den Feldern aus den Tabellen in 1.4, in den passenden C++-Typen aus deinem Projekt:

```cpp
struct AddOrderMsg {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint64_t timestamp;      // eigentlich nur 48 Bit belegt, aber uint64_t zum Speichern
    OrderId  order_ref_num;  // deckungsgleich mit deiner OrderId (uint64_t)
    Side     side;
    Volume   shares;
    std::string stock;       // getrimmtes Ticker-Symbol
    Price    price;
};

struct DeleteOrderMsg {
    uint64_t timestamp;
    OrderId  order_ref_num;
};

struct CancelOrderMsg {   // Message-Type 'X'
    uint64_t timestamp;
    OrderId  order_ref_num;
    Volume   canceled_shares;
};

struct ExecutedOrderMsg {   // Message-Type 'E'
    uint64_t timestamp;
    OrderId  order_ref_num;
    Volume   executed_shares;
};

struct ReplaceOrderMsg {   // Message-Type 'U'
    uint64_t timestamp;
    OrderId  original_order_ref_num;
    OrderId  new_order_ref_num;
    Volume   shares;
    Price    price;
};
```

`Side`, `OrderId`, `Price`, `Volume` sind exakt die Typen aus deinem bestehenden [types.h](types.h) — bewusst wiederverwendet, damit der Parser direkt kompatible Werte für `OrderBook` liefert, ohne Konvertierung an der Schnittstelle.

### Schritt 4: Einen einzigen Typ vollständig parsen (`A`)

Jetzt kommt der Teil, der Schritt 1 (Byte-Utils) und die Feldtabelle aus 1.4 zusammenbringt. Der Trick, um die richtigen Offsets zu finden: du gehst die Felder der Reihe nach durch und **summierst die Größen der davor liegenden Felder auf**.

Für `A` (Werte relativ zum Nachrichtenanfang, `msg_buf[0]` ist das Type-Byte):

| Feld | Größe | Start-Offset (Herleitung) |
|---|---|---|
| Type-Byte | 1 | 0 |
| Stock Locate | 2 | 1 (direkt nach Type-Byte) |
| Tracking Number | 2 | 1 + 2 = 3 |
| Timestamp | 6 | 3 + 2 = 5 |
| Order Reference Number | 8 | 5 + 6 = 11 |
| Buy/Sell Indicator | 1 | 11 + 8 = 19 |
| Shares | 4 | 19 + 1 = 20 |
| Stock | 8 | 20 + 4 = 24 |
| Price | 4 | 24 + 8 = 32 |

(Zur Erinnerung aus dem Hinweis am Dateianfang: diese konkreten Offsets gegen die offizielle Spec verifizieren, bevor du sie fest einbaust — das Prinzip der Herleitung per Aufsummieren bleibt aber so oder so richtig.)

```cpp
AddOrderMsg parse_add_order(const uint8_t* msg_buf) {
    // msg_buf[0] ist das Type-Byte 'A', wird vom Aufrufer schon geprüft.
    AddOrderMsg out;

    out.stock_locate    = read_u16_be(msg_buf + 1);
    out.tracking_number = read_u16_be(msg_buf + 3);
    out.timestamp        = read_u48_be(msg_buf + 5);
    out.order_ref_num    = read_u64_be(msg_buf + 11);
    out.side              = (msg_buf[19] == 'B') ? Side::Buy : Side::Sell;
    out.shares            = read_u32_be(msg_buf + 20);

    // Stock-Symbol: 8 rohe ASCII-Bytes, mit Leerzeichen aufgefuellt
    out.stock = std::string(reinterpret_cast<const char*>(msg_buf + 24), 8);
    while (!out.stock.empty() && out.stock.back() == ' ') {
        out.stock.pop_back();
    }

    out.price = read_u32_be(msg_buf + 32);
    return out;
}
```

`msg_buf + 1` ist Zeiger-Arithmetik: `msg_buf` ist ein `const uint8_t*`, `msg_buf + 1` zeigt genau ein Byte weiter im selben zusammenhängenden Puffer. Das ist derselbe Puffer, den du in Schritt 2 pro Nachricht neu befüllt hast.

Zum Preis-Feld noch ein konkretes Zahlenbeispiel, weil das leicht zu Verwirrung führt: ITCH-Preise sind Fixed-Point mit 4 Nachkommastellen, das heißt der rohe Wert im Feld ist der **tatsächliche Dollar-Preis mal 10000**. Angenommen `read_u32_be` liefert `1050000` — das bedeutet $105.0000 (`1050000 / 10000 = 105.0`). Dein eigener `Price`-Typ ([types.h](types.h)) ist als Cent-Betrag gedacht. Um von ITCH-Fixed-Point auf Cent umzurechnen, teilst du durch 100 statt durch 10000: `1050000 / 100 = 10500` Cent = $105.00. Diese Umrechnung machst du am besten explizit und mit Kommentar an der Stelle, wo du `out.price` setzt, damit später niemand (auch nicht du selbst in drei Monaten) rätseln muss, welche Einheit gerade vorliegt.

Testen mit einer handgeschriebenen Byte-Sequenz, bei der du die erwarteten Werte selbst festgelegt hast (du weißt ja, was du reingeschrieben hast) — das ist dein erster echter End-to-End-Beweis, dass Framing und Feld-Parsing zusammen funktionieren, bevor du dich auf eine echte Datei verlässt.

### Schritt 5: Die restlichen Typen parsen (`D`, `X`, `E`, `C`, `U`)

Gleiches Muster wie Schritt 4: Tabelle mit aufsummierten Offsets aufstellen, dann eine `parse_*`-Funktion, die aus dem Puffer und den Offsets die Felder rausliest. Diese Nachrichten sind alle kürzer als `A` (weniger Felder), die Offset-Tabellen sind entsprechend kleiner. Jeweils mit eigenem kleinen Test wie in Schritt 4.

### Schritt 6: Die Design-Entscheidung aus 1.5 treffen

Bevor du weitermachst: Option A oder B? Das bestimmt, was in Schritt 7 passiert.

### Schritt 7: `OrderBook` um das nötige API erweitern (nur bei Option A)

Zwei neue Methoden, beide als Ergänzung zu den bestehenden `add_order`/`cancel_order` in [order_book.h](order_book.h)/[order_book.cpp](order_book.cpp), nicht als Ersatz.

**`add_resting_order`** ist inhaltlich identisch zu `add_order()`, nur ohne den `match()`-Aufruf am Ende — dieselbe Pool-Allokation, Placement-New und Verlinkung wie in deinem bestehenden Code:

```cpp
bool OrderBook::add_resting_order(Order order) {
    if (order.side == Side::Buy) {
        auto& queue = bids[order.price];
        void* raw = order_pool_.allocate();
        Order* new_order = new (raw) Order(order);

        if (queue.tail == nullptr) {
            queue.head = new_order;
            queue.tail = new_order;
        } else {
            queue.tail->next_order = new_order;
            new_order->prev_order = queue.tail;
            queue.tail = new_order;
        }
        order_map[order.id] = {order.price, Side::Buy, new_order};
    }
    else if (order.side == Side::Sell) {
        // symmetrisch, exakt wie der Sell-Zweig in add_order()
    }

    return true;   // kein match() hier, das ist der ganze Unterschied
}
```

Da sich `add_order` und `add_resting_order` fast komplett doppeln (nur der `match()`-Aufruf fehlt), ist das ein guter Kandidat, um beide später auf eine gemeinsame private Hilfsmethode (z.B. `insert_into_queue(Order)`) zurückzuführen, die nur das Einfügen macht, während `add_order` danach zusätzlich `match()` aufruft. Für den ersten Durchlauf reicht aber auch die einfache Kopie mit weggelassenem `match()`.

**`reduce_volume`** ist neu, es gibt aktuell nichts Vergleichbares in deinem Code:

```cpp
bool OrderBook::reduce_volume(OrderId id, Volume qty) {
    auto it = order_map.find(id);
    if (it == order_map.end()) return false;

    Order* order = it->second.order;

    if (qty >= order->volume) {
        // die Order ist danach komplett leer, das ist ein normales Cancel
        return cancel_order(id);
    }

    order->volume -= qty;
    return true;
}
```

Der wichtige Fall hier: wenn `qty` mindestens so groß ist wie das verbleibende Volumen, delegierst du an dein bestehendes `cancel_order()` — das kennt schon die komplette Logik zum Aushängen aus der Kette (Kopf/Ende/Mitte/einziges Element), die willst du nicht duplizieren. Ist nach dem Abzug noch Volumen übrig, genügt eine einfache Feldänderung am bestehenden `Order`-Objekt: die Verkettung (`next_order`/`prev_order`) bleibt komplett unangetastet, weil die Order an ihrem Platz in der Queue bleibt, nur mit weniger Volumen.

### Schritt 8: Stock-Directory-Filter (`R`-Nachrichten)

Eine `std::unordered_map<uint16_t, std::string>` für Stock-Locate-zu-Ticker, befüllt beim Einlesen der `R`-Nachrichten. Ein einfacher Filter (z.B. ein `std::optional<uint16_t> target_stock_locate`), der alle Nachrichten verwirft, deren Stock Locate nicht zum gewünschten Test-Symbol gehört.

### Schritt 9: Die Verdrahtung — Parser-Output auf `OrderBook`-Aufrufe abbilden

Eine dünne Schicht, die pro geparster Nachricht den passenden `OrderBook`-Aufruf macht:
- `A`/`F` → `ob.add_resting_order(...)`
- `D` → `ob.cancel_order(order_ref)`
- `X`, `E`, `C` → `ob.reduce_volume(order_ref, shares)`
- `U` → `ob.cancel_order(alte_ref)` + `ob.add_resting_order(...)` mit der neuen Referenz

Genau wie `main.cpp` aktuell `generateX()`-Output auf `add_order()` abbildet ([main.cpp:9-11](main.cpp:9)) — nur dass die Quelle jetzt eine ITCH-Datei statt der Zufallsgenerator ist. Halte diese Schicht bewusst dünn und getrennt vom eigentlichen Parser (der Parser soll `OrderBook` gar nicht kennen müssen), gleiches Trennungsprinzip wie bei `feed_simulator`.

### Schritt 10: Integrationstest mit echten (oder handgebauten) Daten

Entweder eine echte NASDAQ-Sample-ITCH-Datei (ein Symbol rausfiltern, die ersten paar Tausend Nachrichten durchlaufen lassen, `display()` zwischendurch aufrufen) oder, falls du keine Sample-Datei zur Hand hast, eine kleine selbstgebaute Byte-Sequenz mit 5-10 Nachrichten verschiedener Typen, bei der du das erwartete Endergebnis von Hand durchgerechnet hast. Das ist dein Beweis, dass die komplette Kette (Framing → Parsing → `OrderBook`-Aufrufe → Buchzustand) tatsächlich stimmt.

---

## Kurz zusammengefasst

Reihenfolge: Byte-Utils testen → nur Framing (zählen) → Message-Structs → einen Typ komplett parsen und testen (`A`) → restliche Typen genauso → Design-Entscheidung treffen (Rekonstruktion vs. Simulator-Ersatz) → bei Rekonstruktion `OrderBook` um `add_resting_order`/`reduce_volume` erweitern → Symbol-Filter über die `R`-Nachrichten → dünne Verdrahtungsschicht Parser-zu-OrderBook → Integrationstest mit echten Daten.
