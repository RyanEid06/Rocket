# Rocket 2.0 Syntax Dictionary

Rocket 2.0 freezes the cumulative syntax documented by the Rocket 1.0, 1.1,
1.2, 1.3, and 1.8 syntax dictionaries. It introduces no new token, keyword,
declaration, statement, expression, type spelling, ownership construct, or
operator.

The normative grammar and semantics are `SPEC.md`; `CONCURRENCY.md` defines
`async fn`, `await`, and ownership/concurrency rules; `STDLIB.md` defines the
callable library surface. Contextual spellings such as `async`, `await`,
`unsafe`, `export`, `opaque`, and `callback` retain their specified contexts so
older identifiers remain compatible.

For 2.x compatibility:

- four-space indentation and newline/dedent block structure remain stable;
- function boundaries remain explicitly typed while local `let`/`var` types
  may be inferred;
- absence and recoverable failure remain `Option`, `Result`, and postfix `?`;
- native calls remain inside explicit `unsafe:` regions;
- safe concurrency remains structural `Send`/`Share`, affine handles,
  `async fn`, and prefix `await`; and
- no macro system, exception syntax, universal null, class hierarchy, or hidden
  ownership conversion is implied by the 2.0 freeze.
