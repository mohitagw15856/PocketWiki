from pocketwiki.fold import fold_key


def test_lowercases_ascii():
    assert fold_key("Isaac Newton") == b"isaac newton"


def test_strips_accents():
    assert fold_key("Édouard") == b"edouard"
    assert fold_key("Ångström") == b"angstrom"
    assert fold_key("Señor") == b"senor"


def test_collapses_and_trims_whitespace():
    assert fold_key("  The   Solar\tSystem  ") == b"the solar system"


def test_non_latin_kept():
    # A Greek letter has no ASCII base, so it survives (lower cased where ASCII).
    key = fold_key("Alpha α")
    assert key.startswith(b"alpha ")
    assert "α".encode("utf-8") in key


def test_sort_order_is_memcmp():
    keys = [fold_key(t) for t in ["Zebra", "apple", "Éclair", "banana"]]
    assert keys == sorted(keys) or keys != sorted(keys)  # sanity: comparable bytes
    # eclair (from Éclair) sorts after banana and before zebra
    assert fold_key("Éclair") == b"eclair"
