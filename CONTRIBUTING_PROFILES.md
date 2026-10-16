# Physical USB profiles

Physical profiles are declarative JSON files in `src/plasmoid/profiles/`.
To add one:

1. Copy a similar profile JSON and choose a unique ID.
2. Describe the root hardware and only relationships supported by evidence.
3. Do not assume that every child of a hub is integrated.
4. Run `python3 tools/validate_profiles.py`.
5. Submit the new JSON file in a pull request.

No C++ or CMake file-list edit is needed. Profile files are discovered and
bundled automatically during configuration.

“Not matched” does not mean “external”. An unknown relationship remains
unclassified. Profiles should avoid retail-product names unless their
structural evidence genuinely identifies that product.

The profile format is versioned and intentionally bounded: it supports the
matcher’s existing VID/PID, companion, direct-child, logical-port,
manufacturer, product database, interface-class, and container-presence
constraints. It does not execute code or expressions.
