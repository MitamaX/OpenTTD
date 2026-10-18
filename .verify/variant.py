import json
import pathlib
import sys

VERIFY = pathlib.Path(__file__).resolve().parent
REPO = VERIFY.parent
SHADERS = REPO / "media" / "mini_ui" / "shaders"
BACKUPS = VERIFY / "variant-backups"
VARIANTS = json.loads((VERIFY / "variants.json").read_text())


def source_of(name):
    return REPO / name if "/" in name else SHADERS / name


def backup_of(name):
    return BACKUPS / (name.replace("/", "~") + ".orig")


def edited(name, text, edits):
    newline = "\r\n" if "\r\n" in text else "\n"
    for old, new in edits:
        old = old.replace("\n", newline)
        new = new.replace("\n", newline)
        if old not in text:
            sys.exit(f"pattern missing in {name}: {old[:60]}")
        text = text.replace(old, new)
    return text


def apply(name):
    BACKUPS.mkdir(exist_ok=True)
    for target, edits in VARIANTS[name].items():
        source = source_of(target)
        backup = backup_of(target)
        if not backup.exists():
            backup.write_bytes(source.read_bytes())
        if isinstance(edits, str):
            source.write_bytes((VERIFY / edits).read_bytes())
        else:
            source.write_bytes(edited(target, source.read_bytes().decode(), edits).encode())


def restore():
    if not BACKUPS.exists():
        return
    for backup in BACKUPS.glob("*.orig"):
        source_of(backup.stem.replace("~", "/")).write_bytes(backup.read_bytes())
        backup.unlink()


if __name__ == "__main__":
    for argument in sys.argv[1:]:
        if argument == "restore":
            restore()
        else:
            apply(argument)
