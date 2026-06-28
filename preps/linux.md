# Linux

Docker Engine через офіційний скрипт:

```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker "$USER"
```

Вийти й зайти в сесію заново (або reboot) - нова група `docker` підхопиться.

На Ubuntu/Debian/Fedora daemon стартує сам через systemd. Якщо ні:

```bash
sudo systemctl start docker
sudo systemctl enable docker
```

Перевірка:

```bash
docker run hello-world
```

## Як взяти код курсу

```bash
sudo apt update && sudo apt install -y git   # або еквівалент вашого пакетного менеджера

git config --global user.name "Your Name"
git config --global user.email "you@example.com"

mkdir -p ~/projects
cd ~/projects
git clone --branch block-2-lesson-1 https://github.com/robot-dreams-code/C-PLUS-PLUS-FOR-MILITARY-TECHNOLOGY.git
cd C-PLUS-PLUS-FOR-MILITARY-TECHNOLOGY
code .
```

VS Code запропонує встановити Dev Containers extension (із `.vscode/extensions.json`) - погодитись. Якщо промпт не зявився: `Ctrl+Shift+X`, пошук `Dev Containers`, Install.

Далі VS Code запропонує `Reopen in Container` - теж погодитись.
