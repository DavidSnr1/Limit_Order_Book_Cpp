# Limit Order Book ? Nächste Schritte (Stand 01.07.2026)

## Ausgangslage

Der Kern steht sauber: Einfügen, Matching mit Preis-Zeit-Priorität, Cancel, 12 grüne Tests. Was fehlt: das Projekt ist noch nicht lauffähig und noch nicht auf Produktionsniveau. Der Plan hat zwei Teile ? erst **MVP abschließen** (lauffähig + sauber), dann **Receive Side Masterclass** (die 70h Ausbau).

---

## TEIL 1 ? MVP wirklich abschließen

Zuerst lauffähig und sauber machen, bevor irgendein neues Feature dazukommt. Ein Projekt das nicht startet ist wertlos, egal wie gut die Engine ist.

### Schritt 1 ? Lauffähig machen (~5h)

**main.cpp** ? verbindet Simulator und Order Book:
```
Order Book erstellen
Simulator generiert Orders
? jede Order durch add_order
? nach jedem Add: display()
```

**Makefile** ? damit `make` alles baut:
```
Build-Target für die App
Build-Target für die Tests
clean
```

Ergebnis: jemand kann das Repo klonen, `make` tippen und es laufen sehen. Grundvoraussetzung für Vorzeigbarkeit.

### Schritt 2 ? Trade-Ausgabe (~3h)

Die wichtigste fachliche Lücke. Ein Order Book das Trades still verrechnet ist wie ein Taschenrechner der das Ergebnis nicht anzeigt.

```
In match(): bei jeder Ausführung ein Trade-Objekt erzeugen
struct Trade { Price price; Volume qty; OrderId buy_id, sell_id; }
? in einen Trade-Log-Vektor pushen
? optional live ausgeben: "TRADE: 100 @ $67.20"
```

Wird später auch für die P&L-Rechnung der Send Side gebraucht ? jetzt richtig bauen.

### Schritt 3 ? Aufräumen (~2h)

```
Header-Warnungen bereinigen
timestamp entweder nutzen oder als "kommt in Phase 2" kommentieren
README ehrlich machen (Makefile existiert jetzt wirklich)
```

Danach ist der MVP **fertig und sauber** ? committen und als abgeschlossen betrachten.

---

## TEIL 2 ? Receive Side Masterclass (die 70h)

Jetzt wird aus dem soliden MVP ein beeindruckendes System. In dieser Reihenfolge.

### Schritt 4 ? Memory Pool (~15h)
Statt `std::list` (alloziert pro Node einzeln) ein vorallokierter Pool mit Freistapel. Erster echter Performance-Sprung, erster Benchmark-Wert. Orders kommen aus dem Pool in ~10ns statt ~1000ns.

### Schritt 5 ? Intrusive Linked List (~parallel zu 4)
`next_idx`/`prev_idx` direkt im Order-Struct, verzahnt mit dem Pool. Cache-Lokalität ? die Orders liegen zusammenhängend im Speicher.

### Schritt 6 ? ITCH Parser (~25h)
Das Wow-Feature. Echte NASDAQ-ITCH-Binärdaten statt Simulator. Hier wird aus einem Uni-Projekt ein "läuft auf echten Börsendaten"-Projekt. Big-Endian Byte-Swapping, Message-Typen A/D/E/X/U.

### Schritt 7 ? Benchmarking (~10h)
Naive `std::map`/`std::list` Version vs. Pool-Version. Latenz p50/p99 mit `std::chrono`. Das ist deine README-Tabelle und der Speedup-Faktor.

### Schritt 8 ? Lock-Free SPSC Queue (~15h)
Parser-Thread + Matching-Thread, dazwischen die Lock-Free Queue mit `alignas(64)` gegen False Sharing. Krönung der Receive Side ? production-grade.

---

## Reihenfolge auf einen Blick

```
1. main.cpp + Makefile        ? JETZT, lauffähig machen
2. Trade-Ausgabe/-Log         ? fachliche Kernlücke
3. Aufräumen + README         ? MVP abgeschlossen
??????????????????????????????? (MVP fertig)
4. Memory Pool                ? erster Performance-Sprung
5. Intrusive List             ? verzahnt mit Pool
6. ITCH Parser                ? Wow-Feature, echte Daten
7. Benchmarking               ? README-Tabelle
8. Lock-Free SPSC Queue       ? Krönung
??????????????????????????????? (Receive Side komplett)
```

---

## Wichtigster nächster Schritt

**main.cpp + Makefile.** Nicht Memory Pool, nicht ITCH ? erst lauffähig machen. Selbst schreiben (MVP-Prinzip), bei Bedarf konzeptionell nachfragen statt Code kopieren.

---

## Offene Punkte aus dem Projektbericht (zugeordnet)

| Offener Punkt | Wird gelöst in |
|---|---|
| Kein Build-System / main.cpp | Schritt 1 |
| Keine Trade-Ausgabe | Schritt 2 |
| Header-Warnungen | Schritt 3 |
| timestamp ungenutzt | Schritt 3 (dokumentieren) / später Pool |
| Market Orders / Execute-Pfad | nach MVP, optional vor Schritt 4 |
| display() ohne L2-Aggregation | optional, im Zuge Benchmarking/Politur |