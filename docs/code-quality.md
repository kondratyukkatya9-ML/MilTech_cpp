# HowTo: форматування і lint для C++ коду

Команди нижче запускати у terminal всередині devcontainer-а. На host-системі
може не бути `clang-format`, `cmake-format`, `clang-tidy` або потрібної версії
CMake.

## Де лежать налаштування

| Файл | Навіщо потрібен |
| --- | --- |
| `.devcontainer/.clang-format` | Стиль автоформатування C++ source/header файлів. |
| `.devcontainer/.cmake-format.json` | Стиль автоформатування `CMakeLists.txt`. |
| `.devcontainer/.clang-tidy` | Набір static analysis checks для C++ коду. |
| `.devcontainer/devcontainer.json` | VS Code extensions, default formatter-и і clangd settings всередині контейнера. |
| `.vscode/tasks.json` | VS Code tasks для повторюваних команд з terminal. |
| `CMakeLists.txt` | Генерує `compile_commands.json` для clangd і clang-tidy. |
| `CMakePresets.json` | Дає стабільні команди `cmake --preset debug` і `cmake --build --preset debug`. |

Dockerfile копіює formatter/linter конфіги в корінь devcontainer-а (`/`).
Тому tools знаходять їх незалежно від конкретної папки проєкту всередині
контейнера.

## VS Code форматування

У devcontainer вже налаштовано formatter-и:

- C++ файли (`*.cpp`, `*.cc`, `*.cxx`, `*.hpp`, `*.h`) форматуються через
  clangd, який використовує `clang-format`;
- CMake файли форматуються через extension `cheshirekow.cmake-format`;
- `cmake-format` запускається з конфігом `/.cmake-format.json`.

Форматування поточного файлу вручну:

```text
Command Palette -> Format Document
```

Для автоформатування під час save додати `editor.formatOnSave` у вже наявні
`[cpp]` і `[cmake]` blocks у `.devcontainer/devcontainer.json`:

```jsonc
"editor.formatOnSave": true
```

Не створювати другий `[cpp]` або `[cmake]` key поруч: у JSON має лишатися
один block для кожної мови. У цьому repo `editor.defaultFormatter` для `[cpp]`
і `[cmake]` вже є. Для ввімкнення auto-format достатньо додати тільки
`editor.formatOnSave`.

Після зміни `.devcontainer/devcontainer.json` застосувати налаштування через:

```text
Command Palette -> Dev Containers: Rebuild Container
```

`clang-tidy` не варто запускати на кожен save. Він повільніший за formatter,
потребує `compile_commands.json` і краще працює як окрема перевірка перед PR.

## VS Code tasks

Поточний `.vscode/tasks.json` навмисно мінімальний: build через debug preset і
clean build directory.

Для форматування окрема task не потрібна: VS Code вже знає formatter для C++ і
CMake файлів через devcontainer settings. Використовувати `Format Document`
або `editor.formatOnSave`.

Task для `clang-tidy` активного C++ source-файлу:

```jsonc
{
  // CMake configure оновлює compile_commands.json перед clang-tidy.
  "label": "Quality: clang-tidy active file",
  "type": "shell",
  "command": "cmake --preset debug && clang-tidy \"${file}\" -p build/debug",
  "problemMatcher": [
    "$gcc"
  ],
  "presentation": {
    "reveal": "always",
    "panel": "dedicated",
    "clear": true
  }
}
```

Запуск task:

```text
Command Palette -> Tasks: Run Task -> Quality: clang-tidy active file
```

## Мінімальний цикл перед PR

```bash
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Після цього запускати форматування і lint:

```bash
find homework_04 -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' -o -name '*.hpp' -o -name '*.h' \) -print0 \
  | xargs -0 -r clang-format -i

find homework_04 \( -name 'CMakeLists.txt' -o -name '*.cmake' \) -print0 \
  | xargs -0 -r cmake-format -i --config-file=/.cmake-format.json

find homework_04 -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) -print0 \
  | xargs -0 -r clang-tidy -p build/debug
```

Остання команда запускає `clang-tidy` по всіх C++ source-файлах у вибраній
папці домашки. Header-файли (`*.hpp`, `*.h`) не передаються окремо: clang-tidy
перевіряє їх тоді, коли вони підключені з source-файлів, які є в
`compile_commands.json`.

Для іншої домашки замінити `homework_04` на потрібну папку, наприклад
`homework_05` або `homework_06`.

Форматер змінює файли. Зміни після форматування видно у VS Code Source Control.
Якщо файл не змінився, форматеру не було що переписувати. `clang-tidy` за
замовчуванням тільки друкує diagnostics і не переписує код.

## Запуск для одного файлу

Форматування одного C++ файлу:

```bash
clang-format -i homework_04/src/main.cpp
```

Форматування одного `CMakeLists.txt`:

```bash
cmake-format -i --config-file=/.cmake-format.json homework_04/CMakeLists.txt
```

Lint одного C++ source-файлу:

```bash
clang-tidy homework_04/src/main.cpp -p build/debug
```

`-p build/debug` вказує clang-tidy, де лежить `compile_commands.json`.
Без цього clang-tidy часто не бачить ті самі include paths, defines і compiler
flags, що й реальна CMake-збірка.

## Мінімальне підключення нової папки ДЗ

Для порожньої домашки достатньо мати:

```text
homework_06/CMakeLists.txt
homework_06/src/main.cpp
```

Мінімальний `homework_06/CMakeLists.txt`:

```cmake
add_executable(homework_06 src/main.cpp)

target_compile_options(
  homework_06 PRIVATE $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
```

У кореневому `CMakeLists.txt` додати:

```cmake
add_subdirectory(homework_06)
```

Після цього працює той самий цикл:

```bash
cmake --preset debug
cmake --build --preset debug
clang-format -i homework_06/src/main.cpp
cmake-format -i --config-file=/.cmake-format.json homework_06/CMakeLists.txt
clang-tidy homework_06/src/main.cpp -p build/debug
```

## Як читати clang-tidy output

Значущий рядок має такий формат:

```text
homework_06/src/main.cpp:12:7: warning: message text [check-name]
```

Важливі частини:

- `homework_06/src/main.cpp` - файл, де знайдено diagnostic.
- `12:7` - рядок і колонка.
- `warning` або `error` - рівень сигналу.
- `[check-name]` - назва clang-tidy check.

Якщо `clang-tidy` друкує тільки службові рядки на кшталт `warnings generated`,
але немає рядків з шляхом до файлу, рядком, колонкою і назвою check, тоді
потрібно дивитись повний output і команду запуску. Найчастіші причини:

- не виконано `cmake --preset debug`;
- `-p` вказує не на ту build-директорію;
- source-файл не підключено до жодного CMake target.

## Коли можна NOLINT

`NOLINT` - це точкове вимкнення конкретного clang-tidy diagnostic. Його варто
використовувати тільки після того, як warning прочитано і причина лишити код
саме таким зафіксована поруч.

Дозволені випадки:

- false positive clang-tidy, коли код коректний, а check помиляється;
- навчальний або test-код, де приклад навмисно порушує правило;
- вимога зовнішнього API, системного callback-а або C ABI;
- низькорівневий код, де альтернатива гірша для читабельності або безпеки.

Не використовувати `NOLINT`, щоб приховати:

- warning, який ще не розібрано;
- реальний bug або undefined behavior;
- warning-и відразу на весь файл без дуже сильної причини;
- style issue, який можна виправити форматером або простою зміною коду.

Краще вказувати назву check і причину:

```cpp
// NOLINTNEXTLINE(readability-magic-numbers) - Test value mirrors the assignment sample.
const auto timeout_ms = 1500;
```

Або для одного виразу в кінці рядка:

```cpp
const auto* packet = reinterpret_cast<const Packet*>(data);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast) - Wire format defines Packet layout.
```

Гірший варіант:

```cpp
const auto timeout_ms = 1500;  // NOLINT
```

Такий коментар не пояснює, який check вимкнено і чому. У C++ source/header
коментар біля `NOLINT` писати англійською, як і решту коментарів у C++ коді.

Після форматування не варто змішувати в одному commit функціональну зміну і
масове наведення стилю, якщо форматер зачепив багато вже існуючого коду.
