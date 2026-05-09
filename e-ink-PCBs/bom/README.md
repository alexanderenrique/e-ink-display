# Bill of materials

CSV summaries here mirror the KiCad schematics in each PCB variant. **Regenerate
from KiCad** when you change symbols or values so MPNs and references stay
accurate:

1. Open `e-ink_PCB.kicad_sch` in KiCad for the variant you care about.
2. **Tools → Generate Bill of Materials** (pick fields you want, e.g.
   Reference, Value, Footprint, MPN).
3. Export CSV and replace or supplement the files in this directory.

These CSVs intentionally omit power symbols (`#PWR*`) and decorative
schematic-only items.
