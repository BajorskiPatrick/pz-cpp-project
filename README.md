# Skarbonka

Konsolowy menedżer budżetu domowego napisany w C++23.

Program zapisuje wydatki i przychody w pliku JSON, pozwala je przeglądać, edytować i usuwać
(pojedynczo albo całymi miesiącami / latami) oraz przygotowuje podsumowania: bilans, wydatki
według kategorii i saldo w kolejnych miesiącach.

```
$ skarbonka summary --month 2026-10
Podsumowanie: 2026-10

  Przychody:      5550.00
  Wydatki:        2503.00
  Saldo:          3047.00

Wydatki według kategorii:
  mieszkanie          1800.00   71.9%  ######################
  jedzenie             523.10   20.9%  ######
  transport            120.00    4.8%  #
  rozrywka              59.90    2.4%  #
```

## Funkcje

- dodawanie wydatków i przychodów (kwota, kategoria, data, opis),
- edycja transakcji (zmiana dowolnych pól: typu, kwoty, kategorii, daty, opisu),
- usuwanie transakcji po identyfikatorze albo wszystkich transakcji z danego miesiąca lub roku,
- lista transakcji w kolejności chronologicznej z filtrami `--type`, `--category`, `--month`, `--year`,
- podsumowanie miesiąca, roku lub całego okresu: przychody, wydatki, saldo, udział kategorii
  w wydatkach, a dla roku i całego okresu także saldo w poszczególnych miesiącach (gdy dane
  obejmują więcej niż jeden miesiąc),
- kategorie bez względu na wielkość liter (także polskich: `ŻYWNOŚĆ` = `żywność`) i spacje na brzegach,
- kwoty liczone w groszach (bez błędów zaokrągleń), akceptowany zapis `12.50` i `12,50`,
- kontrola poprawności danych (np. odrzucenie daty `2026-02-30`, kwota od `0.01` do `1000000000.00`)
  z czytelnymi komunikatami błędów,
- bezpieczny zapis pliku (najpierw plik tymczasowy, potem podmiana).

## Zależności

| Biblioteka | Do czego | Wersja |
|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | zapis i odczyt danych (JSON) | ≥ 3.11 |
| [Boost](https://www.boost.org/) – Program_options, String_algo | parsowanie linii poleceń i pomoc `--help`; normalizacja nazw kategorii | ≥ 1.80 |
| [GoogleTest](https://github.com/google/googletest) | testy jednostkowe (tylko przy budowaniu testów) | ≥ 1.14 |

Wszystkie biblioteki są wyszukiwane w systemie (`find_package`), a jeśli ich nie ma – CMake
pobiera je automatycznie (`FetchContent`). Przy pierwszej konfiguracji potrzebny jest więc dostęp
do internetu (archiwum Boost ma ok. 100 MB; budowane są tylko potrzebne moduły).

Wymagania narzędziowe:

- kompilator z obsługą C++23 (`std::expected`, `std::ranges::to`, `std::format` dla `<chrono>`),
  np. GCC ≥ 14 – projekt testowany na GCC 15.2 (MinGW-w64 dołączony do CLion) pod Windows 11,
- CMake ≥ 3.24, Ninja lub Make.

## Budowanie

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Testy można wyłączyć opcją `-DSKARBONKA_BUILD_TESTS=OFF`.

Powyższe polecenia (oraz `ctest` w sekcji [Testy](#testy)) wymagają, żeby `cmake`, `ninja`
i kompilator były w `PATH`. Jeśli są zainstalowane tylko razem z CLion, najprościej uruchomić je z terminala
wbudowanego w CLion albo dopisać do `PATH` katalogi `bin\cmake\win\x64\bin`, `bin\ninja\win\x64`
i `bin\mingw\bin` z katalogu instalacji CLion.

Uwaga (Windows): pliki źródłowe Boost są głęboko zagnieżdżone, dlatego katalog budowania
nie powinien mieć bardzo długiej ścieżki (limit 260 znaków). Ścieżka typu
`C:\Users\<nazwa>\Desktop\...\Project\build` jest w porządku.

W CLion wystarczy otworzyć katalog projektu (`File → Open`) – IDE samo skonfiguruje CMake.

## Użycie

```
skarbonka <polecenie> [opcje]
```

Program nie jest instalowany w systemie, więc uruchamia się go z katalogu budowania, np.
`./build/skarbonka` (Linux) albo `.\build\skarbonka.exe` (PowerShell; w CLion katalogiem
budowania jest domyślnie `cmake-build-debug`). W przykładach poniżej `skarbonka` oznacza tę ścieżkę.

| Polecenie | Opis | Opcje |
|---|---|---|
| `add` | dodaje transakcję | wymagane `--amount`, `--category`; opcjonalne `--type` (domyślnie `expense`), `--date` (domyślnie dziś), `--note` |
| `edit` | zmienia transakcję | wymagane `--id` oraz co najmniej jedna z opcji `--type`, `--amount`, `--category`, `--date`, `--note` (`--note ""` czyści opis) |
| `list` | wyświetla transakcje | filtry `--type`, `--category`, `--month`, `--year` |
| `remove` | usuwa jedną transakcję albo wszystkie z okresu | dokładnie jedna z opcji: `--id`, `--month` (wszystkie z miesiąca), `--year` (wszystkie z roku) |
| `summary` | podsumowanie | opcjonalnie `--month` albo `--year` (bez nich – cały okres) |
| `help` | pomoc | – |

Opcje ogólne: `-h/--help` (pomoc), `-f/--file <plik>` (plik z danymi, domyślnie `skarbonka.json`
w bieżącym katalogu). Pełny opis opcji wyświetla `skarbonka --help`.

Ścieżka podana w `--file` jest liczona względem **bieżącego katalogu terminala**, a nie katalogu
z plikiem `skarbonka.exe`. Jeśli pliku nie ma, polecenie `add` go utworzy, a `list`, `summary`,
`edit` i `remove` zgłoszą błąd (np. przy literówce w ścieżce).

Edycja zachowuje identyfikator transakcji, a zmienione dane są sprawdzane tak samo jak przy
dodawaniu – przy błędzie plik pozostaje bez zmian. `remove --month`/`--year` wypisuje listę
usuniętych transakcji; jeśli w podanym okresie nie ma żadnej, informuje, że nic nie usunięto. Przed
usunięciem całego okresu warto go wyświetlić, np. `skarbonka list --year 2025`.

Przykłady:

```bash
skarbonka add --amount 45.90 --category jedzenie --note "zakupy"
skarbonka add -t income -a 5200 -c pensja -d 2026-10-01
skarbonka list --month 2026-10 --type expense
skarbonka list --category jedzenie
skarbonka summary --month 2026-10
skarbonka summary --year 2026
skarbonka edit --id 3 --amount 49.90 --note "zakupy w markecie"
skarbonka edit --id 5 --type income --date 2026-10-02
skarbonka remove --id 3
skarbonka remove --month 2026-10
skarbonka remove --year 2025
```

Przykładowe dane są w katalogu `examples/` (polecenie uruchamiane z katalogu projektu):

```bash
skarbonka summary --file examples/budget.json
```

Program kończy się kodem `0` przy powodzeniu (także gdy filtr nic nie znalazł), `1` przy błędzie
(np. niepoprawna kwota lub data, nieistniejący identyfikator, brak pliku; komunikat trafia
na `stderr`) i `2` przy nieoczekiwanym błędzie wewnętrznym.

## Format pliku z danymi

```json
{
  "nextId": 2,
  "transactions": [
    {
      "amount": "5200.00",
      "category": "pensja",
      "date": "2026-09-01",
      "id": 1,
      "note": "wypłata za wrzesień",
      "type": "income"
    }
  ],
  "version": 1
}
```

Kategorie są zapisywane małymi literami (także polskimi) i bez spacji na brzegach, więc
`Jedzenie`, ` jedzenie` oraz `ŻYWNOŚĆ` i `żywność` to odpowiednio te same kategorie.

Pole `nextId` to numer, który dostanie następna transakcja – dzięki niemu identyfikatory
usuniętych transakcji nie są używane ponownie (pole jest opcjonalne; gdy go brak, program
wylicza je z istniejących identyfikatorów).

## Testy

```bash
cd build
ctest --output-on-failure
```

Testy (GoogleTest, katalog `tests/`) obejmują parsowanie kwot i dat, operacje na budżecie,
filtry i raporty, zapis/odczyt JSON, analizę argumentów linii poleceń oraz pełny przepływ
poleceń programu na pliku tymczasowym.

## Struktura projektu

```
include/skarbonka/   nagłówki (interfejs modułów)
src/                 implementacja + main.cpp
tests/               testy jednostkowe
cmake/               pobieranie zależności
examples/            przykładowy plik z danymi
```

## Autor

Patrick Bajorski
