import sys
from pathlib import Path

# Make `sli_backend` importable when pytest runs from the backend folder
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
