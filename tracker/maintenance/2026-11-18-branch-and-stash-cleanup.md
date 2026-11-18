# Branch and stash cleanup, 2026-11-18

Every branch below was an ancestor of `origin/main` at `41a21f6` when deleted,
so its commits remain in main's history. To restore any one:

    git push origin <sha>:refs/heads/<name>

Excluded and NOT deleted: `main`; `docs/064-065-tracker-issue-refs` (open PR #348);
and `v2.x` — fully subsumed (0 commits main lacks) but kept as a line marker.

| branch | tip sha |
|---|---|
| `add-language-support` | `e618f47948f0e8aced872160883e6f9c7ee7ea4d` |
| `audit/name-extraction-strategy-mismatch` | `105a12e9199aa527a5325f5cf86b8635666ec60e` |
| `chore/bump-duckdb-v1.5.6` | `aee09a51b536cd5ffe8b0663b2b0419483de0bac` |
| `chore/v2-deparser-drift-and-tooling` | `93c3c6d1c0139377976cde22ad977367beb73561` |
| `ci/defang-v2-canary` | `ec2ea4c5ff2adb990db84ddefd4ad4dd6b762af9` |
| `ci/duckdb-pin-registry-quad-and-v2-coverage` | `6a5acf8756f39d9d60021d57197335ed24aa9d3f` |
| `ci/enable-windows-builds` | `715e08e30820a04892460cad2648b107af39e452` |
| `ci/pin-equals-shipped-tag` | `e0e623f3e72e5c8c68cce36b51deeb020f763630` |
| `ci/raise-v2-test-budget` | `6974c3db747eff7b7c17018004e75069bcbba89b` |
| `copilot/fix-github-actions-job` | `c887062bcaae082c2200e61c80829fca34ec3f65` |
| `docs/044-baseline-after-232` | `7d4857a56ee06ffb143cbf3eb8ca95dd4f0460e6` |
| `docs/049-language-parity` | `e5cd49dfe7f6f3ef70a9df36a2d166f3c16cf0b0` |
| `docs/050-issue-closeout-plan` | `435ebeb72a7b6d430edaaf0c2b38ee2dae84e8f9` |
| `docs/051-duckdb-2.0-migration-plan` | `f0fb9515d4f362c7ce443e5f99addcaf89bb69b5` |
| `docs/051-rung-measurements` | `252c495726a8b50ce1a356e4b0a967230f36447c` |
| `docs/26036-remeasure` | `6ebefa247352b91980a8cacb63288af4bdd1c172` |
| `docs/abi-crossrefs-and-043` | `5499b7150b73fd54d226a87f177e7e1cc9911cce` |
| `docs/custom-languages-howto-and-math` | `7ba352149be38cee6ba3ff05e09ceb28ea822839` |
| `docs/m2-core-still-owed` | `4dc9795cafb627d279ce39649c54e0e5152b2a93` |
| `docs/tracker-040-043-053` | `1f13bb42753ae280d107d5d6a7a3e48b79cd48ba` |
| `docs/tracker-stale-lines` | `1b12c8bf93a7bc50ae116f27c93aa481b1410090` |
| `docs/v1.16.0-release-note` | `2ba4cbed8bf490fe79a68b9aa33c2ef367af9ce9` |
| `docs/v1160-verification-refresh` | `5458a588f9577d27f28172e3f276a2380089fa64` |
| `docs/v2-roadmap-explicit` | `8103e92bb811a43c5457c456751ca074d0ef09cd` |
| `feat/042-gate-unreachable-specializations` | `8ba761a0279e024a7215ef4b6b612425bf6a3061` |
| `feat/047-gate-def-type-tables` | `a34130eecc15fbb1055dd5a543af51ba960cfde2` |
| `feat/047-sql-keyword-leaf-text` | `9bc100fa50ae90644ef4706745afd3aa7461ac14` |
| `feat/051-duckdb-2.0-pin` | `84f4f14f66e04d704491b47a763b65b8b77d25fd` |
| `feat/054-m2-stage0-layout-invariance` | `979116140944c17ac7f62c9696e0ca29a403e2dd` |
| `feat/054-m2-stage1-languages-layout` | `a6e8d1687087874c8ab97697fc3cda8981178ccb` |
| `feat/170-macro-descriptions` | `09008c97a7d2a9d8fa46ac03903a7f773d12eca5` |
| `feat/188-call-sites-and-alias-table` | `6d61710dabb672f46c92f17c0be3b425e265ec99` |
| `feat/227-wire-up-yaml` | `b53f95ff8bf4259ef8362cecce7b371ed2febbe7` |
| `feat/232-unreachable-declaration-audit` | `a8a3e550acda02eb43fa7092d68d61447f9fcc6f` |
| `feat/99-entity-resolution` | `68ddeb467254baa6a15348b85ad8144a4bdea677` |
| `feat/ast-selector-for` | `5cf048e8c60309ba0204ba65fb059facc1b58af1` |
| `feat/duck-blocks-1.2` | `62b7d9c66918597d054e9c57c7f37f83b162882e` |
| `feat/duck-blocks-1.3-sync` | `0a8483035eb4289f2b6160f7fcbb8e038d130d4a` |
| `feat/grammar-ts-using-conflict` | `6e9acf3bf5eaf8d7799c028d3b490d59db48954e` |
| `feat/jsx-node-types` | `b8408a8c5795f36bf02803f1f3657468df9b15e3` |
| `feat/m2-stage3-macro-split` | `691de8befd944a74361a3523840db0088940574d` |
| `feat/m2-stage4-core-directory` | `86c3877d7f9e57a52c32e968cce21faa6636cf18` |
| `feat/peer-review-functions` | `ef6e469703b99fa1ca3b01f374abaafb1ef1d790` |
| `feat/selector-file-line-attrs` | `f072d268f2ab88daa5101120151bc7d13b383c7f` |
| `feat/unparse-rules-engine` | `258079ce8bd55ed1f9d00647658729bb711a17d9` |
| `feature/044-m1-module-abi` | `aaeeeb24c6e4749586f43ba364e459e2b0ab3508` |
| `feature/add-dart-language` | `9f491c5d390404e1779ef8b09384627e54828225` |
| `feature/add-graphql-language` | `22831984342b22ba6505261cefee0e1027ce9c9f` |
| `feature/add-hcl-language` | `0760066a1242b5c10add6108db134f0561d655f0` |
| `feature/add-zig-language` | `a8718167c74a1f4f2e286d48047c056bd96769a9` |
| `feature/better-rust-ruby-r-natives` | `c787868c3c028eb20e146a971591882abb56e0a3` |
| `feature/configurable-extraction` | `f1883cf2450eb27f91ce7c179985d120d3586b78` |
| `feature/glob-pattern-support` | `451b35fa477f28e40a2db63c1d61357b3d14b54e` |
| `feature/grammar-adapter-refactor` | `75dd4643152334720a525ea52529cbdcc9bd49a8` |
| `feature/language-expansion` | `2f2faf886b1658cb1d1ee3f88c98084ef5b4b4bf` |
| `feature/streaming-results` | `3b1fcb44a35d23cb5036417112d1fe4972e826b5` |
| `feat/v2-family-c-named-parameters` | `302a738d694286614fceceaf546d5180dd5c950c` |
| `fix/047-leaf-text-nonkeyword-gaps` | `e1363e08e8a379692bb9f001c7fd9efd43e5fea5` |
| `fix/061-duckdb-adapter-file-path` | `54a1b0989d79ff7a8a296106056ee704e4233b12` |
| `fix/148-exported-includes-methods-and-nested-functions` | `3498c54e4b6eb6c5ef3117123038f28fc9ef3466` |
| `fix/184-zero-arity-pseudo-class-args` | `6e6e943b9985465dd082d9255223e011c4fff1f1` |
| `fix/196-alias-list-gate` | `17cbe2fb0cff7ee848df142d683fbbb573941429` |
| `fix/196-delete-dead-taxonomy` | `5a88a5d8b3ca209cc8594abb508d239d4596d283` |
| `fix/222-part2-resolve-shadowed` | `1aaeb295d975028c8b8258778d92815d2003bd28` |
| `fix/222-shadowed-typescript-decls` | `6d1d99a35ce65ca8b765c24d8b5eb46618b58091` |
| `fix/231-keyword-operator-classification` | `a896d1f03f29b9186185d49d17397ef7f78b5b59` |
| `fix/235-language-reference-generator` | `5ab6ba5f0d8cb076e6204dfa815d7f11a6456824` |
| `fix/243-semantic-attribute-guard` | `ad4865a9227dee4294b0e7539a106fda2dc4f86a` |
| `fix/244-register-semantic-type` | `2ca5c7c4c77018a9a6fb87118207ebe8b62117c3` |
| `fix/261-ast-select-peek` | `aae8453318f2b1173c24e11edce9d3dcb392ac1e` |
| `fix/279-match-capture-binding` | `8f03d2f91b8b9c822f0734135814e363f069a14e` |
| `fix/289-for-tuple-targets` | `9dbb7c383c5df5bfce4d8054099b90d05c918042` |
| `fix/319-320-nested-parameter-names` | `d5eeded942c87fb68609b77475ee4b47c8d90496` |
| `fix/64-catch-vars` | `747a22fe02bd9137a4289196d066142146348953` |
| `fix/64-for-targets` | `f68dee598706339dd455141a0e0c1af92a107392` |
| `fix/64-phase3-languages` | `6745b9cade60bceedd77d0e2bdac0a17c1442f58` |
| `fix/access-alias-and-coverage` | `4cd5b1d6ce0170a83bdb0da2aa3b03af4decc964` |
| `fix/cyanoptera-parser-ctor` | `0c49b2c8a405a4d21a26d9e9cc348c08276bfcfe` |
| `fix/phase2-name-binding` | `87125a0e4fdf679d30a4db7605750c648f3fbdfc` |
| `fix/repro-gate-cli-in-worktree` | `0afdd4f61e5a68883791f636e84f3e22b5bc8c37` |
| `fix-segfault-class-name-filter` | `5a76d84b32fa6b397f18e8d2cec4729e44ce82ca` |
| `fix/v1180-notes-tracker-refs` | `54b07c2170eef20bf0a7609872159f13d3ac46a1` |
| `fix/v2-function-documentation-test` | `347523a5f06864b9e8a2cbb23db24360d86b3864` |
| `fix/v2-lambda-arrow-in-220-tests` | `ceb9f8fe612cd315b9015032724eee0e321bb3c4` |
| `fix/wire-semantic-type-codes` | `f186f1beea738b5561c7d6a1d041f0b2ac9bb5c9` |
| `flat-ast-structure` | `a0d85937af1ebc7780a5735abaae7684212238ea` |
| `native-fixes` | `0ccd1db4e5bf3537f0d1734b5ab5544022f96934` |
| `parallel-processing-v2` | `5f165f7c3b50c31bfe746bcb88c9983b000c01b1` |
| `pattern-matching-prototype` | `c2dcee41788bdad65cb028a7f776ea77eed8ad5c` |
| `perf/decorrelate-pseudo-classes` | `64b3cd8a42fd8f70dfa7d941397fd55a630120d0` |
| `read-multiple-globs` | `02a65e0f4f166a77b679b9c24469b4eb9b6e5506` |
| `release/v1.16.0-date` | `44f32cde7099390a70759c0d2ace6322b9605a1a` |
| `struct-implementation` | `c2a926344e40bb167a9feea051a59a78fd721e5a` |
| `structured-extraction-redesign` | `2c1acf52c3bf88772c14974911fdbe99da00fdfe` |
| `tracker/v2-roadmap-entries` | `652af7f92c5be6e1e98013f9b18c5c9ba9a90455` |
| `unified-backend-update-functions` | `5f4160833e8a48d7cbb8db213decc90b29ee6282` |
| `unified-function-architecture` | `c2a926344e40bb167a9feea051a59a78fd721e5a` |
| `worktree-agent-a34146ecb47135480` | `cbfcb79e19c4f1de2291823c8738f1ffe2583f78` |
| `worktree-agent-a43208dec0623b577` | `03671ceebb89ead351ec1f865d277523358d187a` |
| `worktree-agent-a54173d6feec281e4` | `0d425792dfb1e2e8b240dfaf8c0ffd634e79b968` |
| `worktree-agent-a7995cfa722513287` | `ea73bf8b09f4160d986866733ff0be9437aafb69` |
| `worktree-agent-a86cfe6551d2f50cd` | `a4487530c6838f2881b2b8fc7d85a989d8954272` |
| `worktree-agent-a88834a4efcb300b4` | `0103a79630ea279806668efc867518819affb19e` |
| `worktree-agent-a8e936b34c9392b69` | `40207d07e31ef9e40c73ac774f70884999e4d94f` |
| `worktree-agent-ac393a578ff330bda` | `66eb40b7b87217fa97d112f62c675e56c0a52ab3` |
| `worktree-agent-aca6e94d409b8e30e` | `c593238f954080c47dc8deacd91838fb3837959e` |
| `worktree-agent-adde087618ecff9c6` | `4d9b28374e4ca309a9f4f34c6c8f6bce38056590` |
| `worktree-agent-af4784e9b68c06d84` | `8f41de0f82b63c3de47b7a01efec1e3da7422fad` |

## Stashes

Both stashes were dropped. Neither was lost: each is kept at a permanent ref, so
`git stash apply <ref>` or `git show <ref>` still works.

| was | archived ref | sha | content | why it was safe to drop |
|---|---|---|---|---|
| `stash@{0}` (2026-08-31, on `fix/numeric-peek`) | `refs/archive/stash-0-numeric-peek` | `d5f5510` | one hunk in `API_REFERENCE.md` removing a false "numeric peek is not honoured" warning | the correction already landed on main by another route, in a fuller form (`API_REFERENCE.md:40-44`), and numeric peek is honoured — measured: `peek := 50` gives `max(length(peek)) = 50` |
| `stash@{1}` (2026-05-31, on `docs/build-instructions`) | `refs/archive/stash-1-clangd-cache` | `e21e271` | **no tracked changes at all**; its entire content is two untracked files, `.cache/clangd/compile_commands.json` and `.cache/clangd/release/compile_commands.json` | build artifacts |

`stash@{1}` is the one worth noting: `git stash show --stat` prints **nothing**
for it, because the stash holds only untracked files and that command reads the
working-tree commit alone. An empty stat is not an empty stash — the content was
in the third parent (`stash@{1}^3`).

Dropped in descending index order, because dropping `stash@{0}` first would
renumber `stash@{1}` to `stash@{0}`.

## Not done, and why

**21 worktrees are registered, and 10 merged local branches are checked out in
them**, so `git branch -d` correctly refuses those ten. Cleaning them up means
removing worktrees, and a worktree can hold files that exist nowhere else.
That is a bigger decision than branch hygiene and it is Teague's to make.
`git worktree list` is the inventory.

Local branches deleted (both merged into `origin/main`, neither checked out
anywhere): `fix/061-duckdb-adapter-file-path` (was `54a1b09`) and
`feat/m2-stage4-core-directory` (was `86c3877`).

Local `main` was 5 commits behind and is fast-forwarded to `41a21f6`.

`v2.x` was kept. It holds 0 commits that `main` lacks and no pull request has
ever targeted it, so it is fully subsumed — but the name reads as a release-line
marker, so deleting it is Teague's call, not a hygiene default.
