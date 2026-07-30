import os
import sys

# Make the companion package importable when tests run from the repo root or the
# companion folder without an editable install.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
