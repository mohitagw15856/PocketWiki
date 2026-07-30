import os

from pocketwiki.cli import main


def _write_articles(tmp_path):
    (tmp_path / "isaac-newton.md").write_text(
        "---\ntitle: Isaac Newton\n---\n\n## Life\n\nHe described [Gravity](gravity.md).\n",
        encoding="utf-8",
    )
    (tmp_path / "gravity.md").write_text(
        "---\ntitle: Gravity\n---\n\n*A* fundamental **force**.\n", encoding="utf-8"
    )


def test_build_and_inspect(tmp_path, capsys):
    _write_articles(tmp_path)
    out = tmp_path / "sample.pwa"
    rc = main(["build", "markdown", str(tmp_path), "-o", str(out), "-n", "Demo"])
    assert rc == 0
    assert out.exists()
    captured = capsys.readouterr()
    assert "Articles:          2" in captured.out

    rc = main(["inspect", str(out), "--list", "--search", "isa"])
    assert rc == 0
    captured = capsys.readouterr()
    assert "Isaac Newton" in captured.out
    assert "Demo" in captured.out


def test_build_strict_over_budget(tmp_path):
    _write_articles(tmp_path)
    out = tmp_path / "sample.pwa"
    rc = main(
        ["build", "markdown", str(tmp_path), "-o", str(out), "--index-budget", "1", "--strict"]
    )
    assert rc == 2


def test_bad_archive(tmp_path, capsys):
    bad = tmp_path / "bad.pwa"
    bad.write_bytes(b"NOPE" + b"\x00" * 60)
    rc = main(["inspect", str(bad)])
    assert rc == 1
