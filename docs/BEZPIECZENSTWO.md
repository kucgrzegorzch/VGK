# Mechanizm Bezpieczeństwa i Zaufania VGK

## Cel

VGK ma rozpoznawać nieautoryzowaną zmianę plików i nie uruchamiać elementów systemu, których pochodzenia oraz integralności nie da się potwierdzić. Ochrona ma obejmować cały łańcuch uruchamiania, jądro, sterowniki, składniki systemowe i aktualizacje.

Dokument opisuje projekt oraz pierwszy pomocniczy prototyp sprawdzania wydania. Narzędzie komputerowe opisane niżej jest zaimplementowane jako kontroler uruchamiany ręcznie; nie jest jeszcze częścią programu startowego VGK ani działającego jądra. Nie zapewnia więc jeszcze ochrony podczas uruchamiania systemu.

## Zasady nadrzędne

1. Samo położenie pliku na dysku VGK nie czyni go zaufanym.
2. Każdy chroniony składnik musi być uwzględniony w podpisanym wykazie wydania.
3. Weryfikacja musi odbywać się przed użyciem składnika, a nie dopiero po jego uruchomieniu.
4. Błąd weryfikacji oznacza brak zgody na uruchomienie danego składnika.
5. Naprawa może korzystać wyłącznie z wersji, których podpis i integralność zostały poprawnie sprawdzone.
6. Klucz służący do podpisywania oficjalnych wydań pozostaje poza repozytorium i pod kontrolą właściciela projektu.
7. Każda decyzja o naprawie, zablokowaniu pliku lub zmianie zaufania powinna być odnotowana w dzienniku zdarzeń.

## Łańcuch sprawdzania zaufania

Docelowa kolejność:

1. **Oprogramowanie układowe komputera** uruchamia wybrany mechanizm startowy. Jeżeli sprzęt obsługuje sprawdzanie podpisów uruchomieniowych, należy je wykorzystać, ale nie zakładać, że jest dostępne na każdym komputerze.
2. **Mechanizm uruchamiania VGK** sprawdza podpis własnego wykazu zaufania i składników potrzebnych do dalszego startu.
3. **Ładowanie jądra** następuje dopiero po sprawdzeniu podpisu wydania i skrótu kryptograficznego pliku jądra.
4. **Przekazanie sterowania jądru** odbywa się wraz z informacją o wyniku sprawdzeń, wersji wykazu i wykrytych problemach.
5. **Jądro VGK** sprawdza sterowniki i składniki systemowe przed dopuszczeniem ich do działania.
6. **System Naprawczy** może odizolować podejrzany składnik i zaproponować przywrócenie poprawnej kopii.

Każdy etap musi ufać wyłącznie kluczowi publicznemu lub wynikowi sprawdzenia przekazanemu przez wcześniejszy, zaufany etap. Sam komunikat tekstowy ani nazwa pliku nie są dowodem autentyczności.

## Pierwsze narzędzie sprawdzające wydanie

W repozytorium znajduje się `tools/sprawdz_wydanie.py`. Narzędzie sprawdza podpis Ed25519 wykazu JSON przy użyciu OpenSSL, a następnie porównuje rozmiar i skrót SHA-256 każdego wskazanego pliku. Odrzuca między innymi nieprawidłowy podpis, zmieniony wykaz, ścieżki wychodzące poza katalog wydania i dowiązania symboliczne.

Testy uruchamia polecenie `make security-test`. Tworzą one tymczasowy klucz testowy, sprawdzają poprawne wydanie, zmianę pliku o niezmienionym rozmiarze, zmianę wykazu, błędny typ ścieżki i próbę wyjścia ścieżką poza katalog. Klucz testowy jest wyłącznie do testów i nie może być używany do podpisywania oficjalnych wydań.

To narzędzie jest kontrolą po stronie komputera budującego lub sprawdzającego wydanie. Nie jest jeszcze zintegrowane z UEFI Stage 0 i nie blokuje startu zmodyfikowanego jądra.

## Podpisy i wykaz plików

Oficjalne wydanie będzie zawierało wykaz obejmujący co najmniej:
- nazwę i wersję wydania;
- wersję formatu wykazu;
- ścieżkę każdego chronionego pliku;
- jego rozmiar;
- skrót SHA-256;
- przeznaczenie i wymagany poziom zaufania.

Cały wykaz będzie podpisany kluczem prywatnym przechowywanym poza repozytorium. VGK będzie zawierał odpowiadający mu klucz publiczny i będzie sprawdzał podpis przed uznaniem wykazu za wiarygodny.

Skrót SHA-256 pozwala wykryć zmianę zawartości, ale sam w sobie nie potwierdza autora. Dopiero podpis wykazu wiąże sprawdzane skróty z zaufanym wydawcą.

Klucz prywatny nie może być umieszczany w kodzie źródłowym, obrazie ISO, artefaktach budowania ani w publicznym repozytorium. Utrata lub ujawnienie klucza wymaga procedury unieważnienia i bezpiecznej zmiany klucza zaufania.

## Reakcja na wykrytą zmianę

Jeżeli plik nie zgadza się z podpisanym wykazem, VGK powinien:

- nie uruchamiać go ani nie ładować jako sterownika;
- wskazać nazwę pliku i rodzaj niezgodności;
- zapisać zdarzenie w dzienniku;
- jeśli to możliwe, uruchomić ograniczone środowisko naprawcze;
- przywrócić plik wyłącznie z poprawnie podpisanego źródła;
- ponownie sprawdzić plik po naprawie.

Jeżeli nie można potwierdzić autentyczności źródła naprawy, system ma przerwać naprawę zamiast instalować niepewny plik.

## Ochrona podczas pracy

Samo sprawdzenie przy uruchamianiu nie wystarcza. Docelowo VGK powinien również:
- ograniczać aplikacjom dostęp do pamięci jądra i plików krytycznych;
- rozdzielać uprawnienia zwykłych programów, administratora i składników systemowych;
- chronić kluczowe obszary pamięci przed zapisem po ich przygotowaniu;
- kontrolować operacje zmieniające pliki systemowe;
- okresowo lub po podejrzanym zdarzeniu ponownie sprawdzać wybrane pliki;
- nie pozwalać zwykłej aplikacji na zmianę kluczy zaufania.

Ochrona plików na dysku nie wykrywa automatycznie każdej zmiany kodu już znajdującego się w pamięci. Potrzebne są osobne zabezpieczenia pamięci, uprawnień i sterowników.

## Cofanie do starszych wersji

Podpisany plik może być autentyczny, ale przestarzały i podatny na znaną lukę. Dlatego wykaz wydań powinien zawierać numer wersji zabezpieczeń, a VGK powinien w przyszłości zapobiegać nieautoryzowanemu cofnięciu do starszej wersji. Można wykorzystać do tego sprzętowy licznik lub chroniony magazyn stanu, jeśli komputer go udostępnia. Nie wolno zakładać, że każdy komputer ma taki sprzęt.

## Ograniczenia i uczciwe kryteria

Nie da się obiecać absolutnej niemożliwości podmiany pliku na dowolnym komputerze. Atak może objąć oprogramowanie układowe, klucze zaufania, sprzęt lub działający już system. Celem VGK jest utrudnić takie działania, wykrywać niezgodności i nie ufać zmienionym składnikom.

Każdy etap będzie oznaczany osobno jako:
- **zaprojektowany** – opisany, ale bez kodu;
- **zaimplementowany** – kod istnieje;
- **sprawdzony statycznie** – przeszedł testy struktury lub kompilacji;
- **sprawdzony w emulatorze** – działa w określonym środowisku testowym;
- **sprawdzony na sprzęcie** – działa na wskazanym komputerze.

Nie wolno przedstawiać samego udanego budowania programu jako dowodu skutecznej ochrony.

## Kolejność realizacji

1. Dokończyć i zweryfikować narzędzie sprawdzające podpisane wydanie.
2. Ustalić bezpieczny sposób wbudowania klucza publicznego wydawcy w mechanizm uruchamiania.
3. Dodać sprawdzanie podpisu i skrótów do własnego mechanizmu uruchamiania UEFI.
4. Odmówić uruchomienia jądra przy błędzie weryfikacji.
5. Przekazać jądru wynik sprawdzenia w wersjonowanej strukturze informacji startowych.
6. Rozszerzyć kontrolę na sterowniki i pliki systemowe.
7. Dodać dziennik zdarzeń oraz procedurę bezpiecznego odzyskiwania.
8. Przetestować przypadki: plik poprawny, plik zmieniony, brak wykazu, błędny podpis, nieznany klucz, starsze wydanie i uszkodzony nośnik.

Na obecnym etapie VGK ma własny punkt wejścia UEFI Stage 0 oraz pomocnicze narzędzie do sprawdzania podpisanego wydania. Sam mechanizm startowy nie sprawdza jeszcze podpisu ani nie ładuje jądra.

