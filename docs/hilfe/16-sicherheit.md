# Prüfsummen, Verschlüsselung und Rechte

## Prüfsummen

Rechtsklick auf eine Datei ▸ **Prüfsumme berechnen …** öffnet einen kleinen Dialog mit der Prüfsumme. Unterstützt sind MD5, SHA-1, SHA-256, SHA-512 und BLAKE2. Damit prüfen Sie zum Beispiel, ob ein Download unverändert angekommen ist: Fügen Sie den Wert der Quelle ein, Nolphin zeigt, ob er übereinstimmt.

## Verschlüsseln

Rechtsklick ▸ **Verschlüsseln …** verschlüsselt eine Datei mit **GPG** und fragt die dafür nötigen Angaben ab. Dafür muss `gpg` installiert sein.

## Eigenschaften und Zugriffsrechte

**Alt+Eingabe** (oder Rechtsklick ▸ Eigenschaften) öffnet die Eigenschaften im Arbeitsbereich.

![Eigenschaften-Panel mit Zugriffsrechten und ACL-Bereich](bilder/panel-eigenschaften.png)

| Bereich | Inhalt |
| ------- | ------ |
| Allgemein | Typ, Ort, Änderungs- und Zugriffsdatum |
| Zugriffsrechte | Lesen, Schreiben, Ausführen für Eigentümer, Gruppe und Andere |
| Besitzer / Gruppe | wer die Datei besitzt |
| Lesen/Schreiben auf Inhalt anwenden | überträgt die Rechte eines Ordners auf alles darin |
| Weitere Benutzer und Gruppen | **ACL**: Rechte für einzelne Personen oder Gruppen darüber hinaus |

> **Voraussetzung für ACL:** `getfacl` und `setfacl`. Mit dem Namensfeld und **Hinzufügen** erweitern Sie die Rechte um eine Person oder Gruppe.

## Symbol wählen

Ein Klick auf das große Symbol im Eigenschaften-Panel öffnet eine Auswahl, in der Sie ein Symbol für Ordner oder Dateien aussuchen – durchsuchbar und nach Kategorien geordnet. Mit **Bild wählen …** nutzen Sie ein eigenes Bild.

![Die Symbolauswahl mit Kategorien und Suchfeld](bilder/panel-symbol-waehlen.png)

## Papierkorb

Gelöschte Dateien liegen zunächst im Papierkorb. Wie lange sie dort bleiben und ab welcher Größe Nolphin warnt, stellen Sie in den Einstellungen ein.
