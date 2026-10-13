/* #243 fixture: the ONLY named CALL-semantic node here is the syntax-only
 * `offsetof` keyword token. The one call node a class selector can match,
 * `offsetof_expression`, carries no name -- so `.call#anything` can never match
 * and must RAISE rather than return a silent empty (#89). */
struct S { int a; int b; };
unsigned long f(void) { return offsetof(struct S, b); }
