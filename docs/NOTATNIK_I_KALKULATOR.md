# Notatnik z wbudowanym kalkulatorem — wymagania VGK

## Cel

VGK ma mieć jedną aplikację „Notatnik”, w której można pisać zwykły tekst, wstawiać poprawnie sformatowane wzory matematyczne i wykonywać obliczenia. Kalkulator jest częścią notatnika, a nie osobną aplikacją wymagającą przełączania okien.

## Sposób obsługi

- W notatniku znajduje się wyraźny przycisk „Kalkulator”.
- Naciśnięcie przycisku otwiera panel kalkulatora obok notatki lub w dolnej części okna. Nie usuwa ani nie zastępuje aktualnej treści.
- Wynik obliczenia można wstawić w miejscu kursora jako tekst albo jako element matematyczny.
- Użytkownik może wrócić do pisania bez utraty treści i bez zamykania notatki.
- Podstawowe działania są dostępne z klawiatury; przyciski ekranowe są pomocą, a nie jedynym sposobem wprowadzania.

## Edytor matematyczny

Notatnik powinien mieć pasek znaków i wzorów, obejmujący co najmniej:
- dodawanie, odejmowanie, mnożenie, dzielenie i znak równości;
- nawiasy, procent, pierwiastek i potęgi;
- indeks dolny i górny;
- ułamki piętrowe;
- symbole takie jak π, √, ≤, ≥, ≠, ±, ∞ oraz greckie litery.

Ułamek jest pojedynczym, edytowalnym obiektem matematycznym: licznik znajduje się nad poziomą kreską, a mianownik pod nią. Wstawianie ułamka powinno od razu umieszczać kursor w liczniku; przejście do mianownika i wyjście z ułamka musi być możliwe klawiaturą oraz myszą/dotykiem.

## Kopiowanie i wklejanie

- Kopiowanie ułamka, pierwiastka, potęgi lub całego wzoru kopiuje strukturę matematyczną, a nie osobne znaki rozrzucone po wierszach.
- Wklejenie do VGK zachowuje układ, kreski ułamkowe, indeksy, odstępy i kolejność elementów.
- Wklejanie między aplikacjami powinno używać wspólnego formatu matematycznego, gdy aplikacja docelowa go obsługuje; dodatkowo należy zapewnić czytelną reprezentację tekstową jako rozwiązanie awaryjne.
- Jeżeli źródło schowka zawiera wzór bez struktury, notatnik nie powinien udawać, że odtworzył układ. Powinien wkleić czytelny tekst i umożliwić poprawienie go w edytorze matematycznym.

## Format dokumentu i schowka

Wewnętrzny zapis notatnika powinien przechowywać wzory jako strukturę, a nie jako znaki Unicode rozmieszczone w wielu liniach. Dzięki temu wzory pozostają edytowalne, skalowalne i odporne na zmianę czcionki lub rozmiaru okna.

Zalecany model to drzewo elementów matematycznych (tekst, ułamek z licznikiem i mianownikiem, pierwiastek, potęga, indeks, operator i nawias). Dla wymiany danych można obsłużyć MathML lub równoważny format strukturalny. Zwykły tekst pozostaje dostępny jako dodatkowa reprezentacja.

## Obliczenia i bezpieczeństwo

- Kalkulator ma rozróżniać zapis wyrażenia od wyniku.
- Powinien obsługiwać kolejność działań, nawiasy, ułamki, liczby dziesiętne i podstawowe funkcje matematyczne.
- Wyniki powinny być wstawiane dopiero na żądanie użytkownika; kalkulator nie może samowolnie zmieniać treści notatki.
- Obliczenia należy wykonywać lokalnie, bez konieczności połączenia z internetem.
- Nie wolno wykonywać wprowadzonych wyrażeń jako kodu systemowego ani poleceń powłoki.

## Dostępność i wygoda

- Czytelny wygląd szkolnych wzorów, również przy powiększeniu.
- Obsługa klawiatury, myszy i ekranu dotykowego.
- Cofnij/ponów obejmuje całe operacje matematyczne, np. wstawienie lub edycję ułamka.
- Drukowanie i eksport do PDF zachowują układ wzorów.
- Dokument można otworzyć ponownie i dalej edytować bez utraty struktury.

## Etapy realizacji

1. Ustalić wewnętrzny model dokumentu i obiektów matematycznych.
2. Zbudować edytor wzorów z ułamkami piętrowymi, potęgami, indeksami i pierwiastkami.
3. Dodać bezpieczny kalkulator i przycisk otwierający jego panel w notatniku.
4. Dodać kopiowanie/wklejanie strukturalne i tekstowy wariant awaryjny.
5. Dodać drukowanie, eksport i testy zachowania wzorów.

## Warunek odbioru

Użytkownik może wpisać ułamek, skopiować go, wkleić w innym miejscu notatki, zamknąć dokument i otworzyć go ponownie — za każdym razem ułamek nadal wygląda jak ułamek piętrowy i pozostaje edytowalny.