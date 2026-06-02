# AGENTS.md

## Thinking

- Do not blindly agree with the user's claims. Give feedback based on objective evidence from the repository, tests, and command output.
- If an answer includes mathematical notation, also explain the formula in plain language.

## Workflow

- Use `rg` or `rg --files` first when searching the repository.
- Use subagents for independent work units when that will reduce risk or speed up review.
- Do not revert user changes unless the user explicitly asks for that.
- Keep build outputs out of commits. In particular, do not stage `cpp/build-coreml/`.

## Documentation

- Add comments for every new function. Each function comment should explain purpose, parameters, and return value.
- Keep local/developer-only instructions in this file or a clearly named script section in `README.md`.

## Testing

- Add and run unit tests and e2e tests for behavior changes.
- If a test cannot be run, document the blocker and the command that would verify the behavior.

## CoreML Binary Build

Use the local CoreML build script from the repository root:

```sh
cpp/xcode/coreml-build.sh
```

Common commands:

```sh
cpp/xcode/coreml-build.sh --clean
cpp/xcode/coreml-build.sh --jobs 8
cpp/xcode/coreml-build.sh --install
cpp/xcode/coreml-build.sh --install --install-path "$HOME/.katago/katago"
cpp/xcode/coreml-build.sh --smoke-gtp
```

The default build directory is `cpp/build-coreml`, and the default install path is `$HOME/.katago/katago`.

The script uses CMake with:

```text
USE_BACKEND=METAL
BUILD_DISTRIBUTED=0
NO_GIT_REVISION=1
```

`--smoke-gtp` uses `$HOME/.katago/coreml_gtp.cfg` and `$HOME/.katago/model.bin.gz` by default. Override them with `--config`, `--model`, `KATAGO_CONFIG`, or `KATAGO_MODEL`.
