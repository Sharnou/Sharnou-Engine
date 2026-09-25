# AVIF Texture Boundary

This directory accepts texture assets with the .avif extension only.

Sharnou Engine rejects other texture extensions at runtime. The engine uses libavif for actual AVIF decoding and uploads the decoded RGBA surface to Direct3D 11.

Do not commit PNG, JPG, JPEG, TGA or BMP texture assets here.

The repository deliberately ships without a large demo texture pack so the engine source stays small; add production .avif content under this directory when available.
