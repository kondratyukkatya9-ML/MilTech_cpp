# Preps

Мінімум щоб devcontainer у цьому проєкті запрацював.

## Кроки

1. **Поставити Docker** - див. [linux.md](linux.md) / [windows.md](windows.md) / [macos.md](macos.md) для своєї ОС.
2. **Склонувати репо курсу** (гілка `block-2-lesson-1`). Команди в тому ж OS-файлі, секція "Як взяти код курсу". На Windows - клон йде **в WSL**, не на `C:\`.
3. **Відкрити у VS Code**: `code .` з Ubuntu-терміналу після клону (не з Windows Explorer - `devcontainer.json` цього проекту зламається без WSL-сесії).
4. **Встановити Dev Containers extension**: VS Code запропонує сам (із `.vscode/extensions.json`). Якщо ні - `Ctrl+Shift+X` (на macOS `Cmd+Shift+X`), пошук `Dev Containers`, Install.
5. **Reopen in Container**: VS Code запропонує - погодитись. Перший білд 3-5 хв.

## Якщо вже клонували репо раніше

Якщо репо клоновано до нових оновлень (фікси для devcontainer) - підтягнути зміни. В Ubuntu/shell-терміналі:

```bash
cd ~/projects/C-PLUS-PLUS-FOR-MILITARY-TECHNOLOGY
git pull
```

Якщо `cd` каже "No such file or directory" - перевірити куди було клоновано раніше (за інструкцією мало бути `~/projects/`, але могло бути інакше).

Потім у VS Code: Command Palette (Ctrl+Shift+P, на macOS Cmd+Shift+P) -> `Dev Containers: Rebuild Container`. Саме Rebuild, не Reopen - треба перебудувати образ, інакше фікси в Dockerfile не підхопляться.

## Альтернативні редактори

Не VS Code? Див. [devcontainers-cli.md](devcontainers-cli.md) - Neovim, JetBrains, Zed, Helix, Sublime, Emacs.

## Перевірка

Всередині контейнера (VS Code terminal):

```bash
cmake -S . -B build
cmake --build build
./build/app
```

Має вивести `Hello, section 2!`. Якщо не працює - в чат курсу.
