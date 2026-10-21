# Guide: Den sicheren Terminal-Messenger selbst entwickeln und verstehen

Idee: Du baust das Projekt **in 8 Stufen von null nach**. Jede Stufe lässt sich einzeln ausprobieren.
Der fertige Code in `src/` ist deine Musterlösung – schau erst nach, wenn du es selbst versucht hast.

Werkzeuge: `g++`, `cmake`, `libsodium-dev`, `tcpdump` oder Wireshark, `gdb`, `strace`.
Compile-Flags beim Lernen: `-g -fsanitize=address,undefined` (findet Speicherfehler sofort).

---

## Stufe 1 – TCP-Sockets (Blocking)
**Konzept:** TCP ist ein *Byte-Strom*, keine Nachrichtenfolge. `socket → bind → listen → accept` (Server),
`socket → connect` (Client), dann `send`/`recv`.

**Baue:** Einen Echo-Server (ein Client) und einen Client, der Zeilen von stdin sendet.
**Teste:** `nc 127.0.0.1 7777` als Gegenstelle.
**Frage:** Was passiert, wenn du 10 Mal `send` aufrufst – kommt auf der anderen Seite 10 Mal `recv` mit je einer Nachricht an? (Nein! → Stufe 2)
**Lesen:** Beej's Guide to Network Programming.
**Im Projekt:** `tcp_listen`, `tcp_connect`, `write_all` in `src/net.cpp`.

## Stufe 2 – Framing
**Konzept:** Da TCP keine Grenzen kennt, brauchst du ein Protokoll: `[Länge 4 Byte][Typ][Daten]`.
Der Empfänger puffert, bis ein ganzer Frame da ist (ein `recv` kann Teile oder mehrere Frames liefern).

**Baue:** `FrameReader` mit `feed()` und `next()`. Schreibe einen Test, der einen Frame byteweise einspeist.
**Wichtig (Sicherheit):** Länge immer gegen ein Maximum prüfen, sonst kann ein Angreifer dich 4 GB allokieren lassen.
**Im Projekt:** `FrameReader` in `src/net.cpp`, `MAX_FRAME` in `src/net.hpp`.

## Stufe 3 – Relay-Server mit `poll()`
**Konzept:** Viele Clients gleichzeitig ohne Threads: nicht-blockierende Sockets + `poll()`.
Pro Client: Eingabepuffer (FrameReader) und Ausgabepuffer (für Daten, die `send` nicht sofort loswird → `EAGAIN`, dann `POLLOUT`).

**Baue:** Server mit `map<fd, Client>`, Registrierung eines Namens, Weiterleitung `RELAY → DELIVER`.
**Designentscheidung:** Der **Server** setzt den Absender, nie der Client – sonst kann jeder fremde Namen vortäuschen.
**Teste:** Zwei `nc`-Sessions mit handgebauten Frames (z.B. Python-Skript).
**Im Projekt:** `src/server.cpp` (`handle_frame`, `flush`, Timeout für unregistrierte Clients).

## Stufe 4 – Krypto-Grundlagen (erst verstehen, dann benutzen)
Baue **nie eigene Kryptografie**; benutze libsodium. Verstehe aber die Bausteine:

| Baustein | Aufgabe | libsodium |
|---|---|---|
| AEAD | verschlüsselt **und** authentifiziert (Manipulation fällt auf) | `crypto_aead_xchacha20poly1305_ietf_*` |
| Nonce | darf pro Schlüssel **nie** wiederverwendet werden | hier: Zähler |
| Schlüsselaustausch | zwei Parteien einigen sich über offene Leitung auf ein Geheimnis (Diffie-Hellman) | `crypto_kx_*` (X25519) |
| Signatur | beweist, wer eine Nachricht erzeugt hat | `crypto_sign_*` (Ed25519) |
| Hash | Fingerabdruck | `crypto_generichash` (BLAKE2b) |
| Passwort-KDF | aus Passphrase einen Schlüssel machen, bewusst langsam | `crypto_pwhash` (Argon2id) |

**Übungen (Mini-Programme, je 20 Zeilen):**
1. Nachricht verschlüsseln und entschlüsseln. Dann ein Bit im Chiffretext flippen → Entschlüsselung muss fehlschlagen.
2. Zwei Schlüsselpaare erzeugen, beide Seiten berechnen denselben gemeinsamen Schlüssel.
3. Signieren und verifizieren; Nachricht ändern → Verifikation schlägt fehl.

**Lesen:** libsodium-Doku (doc.libsodium.org), „Serious Cryptography" (Aumasson), Cryptopals (um Angriffe zu verstehen).

## Stufe 5 – Das Handshake-Protokoll entwerfen
Beginne mit dem **Bedrohungsmodell**: Der Server ist nicht vertrauenswürdig (liest mit, ändert, verwirft, spielt ein).

Ablauf (siehe Kommentar oben in `src/client.cpp`):
```
A -> B : HS1 = eph_A | id_A | Sign(id_A, "hs1", A, B, eph_A)
B -> A : HS2 = eph_B | id_B | Sign(id_B, "hs2", B, A, eph_A, eph_B)
beide  : Sitzungsschlüssel aus (eph_A, eph_B)
```
Warum so?
- **Ephemerale Schlüssel** (`eph_*`) → Forward Secrecy; nach dem Handshake wird `eph_sk` gelöscht.
- **Signatur mit Langzeitschlüssel** → Authentizität; ohne sie könnte der Server beide Seiten mit eigenen Schlüsseln „man-in-the-middle"-en.
- **Namen in der Signatur** → der Server kann ein HS1 nicht an eine andere Person umleiten.
- **`eph_A` in HS2 signiert** → Antwort gehört zu *diesem* Handshake (Frische).

**Übung:** Schreibe auf Papier auf, was ein Angreifer im Server tun kann, wenn du (a) die Signatur weglässt, (b) die Namen weglässt. Baue dann den Angriff gedanklich nach.
**Im Projekt:** `hs1_message`/`hs2_message` in `src/crypto.cpp`, `on_hs1`/`on_hs2` in `src/client.cpp`.

## Stufe 6 – Identität, Vertrauen, Datenübertragung
- **Identität:** Ed25519-Schlüsselpaar, Secret Key mit Passphrase verschlüsselt (Argon2id → `secretbox`). Datei mit Rechten `0600`.
- **Vertrauen (TOFU):** Beim ersten Kontakt Schlüssel speichern; später muss er gleich sein, sonst Warnung. Den ersten Kontakt sichert nur der **Fingerprint-Vergleich über einen anderen Kanal**.
- **Daten:** `ctr(8) | AEAD(padded)`; Nonce aus Zähler, Empfänger akzeptiert nur steigende Zähler (Replay-Schutz); Padding verbirgt die exakte Länge.

**Im Projekt:** `identity_*`, `KnownPeers`, `Session::encrypt/decrypt` in `src/crypto.cpp`.

## Stufe 7 – Client als Zustandsautomat
Pro Gesprächspartner: `IDLE → INIT_SENT → ESTABLISHED`.
Knifflige Fälle (durchdenken, dann im Code suchen):
1. Beide schicken gleichzeitig HS1 → Tie-Break: kleinerer Name gewinnt.
2. Nachricht tippen, bevor der Handshake fertig ist → Warteschlange `pending`.
3. Partner startet neu und hat keine Sitzung mehr → neu verhandeln.
4. Ungültige Signatur / falscher Schlüssel → verwerfen, nie Zustand ändern.

Der Event-Loop: `poll()` auf stdin und Socket gleichzeitig.
**Im Projekt:** `App`, `Peer`, `on_*`-Funktionen, `main()` in `src/client.cpp`.

## Stufe 8 – Härten und Testen
- **Terminal-Injection:** Ein Peer kann ESC-Sequenzen oder Zeilenumbrüche senden und dein Terminal manipulieren → `sanitize()`.
- **Limits:** Frame-Größe, Nachrichtenlänge, Ausgabepuffer, Registrierungs-Timeout.
- **Geheimnisse löschen:** `sodium_memzero` statt `memset` (wird nicht wegoptimiert).
- **Test mit Augen:** `sudo tcpdump -A -i lo port 7777` – du siehst Frames, aber keinen Klartext.
- **Integrationstest:** `tests/e2e.sh` (Server + zwei Clients + Schlüsselwechsel).
- **Fuzzing (Bonus):** `FrameReader` und `on_deliver` mit zufälligen Bytes füttern (libFuzzer: `-fsanitize=fuzzer,address`).

---

## Empfohlene Reihenfolge zum Nachbauen
1. Stufe 1–2 in einer Datei, dann in `net.cpp` auslagern.
2. Stufe 3: Server komplett ohne Krypto; Clients senden Klartext. **Das muss funktionieren, bevor du Krypto anfasst.**
3. Stufe 4: Krypto-Übungen separat.
4. Stufe 5–7: Krypto in den Client einbauen: erst feste Schlüssel (Test), dann Handshake.
5. Stufe 8 zum Schluss.

## Selbsttest – kannst du das beantworten?
1. Warum reicht `recv()` allein nicht, um Nachrichten zu trennen?
2. Warum darf der Server den Absender nicht dem Client überlassen?
3. Was passiert bei Nonce-Wiederverwendung im AEAD?
4. Was schützt Forward Secrecy, und was nicht?
5. Warum nützt TOFU nichts gegen einen Angreifer beim *allerersten* Kontakt?
6. Was sieht der Server trotz E2E noch? (Metadaten)

## Weiterführend
- **TLS zwischen Client und Server** (OpenSSL): versteckt Metadaten vor Netzwerkbeobachtern.
- **Noise Protocol Framework** (noiseprotocol.org): Vorbild für professionelle Handshakes.
- **Double Ratchet** (Signal-Spezifikation): Forward Secrecy *pro Nachricht*.
- **Offline-Nachrichten:** Server speichert Chiffretext; dafür sind Prekeys (X3DH) nötig.
- **Gruppenchats**, **Dateiübertragung**, **ncurses-UI**.
