"""Keep extracted ports as immutable standalone-repository pointers."""
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
POINTERS = {
    "fheroes2": (
        "https://github.com/DalsinAI/amigafheroes2.git",
        "dff6f8b08f2436b9093fba8ccb2b5293cdb65468",
    ),
    "openloco": (
        "https://github.com/DalsinAI/amigaopenloco.git",
        "0e1c1835b7704b0412e5c1673fb31d31228144a8",
    ),
}


class StandalonePortPointerTests(unittest.TestCase):
    def test_pointers_are_minimal_and_immutable(self):
        for name, (url, commit) in POINTERS.items():
            with self.subTest(port=name):
                port = ROOT / "ports" / name
                self.assertEqual(
                    sorted(path.name for path in port.iterdir()),
                    ["README.md", "source.json"],
                )
                source = json.loads((port / "source.json").read_text())
                self.assertEqual(source["url"], url)
                self.assertEqual(source["commit"], commit)
                self.assertEqual(len(source["commit"]), 40)


if __name__ == "__main__":
    unittest.main()
