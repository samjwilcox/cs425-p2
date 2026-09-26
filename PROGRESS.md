# P2 implementation status

Implemented: command line, relay registration, packet checksum/validation, pure Go-Back-N sender and receiver, real UDP/file integration, FIN linger, ten-timeout failure, Unity tests, deterministic damaged-channel integration, coverage gate, README, and measurement runner. Production functions and tests have documentation comments.

Verified locally: `make all`, `make check` (15 passing test groups), `make report` (256/256 executable source lines), direct AddressSanitizer runs with leak detection disabled, and four end-to-end UDP transfers including empty, exact-multiple, and damaged channels. The release build has no compiler warnings.

Still requires the course environment: use the course relay with `--delay 50` for 12 measured transfers and fill in the README table and numeric analysis; repeat `make leak` and `make leak-test` on Codespaces/Onyx because LeakSanitizer cannot access `/proc` here; push to the actual GitHub repository and confirm CI, then run the submission-report workflow and download its new DOCX. The starter ZIP did not include the relay file or a Git remote.
