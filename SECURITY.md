# Security and integrity

The builder accepts only the two documented `3G.EXE` hashes, the shared
original WinG hash, and the edition-specific sound-library hashes. It verifies
the supplied patch components before work and the complete generated edition
before reporting success.

The output directory must not already exist or be inside the source tree. On
failure, cleanup is limited to a new partial output directory created by the
current run.

Do not submit proprietary game files in an issue. Hashes, file sizes, logs
produced by a diagnostic build, and reproduction steps are sufficient.

