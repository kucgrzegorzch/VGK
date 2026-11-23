# Polityka nadrzędności Głównych założeń systemowych VGK

**Status:** zatwierdzona reguła projektowa; automatyczne egzekwowanie wszystkich opisanych zabezpieczeń jest jeszcze do wdrożenia.  
**Zakres:** wszystkie zmiany VGK, IOS, instalatora, mechanizmu uruchamiania, systemu aktualizacji, narzędzi wydawniczych i konfiguracji bezpieczeństwa.

## 1. Zasada nadrzędna

Główne założenia systemowe VGK są nadrzędnym i niezmiennym punktem odniesienia dla zwykłego rozwoju oraz aktualizacji systemu. Rozwój VGK odbywa się pod dyktando tych założeń. Nie wolno dostosowywać, osłabiać ani reinterpretować założeń po to, aby przepuścić aktualizację, która byłaby z nimi sprzeczna.

Każda aktualizacja musi być zgodna ze wszystkimi obowiązującymi punktami Głównych założeń systemowych VGK. Zgodność z częścią punktów nie wystarcza.

## 2. Nienaruszalność dokumentu założeń

1. Dokument Głównych założeń systemowych VGK jest dokumentem tylko do odczytu dla VGK, IOS, narzędzi aktualizacji i zwykłych procesów rozwojowych.
2. Aktualizacja nie może zmieniać, usuwać, zastępować, przemianowywać ani obchodzić żadnego punktu dokumentu.
3. Aktualizacja nie może zmieniać mechanizmu, który sprawdza zgodność aktualizacji z tym dokumentem, ani wyłączać tego sprawdzania.
4. Aktualizacja nie może podmienić zaufanej kopii dokumentu, jego oczekiwanego skrótu, klucza weryfikacyjnego ani reguł wymaganych zatwierdzeń.
5. Zmiana kodu lub konfiguracji, która pośrednio osłabia którekolwiek z powyższych zabezpieczeń, jest traktowana tak samo jak bezpośrednia zmiana dokumentu i powoduje odrzucenie aktualizacji.

## 3. Obowiązkowa kontrola każdej aktualizacji

Przed dopuszczeniem aktualizacji należy:

1. wskazać dokładny zakres zmian i wszystkie zmieniane składniki;
2. sprawdzić każdy punkt Głównych założeń systemowych VGK, na który zmiana może mieć wpływ;
3. udokumentować, dlaczego zmiana jest zgodna z każdym z tych punktów;
4. sprawdzić, czy aktualizacja nie zmienia dokumentu założeń ani mechanizmów jego ochrony;
5. przeanalizować kod, zależności, konfigurację, testy, wyniki CI i ryzyka regresji;
6. odrzucić aktualizację, jeśli zgodności nie można wykazać, kontrola jest niepełna albo wyniki są niejednoznaczne.

Brak dowodu zgodności nie jest zgodą. Nie wolno przyjmować, że aktualizacja jest zgodna tylko dlatego, że testy zakończyły się powodzeniem.

## 4. Dwa niezależne zatwierdzenia

Aktualizacja może zostać dopuszczona wyłącznie po spełnieniu łącznie obu warunków:

- **Ocena techniczna:** asystent wspierający rozwój VGK przeprowadza przegląd i musi wyraźnie potwierdzić zgodność aktualizacji z Głównymi założeniami oraz brak wykrytych przeszkód. W razie niezgodności, istotnej wątpliwości lub niepełnej kontroli nie udziela pozytywnej rekomendacji.
- **Zatwierdzenie właściciela:** właściciel projektu VGK osobiście zatwierdza konkretną, sprawdzoną wersję aktualizacji i składa wymagany podpis.

Warunki są łączne, a nie zamienne. Pozytywna ocena techniczna nie zastępuje podpisu właściciela. Podpis właściciela nie zastępuje wymaganej pozytywnej kontroli technicznej.

Jeżeli któregokolwiek zatwierdzenia brakuje, zostało ono odrzucone albo odnosi się do innej wersji zmian, aktualizacja nie może zostać dopuszczona ani wydana.

## 5. Wymagania dla docelowego mechanizmu aktualizacji VGK

Docelowa implementacja musi egzekwować tę politykę, a nie tylko wyświetlać ostrzeżenie. Co najmniej:

- przechowywać zaufaną, tylko do odczytu kopię Głównych założeń systemowych VGK lub jej autorytatywny skrót kryptograficzny;
- weryfikować integralność dokumentu i pochodzenie zaufanych danych kontrolnych;
- wymagać kompletnego rekordu pozytywnej kontroli technicznej dla dokładnego identyfikatora i skrótu aktualizacji;
- wymagać poprawnego podpisu właściciela dla dokładnie tej samej wersji aktualizacji;
- odrzucać aktualizację przy braku, błędzie, niezgodności lub niejednoznaczności któregokolwiek sprawdzenia;
- wykonywać kontrolę w zaufanej części procesu aktualizacji, której aktualizowany pakiet nie może samodzielnie wyłączyć ani zmienić;
- chronić klucze weryfikacyjne i mechanizm zaufania przed podmianą przez zwykłą aktualizację.

Sama flaga „tylko do odczytu”, wpis w README lub test CI nie stanowią wystarczającego zabezpieczenia. Potrzebna jest weryfikacja kryptograficzna i egzekwowanie reguł przez zaufany mechanizm poza kontrolą aktualizowanego pakietu.

## 6. Zachowanie przy odmowie

Jeżeli aktualizacja narusza choć jedno Główne założenie systemowe VGK, próbuje osłabić ochronę dokumentu, nie przechodzi kontroli lub nie ma obu wymaganych zatwierdzeń:

- nie otrzymuje pozytywnej rekomendacji technicznej;
- nie może zostać podpisana jako zatwierdzona przez właściciela;
- nie może zostać scalona do oficjalnej gałęzi ani opublikowana jako zatwierdzone wydanie;
- mechanizm aktualizacji VGK musi ją zablokować i zachować informację o przyczynie odmowy.

Nie wolno obchodzić odmowy przez podział aktualizacji na mniejsze pakiety, zmianę nazwy, zmianę kolejności instalacji ani przeniesienie modyfikacji do IOS lub innego składnika.

## 7. Rozdzielenie reguły projektowej od wdrożenia

Od chwili przyjęcia tej polityki obowiązuje ona przy wszystkich przeglądach, decyzjach i pracach nad VGK prowadzonych w projekcie. Jej pełne techniczne egzekwowanie w działającym systemie jest osobnym zadaniem implementacyjnym i musi zostać zweryfikowane testami bezpieczeństwa.

Do czasu wdrożenia i sprawdzenia mechanizmu blokującego nie wolno twierdzić, że VGK automatycznie uniemożliwia instalację niezgodnej aktualizacji. W tym okresie zasada jest egzekwowana w procesie pracy nad projektem: brak pozytywnej oceny technicznej lub brak podpisu właściciela oznacza brak zgody na scalenie i wydanie.

## 8. Kryterium dopuszczenia

**Aktualizacja VGK jest dopuszczalna wtedy i tylko wtedy, gdy jest wykazana jej zgodność ze wszystkimi Głównymi założeniami systemowymi VGK, kontrola techniczna zakończyła się wyraźną pozytywną rekomendacją, a właściciel projektu podpisał dokładnie tę sprawdzoną wersję. Każdy brak lub sprzeczność oznacza odmowę.**
