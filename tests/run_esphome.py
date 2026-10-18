import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "jura-extended/.schema-env"))
from esphome.__main__ import main
if __name__ == "__main__":
    sys.exit(main())
