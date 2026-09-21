#pragma once

// Headless check (no terminal needed) that every data source vitals relies on
// is readable and returns plausible values. Prints one line per component:
//   <name>  OK | SKIP | FAIL  [reason]
// SKIP is for optional hardware that isn't present (GPU, thermal sensors).
// Returns 0 if nothing failed, 1 otherwise.
int run_self_test();
