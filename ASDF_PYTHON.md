# How to Install asdf and Manage Python Versions

## Installation

### 1. Clone asdf

```bash
git clone https://github.com/asdf-vm/asdf.git ~/.asdf --branch v0.14.0
```

### 2. Load asdf in your shell

```bash
echo '. "$HOME/.asdf/asdf.sh"' >> ~/.bashrc && echo '. "$HOME/.asdf/completions/asdf.bash"' >> ~/.bashrc
```

Reload:

```bash
source ~/.bashrc
```

### 3. Verify

```bash
asdf --version
```

---

## Setting Up Python

### Install build dependencies (Ubuntu/WSL)

```bash
sudo apt-get install -y build-essential libssl-dev zlib1g-dev libbz2-dev libreadline-dev libsqlite3-dev wget curl llvm libncursesw5-dev xz-utils tk-dev libxml2-dev libxmlsec1-dev libffi-dev liblzma-dev
```

### Add the Python plugin

```bash
asdf plugin add python
```

---

## Managing Versions

### Install a version

```bash
asdf install python 3.12.1
```

### List available versions

```bash
asdf list all python
```

```bash
asdf list python
```

### Set a version

| Scope | Command | Stored in |
|---|---|---|
| Project | `asdf local python 3.12.1` | `.python-version` |
| Global (user) | `asdf global python 3.12.1` | `~/.tool-versions` |
| Current shell | `asdf shell python 3.12.1` | `$ASDF_PYTHON_VERSION` |

Resolution order: **shell → local → global → system**

```bash
asdf local python 3.12.1
```

```bash
asdf global python 3.12.1
```

```bash
asdf shell python 3.12.1
```

### Check active version

```bash
asdf current python
```

```bash
which python
```

### Remove a version

```bash
asdf uninstall python 3.11.9
```

---

## Using with Virtual Environments

asdf controls the Python version; `venv` isolates packages. Use both:

```bash
asdf local python 3.12.1
```

```bash
python -m venv .venv
```

```bash
source .venv/bin/activate
```

> After `pip install` of any CLI tool, run `asdf reshim python` to make the command available in PATH.

```bash
asdf reshim python
```
