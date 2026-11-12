# Mechanizm Bezpieczeństwa i Zaufania VGK

## Cel

VGK ma rozpoznawać nieautoryzowaną zmianę plików i nie uruchamiać elementów systemu, których pochodzenia oraz integralności nie da się potwierdzić. Ochrona ma obejmować cały łańcuch uruchamiania, jądro, sterowniki, składniki systemowe i aktualizacje.

Dokument opisuje zasady docelowe oraz aktualny, eksperymentalny łańcuch sprawdzania zaufania w ścieżce UEFI. Ładowacz UEFI odczytuje podpisany wykaz VGKM, sprawdza podpis Ed25519 i skróty SHA-256 plików, a następnie ładuje jądro ELF64. Ten przepływ uruchomił jądro diagnostyczne w emulatorze QEMU/OVMF. Test używa tymczasowego klucza CI; produkcyjny klucz wydawcy nie został jeszcze ustalony. Nie jest to jeszcze pełna ochrona całego systemu ani potwierdzenie działania na fizycznym komputerze.

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
6. **Inteligentny Opiekun Systemu** może wykryć podejrzany składnik, odizolować go w zakresie swoich uprawnień i zgłosić potrzebę naprawy. Przywracanie systemu odbywa się wyłącznie w trybie awaryjnym.

Każdy etap musi ufać wyłącznie kluczowi publicznemu lub wynikowi sprawdzenia przekazanemu przez wcześniejszy, zaufany etap. Sam komunikat tekstowy ani nazwa pliku nie są dowodem autentyczności.

## Własne obliczanie skrótu SHA-256

Dodano przenośną implementację SHA-256 w języku C:
- `boot/crypto/sha256.h` – interfejs;
- `boot/crypto/sha256.c` – obliczanie skrótu;
- `tests/test_sha256.c` – testy wektorów kontrolnych i podawania danych w częściach.

Test jest uruchamiany jako część `make security-test`. Sprawdza między innymi znane wyniki dla pustego tekstu i tekstu `abc`. Kod został skompilowany lokalnie z ostrzeżeniami traktowanymi jako błędy, a testy zakończyły się powodzeniem.

Moduł SHA-256 jest wykorzystywany przez parser wykazu i ładowacz UEFI do kontroli zawartości plików. Sam skrót nie potwierdza, kto utworzył plik; zaufanie do oczekiwanego skrótu wynika z podpisu wykazu.

## Pierwsze narzędzie sprawdzające wydanie

W repozytorium znajduje się `tools/sprawdz_wydanie.py`. Narzędzie sprawdza podpis Ed25519 binarnego wykazu VGKM przy użyciu OpenSSL, a następnie porównuje rozmiar i skrót SHA-256 każdego wskazanego pliku. Odrzuca między innymi nieprawidłowy podpis, zmieniony wykaz, ścieżki wychodzące poza katalog wydania i dowiązania symboliczne.

Testy uruchamia polecenie `make security-test`. Tworzą one tymczasowy klucz testowy, sprawdzają poprawne wydanie, zmianę pliku o niezmienionym rozmiarze, zmianę wykazu, uszkodzony podpis, błędny typ ścieżki, próbę wyjścia poza katalog i dowiązanie symboliczne. Klucz testowy jest wyłącznie do testów i nie może być używany do podpisywania oficjalnych wydań.

To narzędzie jest kontrolą po stronie komputera budującego lub sprawdzającego wydanie. Nie jest jeszcze zintegrowane z UEFI Stage 0 i nie blokuje startu zmodyfikowanego jądra.

## Przygotowanie podpisanego wydania

Do przygotowania wykazu służy narzędzie tools/przygotuj_wydanie.py. Przykładowa kolejność dla osoby wydającej system:

1. Wygenerować parę kluczy na zaufanym komputerze, poza repozytorium VGK:

       python3 tools/przygotuj_klucz_zaufania.py --private-key vgk-klucz-prywatny.pem --public-key vgk-klucz-publiczny.pem --header vgk-klucz-publiczny.h

   Narzędzie tworzy klucz prywatny z uprawnieniami ograniczonymi do właściciela, klucz publiczny i nagłówek C zawierający wyłącznie klucz publiczny. Nie nadpisuje istniejących plików.

2. Zachować klucz prywatny w bezpiecznym miejscu, najlepiej w dwóch kopiach offline. Nie przesyłać go do GitHub, nie umieszczać w ISO ani w artefaktach budowania. Nie wysyłać go do pomocy technicznej ani do rozmowy.
3. Klucz publiczny i nagłówek zawierający wyłącznie klucz publiczny można przekazać do projektu w celu osadzenia klucza w mechanizmie uruchamiania. Przed użyciem trzeba niezależnie potwierdzić, że klucz jest właściwy.
4. Utworzyć wykaz i podpis, wskazując wszystkie pliki wydania, które mają być chronione, na przykład:

       python3 tools/przygotuj_wydanie.py --root katalog-wydania --release VGK-0.2 --private-key vgk-klucz-prywatny.pem --manifest katalog-wydania/VGKM --signature katalog-wydania/VGKM.sig boot/BOOTX64.EFI boot/vgk-kernel.elf

5. Sprawdzić wynik narzędziem tools/sprawdz_wydanie.py, podając ten sam katalog, wykaz, podpis i odpowiadający klucz publiczny.

Program przygotowujący wykaz nie nadpisuje istniejącego wykazu ani podpisu. Należy wybrać nowe ścieżki wyjściowe dla kolejnego wydania.

**Uwaga:** produkcyjny klucz publiczny wydawcy nie jest jeszcze osadzony w zwykłym obrazie UEFI. W GitHub Actions budowany jest osobny obraz testowy z losowym kluczem tymczasowym, do którego podpisywane jest wydanie testowe. Test potwierdza ścieżkę weryfikacji w emulatorze, ale nie ustanawia zaufania dla oficjalnych wydań.

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

Na obecnym etapie natywna ścieżka UEFI VGK sprawdza podpis Ed25519 wykazu VGKM i skróty SHA-256 plików, waliduje ELF64, ładuje jądro i przekazuje mu sterowanie. Przebiegi GitHub Actions 830 i 831 z 11.11.2026 potwierdziły uruchomienie jądra diagnostycznego w QEMU/OVMF. Osobny test zmodyfikował jeden bajt jądra i potwierdził odmowę startu. Jest to nadal eksperymentalna ścieżka UEFI, nie pełna ochrona całego systemu ani test na fizycznym komputerze.


## Sprawdzanie integralności plików w języku C

Dodano moduł:
- `boot/crypto/integrity.h` – wspólny interfejs i kody wyniku;
- `boot/crypto/integrity.c` – sprawdzanie skrótu całego bufora lub pliku czytanego porcjami;
- `tests/test_integrity.c` – testy poprawnego pliku, zmienionej zawartości, błędnego oczekiwanego skrótu, nieprawidłowego argumentu, krótkich odczytów i błędu odczytu.

Porównanie 32-bajtowych skrótów wykonuje się bez kończenia pętli po pierwszej różnicy. Odczyt pliku odbywa się porcjami, z kontrolą rozmiaru i błędów źródła danych. Testy są uruchamiane przez `make integrity-test`, a także jako część `make security-test`.

Moduł nie otwiera samodzielnie plików i nie ufa nazwie pliku: otrzymuje funkcję odczytu od przyszłego programu uruchamiającego. Nie sprawdza podpisu wydawcy, dopóki zaufany skrót nie zostanie dostarczony z wiarygodnego, podpisanego wykazu. Nie jest jeszcze podłączony do UEFI ani jądra VGK.

## Weryfikacja podpisów Ed25519 w języku C

Dodano interfejs VGK:
- `boot/crypto/ed25519.h` – publiczny interfejs weryfikacji;
- `boot/crypto/ed25519.c` – połączenie interfejsu VGK z biblioteką kryptograficzną;
- `tests/test_ed25519.c` – testy wektora Ed25519 z dokumentu RFC 8032 oraz przypadków błędnych.

Do obliczeń wykorzystano istniejącą bibliotekę Monocypher 4.0.3, a nie nowy, pisany od zera algorytm. Zachowano jej pliki źródłowe i licencję w `boot/crypto/monocypher/`; pochodzenie opisuje `boot/crypto/monocypher/POCHODZENIE.md`. Biblioteka jest udostępniana na licencji BSD-2-Clause lub CC0-1.0, zgodnie z zachowanymi nagłówkami źródłowymi.

Interfejs VGK przyjmuje klucz publiczny o długości 32 bajtów, wiadomość i podpis o długości 64 bajtów. Nie potrzebuje klucza prywatnego do sprawdzania. Zwraca osobny wynik dla poprawnego podpisu, niepoprawnego podpisu i błędnych argumentów. Testy obejmują poprawny wektor RFC 8032, zmieniony podpis, zmieniony klucz publiczny i błędne argumenty. Uruchamia je `make ed25519-test`; test jest także częścią `make security-test`.

Moduł Ed25519 jest podłączony do parsera wykazu VGKM i ładowacza UEFI. W emulatorowym teście CI sprawdzanie podpisu odbywa się przed interpretacją wykazu. Nadal brakuje zatwierdzonego, produkcyjnego klucza wydawcy, procedury jego rotacji i unieważnienia oraz szerszego testu na sprzęcie.

## Podpisany wykaz plików – parser w języku C

Dodano ścisły format binarny wykazu VGKM w wersji 1 oraz jego parser:
- `boot/crypto/manifest.h` – struktury i kody wyniku;
- `boot/crypto/manifest.c` – sprawdzenie podpisu Ed25519 przed interpretacją zawartości, kontrola formatu, limitów i ścieżek;
- `tests/test_manifest.c` – testy poprawnego wykazu, zmienionej zawartości, ścieżki wychodzącej poza katalog, pustego elementu ścieżki i nadmiarowych danych.

Wykaz zawiera nagłówek `VGKM`, wersję formatu, liczbę wpisów, a następnie wpisy z długością ścieżki, rozmiarem pliku, 32-bajtowym skrótem SHA-256 i ścieżką względną. Limit wynosi 128 wpisów, 240 bajtów na ścieżkę i 64 KiB na cały wykaz. Parser nie przydziela pamięci i zwraca odwołania do danych wejściowych.

Parser odrzuca ścieżki bezwzględne, elementy `.` i `..`, puste elementy, niedozwolone znaki, powtórzone ścieżki, niezgodną liczbę danych oraz niepoprawny podpis. Podpis obejmuje wszystkie bajty wykazu.

Test uruchamia `make manifest-test` i jest częścią `make security-test`.

Parser jest podłączony do ładowacza UEFI. Wykaz jest sprawdzany przed użyciem ścieżek, a adapter plików UEFI odczytuje wskazane pliki i porównuje ich skróty. Format VGKM pozostaje formatem projektu i może zostać zmieniony przed pierwszym oficjalnym wydaniem.

### Sprawdzanie plików według podpisanego wykazu

Parser wykazu połączono teraz z modułem SHA-256 przez funkcję `vgk_manifest_verify_files`. Program uruchamiający przekazuje jej własną funkcję otwierania pliku z nośnika oraz funkcję zamykania uchwytu. Dla każdego wpisu mechanizm:
1. pobiera bezpieczną ścieżkę z wcześniej sprawdzonego wykazu;
2. otwiera wskazany plik przez funkcję dostarczoną przez obsługę systemu plików;
3. odczytuje go porcjami i oblicza SHA-256;
4. porównuje wynik z podpisanym wykazem;
5. zamyka plik również po wykryciu błędu.

Testy sprawdzają zgodny plik, podmienioną zawartość, błędny podpis i zamknięcie pliku po kontroli. Adapter jest używany przez ładowacz UEFI do odczytu plików z rzeczywistego woluminu startowego. Produkcyjny klucz publiczny wydawcy pozostaje do ustalenia; test emulatorowy używa wyłącznie klucza tymczasowego.


## Podstawa dostępu do plików przez UEFI

Dodano `boot/uefi/uefi_file.h` z deklaracjami interfejsu systemu plików UEFI oraz obsługi pliku. Uwzględnia ono układ struktur, kolejność funkcji, tryby otwierania i kontrole zgodności wykonywane podczas kompilacji. Test `make uefi-abi-test` sprawdza rozmiary struktur, położenia funkcji i stałe trybów.

Interfejs jest używany przez ładowacz UEFI: pobiera on protokół woluminu startowego, otwiera katalog główny i czyta wykaz, podpis oraz plik jądra. Odczyt jest ograniczony rozmiarem, obsługuje krótkie odczyty i odrzuca plik większy od zadeklarowanego.


## Przygotowanie klucza zaufania wydawcy

Dodano narzędzie tools/przygotuj_klucz_zaufania.py. Tworzy ono parę Ed25519 poza repozytorium, ogranicza uprawnienia pliku prywatnego do właściciela i przygotowuje nagłówek C zawierający wyłącznie 32-bajtowy klucz publiczny. Narzędzie odmawia nadpisania istniejących plików. Testy tests/test_klucz_zaufania.py sprawdzają uprawnienia, zgodność klucza w nagłówku z kluczem PEM oraz odmowę nadpisania.

Klucz prywatny jest własnością wydawcy i nigdy nie może trafić do repozytorium, obrazu ISO, artefaktów budowania ani rozmowy. Do osadzenia w programie startowym wystarczy klucz publiczny. Nie wygenerowano w repozytorium klucza produkcyjnego ani nie wpisano fikcyjnego klucza. Do czasu otrzymania i niezależnego potwierdzenia klucza publicznego rzeczywista weryfikacja podczas startu pozostaje wyłączona.


## Połączenie weryfikacji z odczytem UEFI – aktualny etap

Ładowacz UEFI wykorzystuje `boot/uefi/uefi_release_verify.c` do:
- odczytania podpisanego wykazu VGKM i podpisu z woluminu startowego;
- sprawdzenia podpisu Ed25519 przed użyciem wpisów;
- sprawdzenia rozmiaru i SHA-256 plików wymienionych w wykazie;
- odrzucenia wydania przy błędnym podpisie, brakującym pliku lub niezgodnej zawartości.

Następnie ładowacz ponownie odczytuje jądro do bufora, sprawdza jego skrót, waliduje ELF64, przydziela dokładne zakresy pamięci, kopiuje segmenty i zeruje BSS. Po przygotowaniu mapy pamięci wypełnia `VGK_BOOT_INFO`, kończy usługi startowe UEFI i przekazuje sterowanie do jądra.

GitHub Actions buduje osobny obraz testowy z tymczasowym kluczem Ed25519 i uruchamia go w QEMU/OVMF. Przebiegi 830 i 831 z 11.11.2026 potwierdziły komunikaty jądra, poprawność protokołu startowego, dostępność zarezerwowanej puli stron, przygotowanie nieaktywnych mapowań i uruchomienie menedżera pamięci. Test podmienionego jądra potwierdził odmowę startu przy niezgodnym skrócie. Emulator nie zastępuje testu na fizycznym komputerze.

**Ograniczenia:** zwykły obraz UEFI nie zawiera produkcyjnego klucza publicznego i odmawia startu, jeśli klucza zaufania brakuje. Test CI nie jest wydaniem produkcyjnym, nie potwierdza działania na fizycznym komputerze i nie oznacza jeszcze ochrony sterowników, systemu plików ani kodu działającego już w pamięci.


Kontrola integralności sprawdza nie tylko SHA-256, lecz także dokładny rozmiar pliku. Po odczytaniu zadeklarowanej liczby bajtów wykonuje próbny odczyt jednego dodatkowego bajtu; jeśli istnieje, plik jest odrzucany. Test obejmuje próbę dopisania danych na końcu.
